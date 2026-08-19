#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <stdint.h>

/* ========================================================================
   事件日志环形缓冲区 — 最近 32 条事件
   用于问题追溯和蓝牙/串口查询
   ======================================================================== */

#define EVENT_LOG_SIZE 32U

/* 事件类型 */
typedef enum {
    EVENT_NONE = 0,
    EVENT_ALARM_SET,        /* 告警触发 */
    EVENT_ALARM_CLEAR,      /* 告警恢复 */
    EVENT_MODBUS_ONLINE,    /* Modbus 从机上线 */
    EVENT_MODBUS_OFFLINE,   /* Modbus 从机离线 */
    EVENT_CLOUD_CONNECT,    /* 云端连接 */
    EVENT_CLOUD_DISCONNECT, /* 云端断开 */
    EVENT_SYSTEM_BOOT,      /* 系统启动 */
    EVENT_CONFIG_SAVED,     /* 配置保存 */
    EVENT_USER_CMD,         /* 用户云端命令 */
} event_type_t;

/* 事件记录 */
typedef struct {
    uint32_t    timestamp;  /* 系统运行时间（秒） */
    event_type_t type;
    uint8_t     value;      /* 附加数据（告警位图、从站ID 等） */
    uint8_t     reserved;
} event_entry_t;

/* API */
void event_log_init(void);
void event_log_add(event_type_t type, uint8_t value);
uint8_t event_log_get_recent(event_entry_t *out, uint8_t max_count);
uint8_t event_log_count(void);

#endif
