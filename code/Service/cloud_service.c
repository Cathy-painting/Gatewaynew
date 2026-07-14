#include "cloud_service.h"
#include "terminal_service.h"
#include "bsp_uart.h"
#include "log.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define CLOUD_RX_BUF_SIZE         256U
#define CLOUD_CMD_BUF_SIZE        512U
#define CLOUD_PAYLOAD_BUF_SIZE    256U
#define CLOUD_FAIL_RECONNECT      3U

static uint8_t cloud_ready = 0U;
static uint8_t cloud_fail_count = 0U;

static void cloud_send_cmd(const char *cmd)
{
    if (cmd == NULL) {
        return;
    }

    bsp_uart2_send_string(cmd);
    bsp_uart2_send_string("\r\n");
}

static uint8_t cloud_wait_response(const char *expect, uint32_t timeout_ms)
{
    uint8_t rx_buf[CLOUD_RX_BUF_SIZE];
    uint32_t start_tick = osKernelGetTickCount();

    if (expect == NULL) {
        return 0U;
    }

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

static void cloud_escape_at_string(const char *src, char *dst, uint16_t dst_size)
{
    uint16_t out = 0U;

    if (src == NULL || dst == NULL || dst_size == 0U) {
        return;
    }

    while (*src != '\0' && out < (uint16_t)(dst_size - 1U)) {
        if ((*src == '\"' || *src == '\\') && out < (uint16_t)(dst_size - 2U)) {
            dst[out++] = '\\';
            dst[out++] = *src++;
        } else {
            dst[out++] = *src++;
        }
    }

    dst[out] = '\0';
}

static uint8_t cloud_connect_wifi(void)
{
    char cmd[CLOUD_CMD_BUF_SIZE];

    log_info("[CLOUD] check esp8266\r\n");
    if (!cloud_cmd_expect("AT", "OK", 1000U)) {
        return 0U;
    }
    (void)cloud_cmd_expect("ATE0", "OK", 1000U);
    if (!cloud_cmd_expect("AT+CWMODE=1", "OK", 1000U)) {
        return 0U;
    }

    log_info("[CLOUD] wifi connecting\r\n");
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", CLOUD_WIFI_SSID, CLOUD_WIFI_PASSWORD);
    if (!cloud_cmd_expect(cmd, "OK", 15000U)) {
        return 0U;
    }

    log_info("[CLOUD] wifi connected\r\n");
    return 1U;
}

static uint8_t cloud_connect_mqtt(void)
{
    char cmd[CLOUD_CMD_BUF_SIZE];

    log_info("[CLOUD] mqtt connecting\r\n");
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTUSERCFG=0,1,\"%s\",\"\",\"\",0,0,\"\"",
             CLOUD_MQTT_CLIENT_ID);
    if (!cloud_cmd_expect(cmd, "OK", 2000U)) {
        return 0U;
    }

    snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%u,1", CLOUD_MQTT_HOST, CLOUD_MQTT_PORT);
    if (!cloud_cmd_expect(cmd, "OK", 5000U)) {
        return 0U;
    }

    log_info("[CLOUD] mqtt connected\r\n");
    return 1U;
}

static uint8_t cloud_publish_payload(const char *payload)
{
    char cmd[CLOUD_CMD_BUF_SIZE];
    char escaped_payload[CLOUD_PAYLOAD_BUF_SIZE * 2U];

    if (payload == NULL) {
        return 0U;
    }

    cloud_escape_at_string(payload, escaped_payload, sizeof(escaped_payload));
    snprintf(cmd, sizeof(cmd),
             "AT+MQTTPUB=0,\"%s\",\"%s\",0,0",
             CLOUD_MQTT_TOPIC,
             escaped_payload);

    return cloud_cmd_expect(cmd, "OK", 3000U);
}

void cloud_service_init(void)
{
    cloud_ready = 0U;
    cloud_fail_count = 0U;
    terminal_set_cloud_online(0U);
    bsp_uart2_rx_start();
}

uint8_t cloud_service_is_ready(void)
{
    return cloud_ready;
}

void cloud_service_publish_once(void)
{
    terminal_data_t data;
    char payload[CLOUD_PAYLOAD_BUF_SIZE];

    if (cloud_ready == 0U || cloud_fail_count >= CLOUD_FAIL_RECONNECT) {
        if (cloud_connect_wifi() && cloud_connect_mqtt()) {
            cloud_ready = 1U;
            cloud_fail_count = 0U;
            terminal_set_cloud_online(1U);
        } else {
            cloud_ready = 0U;
            terminal_set_cloud_online(0U);
            cloud_fail_count++;
            log_infof("[CLOUD] connect failed count=%u\r\n", cloud_fail_count);
            return;
        }
    }

    terminal_get_snapshot(&data);
    snprintf(payload, sizeof(payload),
             "{\"local\":%u,\"remote\":%u,\"online\":%u,\"cloud\":%u,\"sample\":%lu,\"mb_ok\":%lu,\"mb_fail\":%lu,\"upload\":%lu}",
             data.local_value,
             data.remote_value,
             data.remote_online,
             data.cloud_online,
             (unsigned long)data.sample_count,
             (unsigned long)data.modbus_ok_count,
             (unsigned long)data.modbus_fail_count,
             (unsigned long)data.upload_count);

    if (cloud_publish_payload(payload)) {
        terminal_inc_upload();
        cloud_fail_count = 0U;
        log_info("[CLOUD] publish ok\r\n");
    } else {
        cloud_fail_count++;
        log_infof("[CLOUD] publish failed count=%u\r\n", cloud_fail_count);
        if (cloud_fail_count >= CLOUD_FAIL_RECONNECT) {
            cloud_ready = 0U;
            terminal_set_cloud_online(0U);
        }
    }
}
