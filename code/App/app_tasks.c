#include "app_common.h"
#include "sample_service.h"
#include "dht11_service.h"

/* ========================================================================
   任务心跳管理（用于 watchdogTask 监控各任务是否卡死）
   ======================================================================== */

#define MAX_HEARTBEATS 7U

static task_heartbeat_t *g_heartbeat_list[MAX_HEARTBEATS];
static uint8_t           g_heartbeat_count = 0U;

void task_heartbeat_register(task_heartbeat_t *hb)
{
    if (hb == NULL) return;
    if (g_heartbeat_count >= MAX_HEARTBEATS) return;
    g_heartbeat_list[g_heartbeat_count++] = hb;
}

void task_heartbeat_beat(task_heartbeat_t *hb)
{
    if (hb == NULL) return;
    uint32_t now = osKernelGetTickCount();
    uint32_t interval = now - hb->last_beat;
    hb->last_beat = now;
    if (interval > hb->max_interval) {
        hb->max_interval = interval;
    }
}

/* 获取任务栈高水位（剩余最小栈空间，单位：字节） */
uint32_t task_get_stack_high_water_mark(void)
{
    osThreadId_t threads[10];
    uint32_t count = osThreadEnumerate(threads, 10U);
    uint32_t min_space = 0xFFFFFFFFU;

    for (uint32_t i = 0U; i < count; i++) {
        if (threads[i] != NULL) {
            uint32_t space = osThreadGetStackSpace(threads[i]);
            if (space > 0U && space < min_space) {
                min_space = space;
            }
        }
    }
    return (min_space == 0xFFFFFFFFU) ? 0U : min_space;
}

/* ========================================================================
   告警 LED 联动映射：
   LED1 = 过温  LED2 = 欠温  LED3 = 过压  LED4 = 欠压
   LED5 = Modbus 离线  LED6 = 云端离线  LED7-8 = 保留
   ======================================================================== */

static volatile uint8_t g_alarm_pending_report = 0U;
static volatile uint8_t g_alarm_report_value   = 0U;

static void alarm_led_update(uint8_t old_status, uint8_t new_status)
{
    uint8_t leds;

    /* 保留 LED5-8（由 ledTask 维护），只更新告警 LED1-4 */
    leds = bsp_led_get_state();
    leds &= 0xF0U;
    if (new_status & ALARM_BIT_OVER_TEMP)  leds |= (1U << 0);  /* LED1 */
    if (new_status & ALARM_BIT_UNDER_TEMP) leds |= (1U << 1);  /* LED2 */
    if (new_status & ALARM_BIT_OVER_VOLT)  leds |= (1U << 2);  /* LED3 */
    if (new_status & ALARM_BIT_UNDER_VOLT) leds |= (1U << 3);  /* LED4 */

    /* 状态变化时写事件日志 */
    uint8_t changed = old_status ^ new_status;
    if (changed & ALARM_BIT_OVER_TEMP) {
        event_log_add((new_status & ALARM_BIT_OVER_TEMP) ? EVENT_ALARM_SET : EVENT_ALARM_CLEAR, ALARM_BIT_OVER_TEMP);
    }
    if (changed & ALARM_BIT_UNDER_TEMP) {
        event_log_add((new_status & ALARM_BIT_UNDER_TEMP) ? EVENT_ALARM_SET : EVENT_ALARM_CLEAR, ALARM_BIT_UNDER_TEMP);
    }
    if (changed & ALARM_BIT_OVER_VOLT) {
        event_log_add((new_status & ALARM_BIT_OVER_VOLT) ? EVENT_ALARM_SET : EVENT_ALARM_CLEAR, ALARM_BIT_OVER_VOLT);
    }
    if (changed & ALARM_BIT_UNDER_VOLT) {
        event_log_add((new_status & ALARM_BIT_UNDER_VOLT) ? EVENT_ALARM_SET : EVENT_ALARM_CLEAR, ALARM_BIT_UNDER_VOLT);
    }

    bsp_led_write8(leds);

    /* 标记待立即上报（回调里不直接发 UART，避免阻塞采样路径） */
    if (changed != 0U) {
        g_alarm_report_value = new_status;
        g_alarm_pending_report = 1U;
    }
}

/* ========================================================================
   初始化
   ======================================================================== */

