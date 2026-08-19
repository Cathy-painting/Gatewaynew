#include "cloud_service.h"
#include "terminal_service.h"
#include "modbus_service.h"
#include "flash_config.h"
#include "event_log.h"
#include "bsp_uart.h"
#include "bsp_led.h"
#include "crc16.h"
#include "log.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLOUD_RX_BUF_SIZE    256U
#define CLOUD_TX_BUF_SIZE    384U

/* 全局变量：命令队列和 UART2 互斥锁 */
osMessageQueueId_t g_cmd_queue = NULL;
osMutexId_t        g_uart2_mutex = NULL;

static uint8_t  cloud_ready = 0U;
static uint32_t cloud_tick  = 0U;
static uint32_t cloud_last_rx_tick = 0U;
static char     cloud_tx_buf[CLOUD_TX_BUF_SIZE];

/* 超过该时间未从 UART2 收到任何字节，则判定云端离线 */
#define CLOUD_RX_TIMEOUT_MS  10000U

/* 粘包处理：保留未完整行 */
static char     cloud_line_buf[CLOUD_RX_BUF_SIZE];
static uint16_t cloud_line_len = 0U;

static void cloud_mark_rx_alive(void)
{
    cloud_last_rx_tick = osKernelGetTickCount();
    if (cloud_ready == 0U) {
        cloud_ready = 1U;
        terminal_set_cloud_online(1U);
        event_log_add(EVENT_CLOUD_CONNECT, 0U);
        log_info("[CLOUD] ESP32 link UP (UART2 RX)\r\n");
    }
}

static void cloud_check_link_timeout(void)
{
    if (cloud_ready == 0U) {
        return;
    }
    if ((osKernelGetTickCount() - cloud_last_rx_tick) > CLOUD_RX_TIMEOUT_MS) {
        cloud_ready = 0U;
        terminal_set_cloud_online(0U);
        event_log_add(EVENT_CLOUD_DISCONNECT, 0U);
        log_info("[CLOUD] ESP32 link DOWN (no RX)\r\n");
    }
}

/* ========================================================================
   简易 JSON 解析（不依赖第三方库，适配嵌入式）
   ======================================================================== */

void cloud_service_init(void)
{
    cloud_ready = 0U;
    cloud_last_rx_tick = 0U;
    terminal_set_cloud_online(0U);
    bsp_uart2_rx_start();
    cloud_line_len = 0U;
}

uint8_t cloud_service_is_ready(void)
{
    return cloud_ready;
}

static int json_get_int(const char *json, const char *key)
{
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (p == NULL) return -1;
    p += strlen(search);
    return (int)strtol(p, NULL, 10);
}

static float json_get_float(const char *json, const char *key)
{
    char search[32];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (p == NULL) return -999.0f;
    p += strlen(search);
    return (float)strtod(p, NULL);
}

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

/* ========================================================================
   UART2 发送（带互斥锁保护）
   ======================================================================== */

static void uart2_send(const char *str)
{
    if (g_uart2_mutex == NULL) {
        bsp_uart2_send_string(str);
        return;
    }
    if (osMutexAcquire(g_uart2_mutex, 100U) == osOK) {
        bsp_uart2_send_string(str);
        osMutexRelease(g_uart2_mutex);
    }
}

/* ========================================================================
   云端命令响应（ACK/NACK）
   ======================================================================== */

static void cloud_send_ack(const char *cmd, const char *msg)
{
    snprintf(cloud_tx_buf, sizeof(cloud_tx_buf),
             "{\"t\":\"ack\",\"cmd\":\"%s\",\"status\":\"ok\",\"msg\":\"%s\"}\n",
             cmd, msg);
    uart2_send(cloud_tx_buf);
}

static void cloud_send_nack(const char *cmd, const char *msg)
{
    snprintf(cloud_tx_buf, sizeof(cloud_tx_buf),
             "{\"t\":\"ack\",\"cmd\":\"%s\",\"status\":\"err\",\"msg\":\"%s\"}\n",
             cmd, msg);
    uart2_send(cloud_tx_buf);
}

/* ========================================================================
   公共 JSON 格式化函数：消除 cloud_send_data 和 cloud_send_query 的重复代码
   ======================================================================== */

