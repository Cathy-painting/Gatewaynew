/**
 * @file cloud_service.c
 * @brief ESP8266 HTTP Server 云端服务 - ESP8266作为HTTP服务器，手机浏览器直接访问
 * 
 * 【方案原理】
 * ┌──────────┐    USART2     ┌──────────┐    WiFi     ┌─────────────┐
 * │  STM32   │ ←──────────→ │ ESP8266  │ ←─────────→ │ 手机浏览器   │
 * │ (主站)   │  AT指令/数据   │(HTTP Srv)│  TCP:80     │             │
 * └──────────               └──────────┘              └─────────────┘
 * 
 * 1. ESP8266 连接WiFi后，开启TCP Server（端口80）
 * 2. 手机浏览器访问 ESP8266的IP地址
 * 3. ESP8266通过USART2将HTTP请求转发给STM32
 * 4. STM32构建HTML页面（含实时数据），通过USART2发回ESP8266
 * 5. ESP8266将HTML响应发送给浏览器
 * 
 * 【AT指令流程】
 * AT+CWMODE=1                    → 设置Station模式
 * AT+CWJAP="ssid","pass"         → 连接WiFi
 * AT+CIFSR                       → 获取ESP8266的IP地址
 * AT+CIPSERVER=1,80              → 开启TCP Server，端口80
 * +IPD,<link_id>,<len>:<data>    → ESP8266收到浏览器请求，转发给STM32
 * AT+CIPSEND=<link_id>,<len>     → STM32构建响应，通过ESP8266发给浏览器
 * AT+CIPCLOSE=<link_id>          → 关闭连接
 */

#include "cloud_service.h"
#include "terminal_service.h"
#include "bsp_uart.h"
#include "log.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define CLOUD_RX_BUF_SIZE         512U
#define CLOUD_CMD_BUF_SIZE        256U
#define CLOUD_HTML_BUF_SIZE       1500U
#define CLOUD_FAIL_RECONNECT      3U

static uint8_t cloud_ready = 0U;
static uint8_t cloud_fail_count = 0U;
static char esp8266_ip[16] = {0};

static void cloud_send_cmd(const char *cmd)
{
    if (cmd == NULL) return;
    bsp_uart2_send_string(cmd);
    bsp_uart2_send_string("\r\n");
}

static uint8_t cloud_wait_response(const char *expect, uint32_t timeout_ms)
{
    uint8_t rx_buf[CLOUD_RX_BUF_SIZE];
    uint32_t start_tick = osKernelGetTickCount();

    if (expect == NULL) return 0U;

    while ((osKernelGetTickCount() - start_tick) < timeout_ms) {
        uint16_t len = bsp_uart_copy_rx_buffer(&huart2, rx_buf, sizeof(rx_buf) - 1U);
        if (len > 0U) {
            rx_buf[len] = '\0';
            if (strstr((const char *)rx_buf, expect) != NULL) {
                return 1U;
            }
            if (strstr((const char *)rx_buf, "ERROR") != NULL) {
                return 0U;
            }
        }
        osDelay(50);
    }
    return 0U;
}

static uint8_t cloud_cmd_expect(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    bsp_uart_clear_rx_buffer(&huart2);
    cloud_send_cmd(cmd);
    return cloud_wait_response(expect, timeout_ms);
}

static void cloud_get_ip(void)
{
    uint8_t rx_buf[CLOUD_RX_BUF_SIZE];
    uint32_t start_tick = osKernelGetTickCount();

    bsp_uart_clear_rx_buffer(&huart2);
    cloud_send_cmd("AT+CIFSR");

    while ((osKernelGetTickCount() - start_tick) < 3000U) {
        uint16_t len = bsp_uart_copy_rx_buffer(&huart2, rx_buf, sizeof(rx_buf) - 1U);
        if (len > 0U) {
            rx_buf[len] = '\0';
            char *p = strstr((const char *)rx_buf, "+CIFSR:STAIP,\"");
            if (p != NULL) {
                p += 14;
                char *q = strchr(p, '"');
                if (q != NULL && (size_t)(q - p) < sizeof(esp8266_ip)) {
                    size_t ip_len = (size_t)(q - p);
                    memcpy(esp8266_ip, p, ip_len);
                    esp8266_ip[ip_len] = '\0';
                    log_infof("[CLOUD] esp8266 ip=%s\r\n", esp8266_ip);
                    return;
                }
            }
        }
        osDelay(50);
    }
}

static int cloud_check_request(uint16_t *out_len)
{
    uint8_t rx_buf[CLOUD_RX_BUF_SIZE];
    uint16_t len = bsp_uart_copy_rx_buffer(&huart2, rx_buf, sizeof(rx_buf) - 1U);

    if (len == 0U) return -1;

    rx_buf[len] = '\0';

    char *ipd = strstr((const char *)rx_buf, "+IPD,");
    if (ipd == NULL) return -1;

    int link_id = -1;
    uint16_t data_len = 0;
    if (sscanf(ipd, "+IPD,%d,%hu:", &link_id, &data_len) == 2) {
    } else if (sscanf(ipd, "+IPD,%d:%hu", &link_id, &data_len) == 2) {
    } else {
        return -1;
    }

    if (out_len != NULL) *out_len = data_len;
    return link_id;
}