void app_init_before_scheduler(void)
{
    /* ====================================================================
       启动心跳：PC8(LD1)闪3次，证明代码跑到了这里
       注意：需先打开 74HC573 锁存（PD2），否则板载 LED 看不到
       ==================================================================== */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8);
        HAL_Delay(200);
    }
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);

    /* LCD 放到最前面，不受其他初始化影响 */
    bsp_lcd_init();

    /* LCD 完成后，PC8 长亮 500ms 作为确认信号 */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET); /* LED 低电平点亮 */
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);

    /* 以下所有初始化如果卡死，至少 LCD 已经亮了 */
    terminal_init();
    flash_config_init();
    event_log_init();
    modbus_service_init();
    cloud_service_init();
    bsp_led_write8(0x00);
    bsp_rs485_init();
    bsp_rs485_set_rx_mode();

    bsp_uart1_rx_start();
    bsp_uart3_rx_start();

    /* 初始化 ADC DMA 采样（DMA 循环转换，零 CPU 占用） */
    sample_service_init();

    /* 初始化 DHT11 温湿度传感器 */
    dht11_service_init();

    /* 注册告警回调：告警触发/恢复 → LED 联动 + 事件日志 */
    terminal_set_alarm_callback(alarm_led_update);

    /* 记录启动事件 */
    event_log_add(EVENT_SYSTEM_BOOT, 0U);

    log_info("[BOOT] 7-task system start\r\n");
}

/* ========================================================================
   任务 1：sensorTask（osPriorityAboveNormal, 200ms）
   传感器采集 + 滤波 + DHT11 + 告警检测
   ======================================================================== */

static task_heartbeat_t hb_sensor = {0, 0, 0, "sensor"};

void app_sensor_task(void *argument)
{
    (void)argument;
    task_heartbeat_register(&hb_sensor);

    for (;;) {
        task_heartbeat_beat(&hb_sensor);

        sample_service_process();

        /* 告警状态变化时立即上报云端 */
        if (g_alarm_pending_report != 0U) {
            uint8_t al = g_alarm_report_value;
            g_alarm_pending_report = 0U;
            cloud_service_send_alarm(al);
        }

        task_heartbeat_beat(&hb_sensor);
        osDelay(SENSOR_TASK_PERIOD_MS);
    }
}

/* ========================================================================
   任务 2：watchdogTask（osPriorityRealtime, 500ms）
   独立喂狗 + 任务心跳异常检测
   ======================================================================== */

static task_heartbeat_t hb_watchdog = {0, 0, 0, "watchdog"};

void app_watchdog_task(void *argument)
{
    (void)argument;
    task_heartbeat_register(&hb_watchdog);

    for (;;) {
        task_heartbeat_beat(&hb_watchdog);

        /* 喂狗：独立于其他任务，不依赖 idle 任务 */
        HAL_IWDG_Refresh(&hiwdg);

        /* 检查各任务心跳 */
        uint32_t now = osKernelGetTickCount();
        for (uint8_t i = 0U; i < g_heartbeat_count; i++) {
            task_heartbeat_t *hb = g_heartbeat_list[i];
            if (hb == NULL) continue;
            uint32_t interval = now - hb->last_beat;
            if (interval > TASK_HEARTBEAT_TIMEOUT_MS) {
                hb->timeout_cnt++;
                if (hb->timeout_cnt == 1U) {
                    log_infof("[WATCHDOG] task %s timeout! interval=%lums\r\n",
                              hb->name, (unsigned long)interval);
                }
            } else {
                hb->timeout_cnt = 0U;
            }
        }

        task_heartbeat_beat(&hb_watchdog);
        osDelay(WATCHDOG_TASK_PERIOD_MS);
    }
}

/* ========================================================================
   任务 3：modbusTask（osPriorityAboveNormal, 1000ms）
   Modbus RTU 主站轮询
   ======================================================================== */

static task_heartbeat_t hb_modbus = {0, 0, 0, "modbus"};

void app_modbus_task(void *argument)
{
    (void)argument;
    task_heartbeat_register(&hb_modbus);

    for (;;) {
        task_heartbeat_beat(&hb_modbus);

        modbus_service_poll_once();

        task_heartbeat_beat(&hb_modbus);
        osDelay(MODBUS_TASK_PERIOD_MS);
    }
}

/* ========================================================================
   任务 4：cloudTask（osPriorityNormal, 500ms）
   云端数据上报 + 命令接收 + 命令投递到队列
   ======================================================================== */

static task_heartbeat_t hb_cloud = {0, 0, 0, "cloud"};