static void cloud_format_data_json(const terminal_data_t *data,
                                    uint8_t include_thresholds)
{
    const alarm_threshold_t *thr = flash_config_get_alarm_thresholds();
    int len;

    len = snprintf(cloud_tx_buf, sizeof(cloud_tx_buf),
        "{\"t\":\"%s\",\"c0\":%u,\"c1\":%u,\"c2\":%u,"
        "\"v0\":%.2f,\"v1\":%.2f,\"temp\":%.1f,"
        "\"r\":%u,\"r1\":%u,\"mb\":%d,\"cl\":%d,"
        "\"s\":%lu,\"ok\":%lu,\"f\":%lu,\"up\":%lu,"
        "\"al\":%u,\"hum\":%u,\"hum_valid\":%d,"
        "\"uptime\":%lu",
        include_thresholds ? "query" : "d",
        data->sample.filtered[0],
        data->sample.filtered[1],
        data->sample.filtered[2],
        data->sample.voltage[0],
        data->sample.voltage[1],
        data->humidity_valid ? data->dht11_temperature : data->sample.temperature[2],
        data->remote_value,
        data->remote_value2,
        data->remote_online ? 1 : 0,
        cloud_ready ? 1 : 0,
        (unsigned long)data->sample_count,
        (unsigned long)data->modbus_ok_count,
        (unsigned long)data->modbus_fail_count,
        (unsigned long)data->upload_count,
        data->alarm_status,
        data->humidity,
        data->humidity_valid ? 1 : 0,
        (unsigned long)data->uptime_seconds);

    if (include_thresholds && len > 0 && (uint16_t)len < sizeof(cloud_tx_buf) - 80) {
        int n = snprintf(cloud_tx_buf + len, sizeof(cloud_tx_buf) - (size_t)len,
            ",\"th_tempH\":%.1f,\"th_tempL\":%.1f,\"th_voltH\":%.2f,\"th_voltL\":%.2f",
            thr->temp_high, thr->temp_low, thr->volt_high, thr->volt_low);
        if (n > 0) {
            len += n;
        }
    }

    /* 先闭合 JSON，对「不含 crc 字段的完整 JSON」做 CRC，再插入 crc 字段。
     * 与 ESP32 validate_crc() 约定一致。 */
    if (len > 0 && (size_t)len + 16U < sizeof(cloud_tx_buf)) {
        cloud_tx_buf[len++] = '}';
        cloud_tx_buf[len] = '\0';

        uint16_t crc = crc16_modbus((const uint8_t *)cloud_tx_buf, (uint16_t)len);
        /* 去掉末尾 '}'，改成 ,"crc":N} */
        cloud_tx_buf[len - 1] = '\0';
        snprintf(cloud_tx_buf + (len - 1), sizeof(cloud_tx_buf) - (size_t)(len - 1),
                 ",\"crc\":%u}\n", (unsigned)crc);
    }
}

static void cloud_send_query(const terminal_data_t *data)
{
    cloud_format_data_json(data, 1);
    uart2_send(cloud_tx_buf);
}

static void cloud_send_data(const terminal_data_t *data)
{
    cloud_format_data_json(data, 0);
    uart2_send(cloud_tx_buf);
    terminal_inc_upload();
}

/* ========================================================================
   命令处理函数（由 cmdTask 调用）
   ======================================================================== */

/* LED 控制 */
static void handle_led_cmd(const char *json_str)
{
    char dev[16] = {0};
    if (!json_get_str(json_str, "dev", dev, sizeof(dev))) return;

    if (strcmp(dev, "led") == 0) {
        int id = json_get_int(json_str, "id");
        char act[16] = {0};
        if (!json_get_str(json_str, "act", act, sizeof(act))) return;
        if (id < 1 || id > 8) return;

        if (strcmp(act, "on") == 0) {
            bsp_led_on((uint8_t)id);
        } else if (strcmp(act, "off") == 0) {
            bsp_led_off((uint8_t)id);
        } else if (strcmp(act, "toggle") == 0) {
            bsp_led_toggle((uint8_t)id);
        }
        cloud_send_ack("led", "ok");
        event_log_add(EVENT_USER_CMD, (uint8_t)id);
    }
    else if (strcmp(dev, "led_all") == 0) {
        char act[16] = {0};
        if (!json_get_str(json_str, "act", act, sizeof(act))) return;
        if (strcmp(act, "on") == 0) {
            bsp_led_write8(0xFF);
        } else if (strcmp(act, "off") == 0) {
            bsp_led_write8(0x00);
        }
        cloud_send_ack("led_all", "ok");
        event_log_add(EVENT_USER_CMD, 0xFFU);
    }
}