static void cloud_build_response(char *buf, size_t buf_size, const terminal_data_t *data)
{
    const char *mb_s = data->remote_online ? "ON" : "OFF";
    const char *cl_s = cloud_ready ? "ON" : "OFF";
    const char *mb_color = data->remote_online ? "#22c55e" : "#ef4444";
    const char *cl_color = cloud_ready ? "#22c55e" : "#ef4444";

    snprintf(buf, buf_size,
        "HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=utf-8\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GW</title><style>"
        "*{margin:0;padding:0}"
        "body{background:#0f172a;color:#e2e8f0;font:14px Arial;padding:10px}"
        "h2{text-align:center;color:#38bdf8;margin:0 0 10px}"
        ".g{display:grid;grid-template-columns:1fr 1fr;gap:6px;max-width:400px;margin:0 auto}"
        ".c{background:#1e293b;border-radius:6px;padding:8px;text-align:center;border:1px solid #334155}"
        ".l{font-size:9px;color:#94a3b8}.v{font-size:20px}.m{font-size:14px}"
        "</style></head><body><h2>Gateway</h2><div class=g>"
        "<div class=c><div class=l>本地</div><div class=v style=color:#38bdf8>%u</div></div>"
        "<div class=c><div class=l>远程</div><div class=v style=color:#22c55e>%u</div></div>"
        "<div class=c><div class=l>Modbus</div><div class=m style=color:%s>%s</div></div>"
        "<div class=c><div class=l>云端</div><div class=m style=color:%s>%s</div></div>"
        "<div class=c><div class=l>采样</div><div class=v style=color:#f59e0b>%lu</div></div>"
        "<div class=c><div class=l>成功</div><div class=v style=color:#a3e635>%lu</div></div>"
        "<div class=c><div class=l>失败</div><div class=v style=color:#ef4444>%lu</div></div>"
        "<div class=c><div class=l>上传</div><div class=v style=color:#f472b6>%lu</div></div>"
        "</div></body></html>",
        data->local_value,
        data->remote_value,
        mb_color, mb_s,
        cl_color, cl_s,
        (unsigned long)data->sample_count,
        (unsigned long)data->modbus_ok_count,
        (unsigned long)data->modbus_fail_count,
        (unsigned long)data->upload_count);
}

static void cloud_send_http_response(int link_id, const char *response)
{
    char cmd[CLOUD_CMD_BUF_SIZE];
    uint16_t resp_len = (uint16_t)strlen(response);

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%u", link_id, resp_len);
    bsp_uart_clear_rx_buffer(&huart2);
    cloud_send_cmd(cmd);

    if (!cloud_wait_response(">", 3000U)) {
        log_info("[CLOUD] CIPSEND timeout\r\n");
        goto close;
    }

    bsp_uart_clear_rx_buffer(&huart2);
    bsp_uart2_send_data((const uint8_t *)response, resp_len);

    if (!cloud_wait_response("SEND OK", 5000U)) {
        log_info("[CLOUD] HTTP send failed\r\n");
    } else {
        log_info("[CLOUD] HTTP response sent\r\n");
    }

close:
    snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%d", link_id);
    (void)cloud_cmd_expect(cmd, "OK", 2000U);
}

void cloud_service_init(void)
{
    cloud_ready = 0U;
    cloud_fail_count = 0U;
    esp8266_ip[0] = '\0';
    terminal_set_cloud_online(0U);
    bsp_uart2_rx_start();
}

uint8_t cloud_service_is_ready(void)
{
    return cloud_ready;
}

void cloud_service_publish_once(void)
{
    log_info("[CLOUD] publish_once enter\r\n");

    if (cloud_ready == 0U || cloud_fail_count >= CLOUD_FAIL_RECONNECT) {
        log_info("[CLOUD] check esp8266\r\n");

        if (!cloud_cmd_expect("AT", "OK", 1000U)) {
            cloud_fail_count++;
            log_infof("[CLOUD] AT failed count=%u\r\n", cloud_fail_count);
            return;
        }

        (void)cloud_cmd_expect("ATE0", "OK", 1000U);

        if (!cloud_cmd_expect("AT+CWMODE=1", "OK", 1000U)) {
            cloud_fail_count++;
            return;
        }

        char cmd[CLOUD_CMD_BUF_SIZE];
        snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"",
                 CLOUD_WIFI_SSID, CLOUD_WIFI_PASSWORD);
        log_info("[CLOUD] wifi connecting\r\n");
        if (!cloud_cmd_expect(cmd, "OK", 15000U)) {
            cloud_fail_count++;
            log_infof("[CLOUD] wifi failed count=%u\r\n", cloud_fail_count);
            return;
        }
        log_info("[CLOUD] wifi connected\r\n");

        cloud_get_ip();

        osDelay(500);

        if (!cloud_cmd_expect("AT+CIPMUX=1", "OK", 2000U)) {
            log_info("[CLOUD] CIPMUX failed\r\n");
            cloud_fail_count++;
            return;
        }

        if (!cloud_cmd_expect("AT+CIPSERVER=1,80", "OK", 3000U)) {
            log_info("[CLOUD] server start failed\r\n");
            cloud_fail_count++;
            return;
        }
        log_info("[CLOUD] http server started\r\n");

        cloud_ready = 1U;
        cloud_fail_count = 0U;
        terminal_set_cloud_online(1U);
    }

    uint16_t req_len = 0;
    int link_id = cloud_check_request(&req_len);

    if (link_id >= 0) {
        log_infof("[CLOUD] browser request link=%d len=%u\r\n", link_id, req_len);

        terminal_data_t data;
        terminal_get_snapshot(&data);

        char response[CLOUD_HTML_BUF_SIZE];
        cloud_build_response(response, sizeof(response), &data);

        cloud_send_http_response(link_id, response);

        terminal_inc_upload();
    }
}
