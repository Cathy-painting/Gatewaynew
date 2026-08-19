#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>

/* ========================================================================
   7 任务架构（优先级从高到低）：
     osPriorityRealtime:     watchdogTask (500ms, 独立喂狗+心跳检测)
     osPriorityAboveNormal:  modbusTask   (1000ms, Modbus RTU 主站轮询)
     osPriorityAboveNormal:  sensorTask   (200ms, 传感器采集+滤波+告警)
     osPriorityNormal:       cloudTask    (500ms, 云端数据上报+命令接收)
     osPriorityNormal:       cmdTask      (事件驱动, 云端命令处理)
     osPriorityBelowNormal:  ledTask      (1000ms, OLED显示+状态LED)
     osPriorityLow:          logTask      (1000ms, 串口日志+运行时间)
   ======================================================================== */

/* ========================================================================
   任务周期宏定义（ms）
   ======================================================================== */
#define SENSOR_TASK_PERIOD_MS      200U
#define WATCHDOG_TASK_PERIOD_MS    500U
#define MODBUS_TASK_PERIOD_MS     1000U
#define CLOUD_TASK_PERIOD_MS       500U
#define CMD_TASK_IDLE_SLEEP_MS     100U
#define CMD_QUEUE_WAIT_MS         1000U
#define LED_TASK_PERIOD_MS        1000U
#define LOG_TASK_PERIOD_MS        1000U

/* 任务心跳超时阈值（ms），超过此值认为任务异常 */
#define TASK_HEARTBEAT_TIMEOUT_MS  5000U

/* 任务心跳数据结构（用于 watchdogTask 监控） */
typedef struct {
    uint32_t last_beat;       /* 上次心跳的时间戳（ms） */
    uint32_t max_interval;    /* 最大间隔（ms） */
    uint8_t  timeout_cnt;     /* 超时次数 */
    const char *name;         /* 任务名称 */
} task_heartbeat_t;

void app_init_before_scheduler(void);

/* 7 个任务函数（CMSIS-RTOS2: void (*)(void *argument)，内部必须 for(;;) 永不返回） */
void app_sensor_task(void *argument);
void app_watchdog_task(void *argument);
void app_modbus_task(void *argument);
void app_cloud_task(void *argument);
void app_cmd_task(void *argument);
void app_led_task(void *argument);
void app_log_task(void *argument);

/* 心跳注册（其他任务在 watchdogTask 注册自己的心跳） */
void task_heartbeat_register(task_heartbeat_t *hb);
void task_heartbeat_beat(task_heartbeat_t *hb);

/* 获取任务栈高水位（剩余最小栈空间，单位：字节） */
uint32_t task_get_stack_high_water_mark(void);

#endif