/* 查询数据 */
static void handle_query_cmd(void)
{
    terminal_data_t data;
    terminal_get_snapshot(&data);
    cloud_send_query(&data);
}

/* 配置参数 */
static void handle_config_cmd(const char *json_str)
{
    flash_config_t cfg;
    flash_config_get(&cfg);

    int val_i;
    float val_f;

    val_f = json_get_float(json_str, "temp_high");
    if (val_f > -900.0f) cfg.alarm.temp_high = val_f;

    val_f = json_get_float(json_str, "temp_low");
    if (val_f > -900.0f) cfg.alarm.temp_low = val_f;

    val_f = json_get_float(json_str, "volt_high");
    if (val_f > -900.0f) cfg.alarm.volt_high = val_f;

    val_f = json_get_float(json_str, "volt_low");
    if (val_f > -900.0f) cfg.alarm.volt_low = val_f;

    val_i = json_get_int(json_str, "sample_period");
    if (val_i >= CFG_PERIOD_MIN_MS && val_i <= CFG_PERIOD_MAX_MS) cfg.sample_period_ms = (uint16_t)val_i;

    val_i = json_get_int(json_str, "cloud_period");
    if (val_i >= CFG_PERIOD_MIN_MS && val_i <= CFG_PERIOD_MAX_MS) cfg.cloud_period_ms = (uint16_t)val_i;

    if (flash_config_save(&cfg)) {
        cloud_send_ack("config", "saved");
        event_log_add(EVENT_CONFIG_SAVED, 0U);
    } else {
        cloud_send_nack("config", "flash write failed");
    }
}

/* 云端触发 Modbus 写寄存器 */
static void handle_write_cmd(const char *json_str)
{
    int reg  = json_get_int(json_str, "reg");
    int val  = json_get_int(json_str, "val");

    if (reg < 0 || val < 0) {
        cloud_send_nack("write", "bad params");
        return;
    }

    if (modbus_service_write_single((uint16_t)reg, (uint16_t)val)) {
        cloud_send_ack("write", "ok");
        event_log_add(EVENT_USER_CMD, (uint8_t)val);
    } else {
        cloud_send_nack("write", "modbus fail");
    }
}

/* 软复位 */
static void handle_reboot_cmd(void)
{
    cloud_send_ack("reboot", "rebooting");
    osDelay(200);
    NVIC_SystemReset();
}

/* ========================================================================
   cmdTask 命令处理入口
   ======================================================================== */

void cloud_service_handle_cmd(const cloud_cmd_t *cmd)
{
    if (cmd == NULL) return;

    switch (cmd->type) {
    case CMD_TYPE_QUERY:
        handle_query_cmd();
        break;
    case CMD_TYPE_CONFIG:
        handle_config_cmd(cmd->data);
        break;
    case CMD_TYPE_WRITE:
        handle_write_cmd(cmd->data);
        break;
    case CMD_TYPE_REBOOT:
        handle_reboot_cmd();
        break;
    case CMD_TYPE_LED:
        handle_led_cmd(cmd->data);
        break;
    case CMD_TYPE_NONE:
    default:
        break;
    }
}

/* ========================================================================
   告警立即上报（由 sensorTask 触发，发现告警变化时立即推送）
   ======================================================================== */

void cloud_service_send_alarm(uint8_t alarm_status)
{
    if (!cloud_ready) return;

    snprintf(cloud_tx_buf, sizeof(cloud_tx_buf),
             "{\"t\":\"alarm\",\"al\":%u,\"uptime\":%lu}\n",
             alarm_status,
             (unsigned long)terminal_get_uptime());
    uart2_send(cloud_tx_buf);
}

/* ========================================================================
   命令解析（由 cloudTask 调用）→ 投递到命令队列而非直接处理
   ======================================================================== */