void app_cloud_task(void *argument)
{
    (void)argument;
    task_heartbeat_register(&hb_cloud);

    for (;;) {
        task_heartbeat_beat(&hb_cloud);

        cloud_service_publish_once();

        task_heartbeat_beat(&hb_cloud);
        osDelay(CLOUD_TASK_PERIOD_MS);
    }
}

/* ========================================================================
   任务 5：cmdTask（osPriorityNormal, 事件驱动）
   从命令队列取命令并异步处理（Flash 写入、Modbus 写等耗时操作）
   ======================================================================== */

static task_heartbeat_t hb_cmd = {0, 0, 0, "cmd"};

void app_cmd_task(void *argument)
{
    (void)argument;
    cloud_cmd_t cmd;

    task_heartbeat_register(&hb_cmd);

    for (;;) {
        if (g_cmd_queue == NULL) {
            task_heartbeat_beat(&hb_cmd);
            osDelay(CMD_TASK_IDLE_SLEEP_MS);
            continue;
        }

        /* 阻塞等待命令（1s 超时，用于心跳更新） */
        if (osMessageQueueGet(g_cmd_queue, &cmd, NULL, CMD_QUEUE_WAIT_MS) == osOK) {
            task_heartbeat_beat(&hb_cmd);
            cloud_service_handle_cmd(&cmd);
        }

        task_heartbeat_beat(&hb_cmd);
    }
}

/* ========================================================================
   任务 6：ledTask（osPriorityBelowNormal, 1000ms）
   OLED 显示刷新 + 状态 LED 更新（Modbus/Cloud 离线指示）
   ======================================================================== */

static task_heartbeat_t hb_led = {0, 0, 0, "led"};

void app_led_task(void *argument)
{
    (void)argument;
    task_heartbeat_register(&hb_led);

    for (;;) {
        task_heartbeat_beat(&hb_led);

        terminal_data_t data;
        terminal_get_snapshot(&data);

        /* 更新 Modbus 离线 LED (LED5) 和 Cloud 离线 LED (LED6) */
        uint8_t base_leds = bsp_led_get_state();
        if (!data.remote_online) base_leds |= (1U << 4);   /* LED5 */
        else                      base_leds &= ~(1U << 4);
        if (!data.cloud_online)   base_leds |= (1U << 5);   /* LED6 */
        else                      base_leds &= ~(1U << 5);
        bsp_led_write8(base_leds);

        bsp_lcd_show_test_info(&data);

        task_heartbeat_beat(&hb_led);
        osDelay(LED_TASK_PERIOD_MS);
    }
}

/* ========================================================================
   任务 7：logTask（osPriorityBelowNormal, 1000ms）
   串口日志输出 + 系统运行时间递增
   ======================================================================== */

static task_heartbeat_t hb_log = {0, 0, 0, "log"};

void app_log_task(void *argument)
{
    (void)argument;
    task_heartbeat_register(&hb_log);

    for (;;) {
        task_heartbeat_beat(&hb_log);

        terminal_data_t data;
        char buf[256];

        /* 每秒递增运行时间 */
        terminal_inc_uptime();

        terminal_get_snapshot(&data);
        snprintf(buf, sizeof(buf),
                 "[STATE] c0=%u c1=%u c2=%u v0=%.2fV v1=%.2fV temp=%.1fC "
                 "r=%u r1=%u mb=%s cloud=%s s=%lu ok=%lu fail=%lu up=%lu al=0x%02X hum=%u%% uptime=%lus "
                 "stack=%lu\r\n",
                 data.sample.filtered[0],
                 data.sample.filtered[1],
                 data.sample.filtered[2],
                 data.sample.voltage[0],
                 data.sample.voltage[1],
                 data.humidity_valid ? (double)data.dht11_temperature : (double)data.sample.temperature[2],
                 data.remote_value,
                 data.remote_value2,
                 data.remote_online ? "ON" : "OFF",
                 data.cloud_online ? "ON" : "OFF",
                 (unsigned long)data.sample_count,
                 (unsigned long)data.modbus_ok_count,
                 (unsigned long)data.modbus_fail_count,
                 (unsigned long)data.upload_count,
                 data.alarm_status,
                 data.humidity,
                 (unsigned long)data.uptime_seconds,
                 (unsigned long)task_get_stack_high_water_mark());
        log_info(buf);

        task_heartbeat_beat(&hb_log);
        osDelay(LOG_TASK_PERIOD_MS);
    }
}
