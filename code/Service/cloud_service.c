/**
 * @file cloud_service.c
 * @brief ESP32 JSON协议云端服务
 * 
 * ┌──────────────┐   UART2(PA2/PA3)  ┌──────────────┐   WiFi    ┌───────────┐
 * │  STM32G431   │ ←───────────────→ │    ESP32     │ ←───────→ │ 手机浏览器  │
 * │  (数据采集)   │  JSON指令/数据    │ (WebSocket)  │           │ LED控制面板 │
 * └──────────────┘                   └──────┬───────┘          └───────────┘
 *                                           │ GPIO直接控制
 *                                           │ 面包板 4路 LED
 * 
 * 协议说明：
 *   STM32 -> ESP32: {"t":"d","l":值,"r":值,"mb":0|1,"cl":0|1,"s":值,"ok":值,"f":值,"up":值}
 *   ESP32 -> STM32: {"t":"c","dev":"led","id":1,"act":"on"}
 *                    {"t":"c","dev":"led","id":1,"act":"off"}
 *                    {"t":"c","dev":"led","id":1,"act":"toggle"}
 */

#include "cloud_service.h"
#include "terminal_service.h"
#include "bsp_uart.h"
#include "bsp_led.h"
#include "log.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLOUD_RX_BUF_SIZE    256U
#define CLOUD_TX_BUF_SIZE    256U

static uint8_t cloud_ready = 0U;
static uint32_t cloud_tick = 0U;
static uint8_t cloud_rx_buf[CLOUD_RX_BUF_SIZE];   /* static 分配，不占栈 */
static char cloud_tx_buf[CLOUD_TX_BUF_SIZE];       /* static 分配，不占栈 */

void cloud_service_init(void)
{
    cloud_ready = 0U;
    terminal_set_cloud_online(0U);
    bsp_uart2_rx_start();
}

uint8_t cloud_service_is_ready(void)
{
    return cloud_ready;
}

/**
 * @brief 从 JSON 字符串中提取整数值
 * @param json  JSON字符串
 * @param key   要查找的键名，如 "id"
 * @return 提取到的数值，找不到返回 -1
 */
static int json_get_int(const char *json, const char *key)
{
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (p == NULL) return -1;
    p += strlen(search);
    return (int)strtol(p, NULL, 10);
}

/**
 * @brief 从 JSON 字符串中提取字符串值
 * @param json   JSON字符串
 * @param key    要查找的键名
 * @param out    输出缓冲区
 * @param size   缓冲区大小
 * @return 1=成功, 0=失败
 */
static int json_get_str(const char *json, const char *key, char *out, size_t size)
{
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (p == NULL) return 0;
    p += strlen(search);
    const char *q = strchr(p, '"');
    if (q == NULL) return 0;
    size_t len = (size_t)(q - p);
    if (len >= size) len = size - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

/**
 * @brief 解析 ESP32 发来的控制指令
 * 指令格式: {"t":"c","dev":"led","id":1,"act":"on"}
 */
void cloud_service_parse_command(const char *json_str)
{
    if (json_str == NULL) return;
    if (strstr(json_str, "\"t\":\"c\"") == NULL) return;

    char dev[16] = {0};
    if (!json_get_str(json_str, "dev", dev, sizeof(dev))) return;

    /* ---- 板载 LED 控制（8个LED，PC8-PC15） ---- */
    if (strcmp(dev, "led") == 0) {
        int id = json_get_int(json_str, "id");
        char act[16] = {0};
        if (!json_get_str(json_str, "act", act, sizeof(act))) return;

        if (id < 1 || id > 8) return;

        if (strcmp(act, "on") == 0) {
            bsp_led_on((uint8_t)id);
            log_infof("[CMD] LED%d ON\r\n", id);
        } else if (strcmp(act, "off") == 0) {
            bsp_led_off((uint8_t)id);
            log_infof("[CMD] LED%d OFF\r\n", id);
        } else if (strcmp(act, "toggle") == 0) {
            bsp_led_toggle((uint8_t)id);
            log_infof("[CMD] LED%d TOGGLE\r\n", id);
        }
    }
    /* ---- 全部 LED 控制 ---- */
    else if (strcmp(dev, "led_all") == 0) {
        char act[16] = {0};
        if (!json_get_str(json_str, "act", act, sizeof(act))) return;
        if (strcmp(act, "on") == 0) {
            bsp_led_write8(0xFF);
            log_info("[CMD] ALL LED ON\r\n");
        } else if (strcmp(act, "off") == 0) {
            bsp_led_write8(0x00);
            log_info("[CMD] ALL LED OFF\r\n");
        }
    }
}

/**
 * @brief 检查并处理 ESP32 发来的指令
 */
static void cloud_check_commands(void)
{
    uint16_t len = bsp_uart_copy_rx_buffer(&huart2, cloud_rx_buf, sizeof(cloud_rx_buf) - 1U);

    if (len == 0U) return;

    cloud_rx_buf[len] = '\0';
    log_infof("[CLOUD] rx %u bytes: %s\r\n", len, cloud_rx_buf);

    /* ESP32 发来的 JSON 指令可能包含多条（以 \n 分隔） */
    char *line = strtok((char *)cloud_rx_buf, "\n");
    while (line != NULL) {
        if (strstr(line, "\"t\":\"c\"") != NULL) {
            log_infof("[CLOUD] cmd received: %s\r\n", line);
            cloud_service_parse_command(line);
        }
        line = strtok(NULL, "\n");
    }
}

/**
 * @brief 向 ESP32 发送 JSON 数据
 */
static void cloud_send_data(const terminal_data_t *data)
{
    int mb = data->remote_online ? 1 : 0;
    int cl = cloud_ready ? 1 : 0;

    snprintf(cloud_tx_buf, sizeof(cloud_tx_buf),
        "{\"t\":\"d\",\"l\":%u,\"r\":%u,\"mb\":%d,\"cl\":%d,"
        "\"s\":%lu,\"ok\":%lu,\"f\":%lu,\"up\":%lu}\n",
        data->local_value,
        data->remote_value,
        mb, cl,
        (unsigned long)data->sample_count,
        (unsigned long)data->modbus_ok_count,
        (unsigned long)data->modbus_fail_count,
        (unsigned long)data->upload_count);

    bsp_uart2_send_string(cloud_tx_buf);
}

void cloud_service_publish_once(void)
{
    cloud_tick++;

    /* 检查并处理 ESP32 发来的指令 */
    cloud_check_commands();

    /* 首次标记 ESP32 已就绪 */
    if (cloud_ready == 0U) {
        cloud_ready = 1U;
        terminal_set_cloud_online(1U);
        log_info("[CLOUD] ESP32 connected, JSON protocol ready\r\n");
    }

    /* 获取数据快照并发送给 ESP32 */
    terminal_data_t data;
    terminal_get_snapshot(&data);
    cloud_send_data(&data);

    /* 每 10 次（约 5 秒）打印一次状态 */
    if ((cloud_tick % 10U) == 0U) {
        log_infof("[CLOUD] tick=%lu local=%u remote=%u mb=%s up=%lu\r\n",
            (unsigned long)cloud_tick,
            data.local_value,
            data.remote_value,
            data.remote_online ? "ON" : "OFF",
            (unsigned long)data.upload_count);
    }
}
