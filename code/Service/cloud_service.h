#ifndef CLOUD_SERVICE_H
#define CLOUD_SERVICE_H

#include <stdint.h>
#include "cmsis_os.h"

/* ========================================================================
   云端通信服务 — ESP32 通过 UART2 JSON 协议通信
   架构：cloudTask 收包 → 投递命令到 cmdQueue → cmdTask 异步处理
   ======================================================================== */

/* 云端日志输出间隔（每 N 次上报输出一次日志） */
#define CLOUD_LOG_INTERVAL    10U

/* 配置参数周期范围（ms） */
#define CFG_PERIOD_MIN_MS     100U
#define CFG_PERIOD_MAX_MS     10000U

/* 命令类型 */
typedef enum {
    CMD_TYPE_NONE = 0,
    CMD_TYPE_QUERY,     /* 查询设备状态 */
    CMD_TYPE_CONFIG,    /* 配置参数（Flash 写入） */
    CMD_TYPE_WRITE,     /* Modbus 写寄存器 */
    CMD_TYPE_REBOOT,    /* 软复位 */
    CMD_TYPE_LED,       /* LED 控制 */
} cmd_type_t;

/* 命令消息 */
typedef struct {
    cmd_type_t type;
    char       data[128];
} cloud_cmd_t;

/* API */
void cloud_service_init(void);
void cloud_service_publish_once(void);
uint8_t cloud_service_is_ready(void);

/* 解析 ESP32 发来的 JSON 指令 → 投递到命令队列 */
void cloud_service_parse_command(const char *json_str);

/* cmdTask 使用的命令处理函数 */
void cloud_service_handle_cmd(const cloud_cmd_t *cmd);

/* 获取命令队列句柄（供 app_freertos.c 创建队列时使用） */
extern osMessageQueueId_t g_cmd_queue;

/* 告警立即上报（由 sensorTask 触发，不走定时周期） */
void cloud_service_send_alarm(uint8_t alarm_status);

/* UART2 发送互斥锁 */
extern osMutexId_t g_uart2_mutex;

#endif