void cloud_service_parse_command(const char *json_str)
{
    if (json_str == NULL) return;
    if (g_cmd_queue == NULL) return;

    cloud_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    /* 判断命令类型 */
    char cmd_type[16] = {0};
    if (!json_get_str(json_str, "t", cmd_type, sizeof(cmd_type))) return;

    if (strcmp(cmd_type, "c") == 0) {
        cmd.type = CMD_TYPE_LED;
    }
    else if (strcmp(cmd_type, "query") == 0) {
        cmd.type = CMD_TYPE_QUERY;
    }
    else if (strcmp(cmd_type, "config") == 0) {
        cmd.type = CMD_TYPE_CONFIG;
    }
    else if (strcmp(cmd_type, "write") == 0) {
        cmd.type = CMD_TYPE_WRITE;
    }
    else if (strcmp(cmd_type, "reboot") == 0) {
        cmd.type = CMD_TYPE_REBOOT;
    }
    else {
        return;
    }

    /* 保存原始 JSON 数据 */
    strncpy(cmd.data, json_str, sizeof(cmd.data) - 1);
    cmd.data[sizeof(cmd.data) - 1] = '\0';

    /* 投递到命令队列，cmdTask 异步处理 */
    if (osMessageQueuePut(g_cmd_queue, &cmd, 0U, 0U) != osOK) {
        log_info("[CLOUD] cmd queue full, dropped\r\n");
    }
}

/* ========================================================================
   命令接收（粘包处理：逐字节扫描，保留未完整行）
   ======================================================================== */

static void cloud_process_line(const char *line)
{
    /* ESP32 心跳：只保活链路，不当命令处理 */
    if (strstr(line, "\"t\":\"ping\"") != NULL) {
        return;
    }
    if (strstr(line, "\"t\":\"") != NULL) {
        log_infof("[CLOUD] cmd received: %s\r\n", line);
        cloud_service_parse_command(line);
    }
}

static void cloud_check_commands(void)
{
    uint8_t  tmp_buf[CLOUD_RX_BUF_SIZE];
    uint16_t len = bsp_uart_copy_rx_buffer(&huart2, tmp_buf, sizeof(tmp_buf));

    if (len == 0U) return;

    /* 只要 UART2 收到字节，就认为 ESP32 链路活着（网页点按钮会回传 JSON） */
    cloud_mark_rx_alive();

    log_infof("[CLOUD] rx %u bytes\r\n", len);

    /* 逐字节扫描，遇到 \n 则处理完整行，否则保留到 cloud_line_buf */
    for (uint16_t i = 0U; i < len; i++) {
        if (tmp_buf[i] == '\n') {
            cloud_line_buf[cloud_line_len] = '\0';
            if (cloud_line_len > 0U) {
                cloud_process_line(cloud_line_buf);
            }
            cloud_line_len = 0U;
        } else if (cloud_line_len < sizeof(cloud_line_buf) - 1U) {
            cloud_line_buf[cloud_line_len++] = (char)tmp_buf[i];
        } else {
            /* 行太长，丢弃并重置 */
            cloud_line_len = 0U;
        }
    }
}

/* ========================================================================
   定时数据上报
   ======================================================================== */

void cloud_service_publish_once(void)
{
    cloud_tick++;

    cloud_check_commands();
    cloud_check_link_timeout();

    /* 注意：不再“一启动就 Cloud:ON”。
     * 只有 UART2 真正收到 ESP32 数据才置 ON；超时无收包则 OFF。 */

    terminal_data_t data;
    terminal_get_snapshot(&data);
    cloud_send_data(&data);

    if ((cloud_tick % CLOUD_LOG_INTERVAL) == 0U) {
        log_infof("[CLOUD] tick=%lu c0=%u c1=%u temp=%.1fC "
                  "mb=%s up=%lu alarm=0x%02X uptime=%lus\r\n",
            (unsigned long)cloud_tick,
            data.sample.filtered[0],
            data.sample.filtered[1],
            data.sample.temperature[2],
            data.remote_online ? "ON" : "OFF",
            (unsigned long)data.upload_count,
            data.alarm_status,
            (unsigned long)data.uptime_seconds);
    }
}
