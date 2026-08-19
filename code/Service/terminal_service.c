#include "terminal_service.h"
#include "flash_config.h"
#include "main.h"
#include <string.h>

/* ========================================================================
   告警阈值 — 从 Flash 配置读取，不再硬编码
   ======================================================================== */

static terminal_data_t g_terminal_data;
static terminal_alarm_cb_t g_alarm_cb = NULL;

/* ========================================================================
   临界区保护：save/restore PRIMASK，支持嵌套
   解决原来 __disable_irq()/__enable_irq() 嵌套时内层提前开中断的 bug
   ======================================================================== */
static void terminal_lock(uint32_t *primask)
{
    *primask = __get_PRIMASK();
    __disable_irq();
}

static void terminal_unlock(uint32_t primask)
{
    __set_PRIMASK(primask);
}

/* ========================================================================
   公共接口
   ======================================================================== */

void terminal_init(void)
{
    uint32_t primask;
    terminal_lock(&primask);
    memset(&g_terminal_data, 0, sizeof(g_terminal_data));
    terminal_unlock(primask);
}

/* --- ADC 采样数据更新 --- */

void terminal_update_sample(uint16_t ch, uint16_t raw, uint16_t filtered,
                            float voltage, float temperature)
{
    uint32_t primask;
    if (ch >= ADC_CH_NUM) return;

    terminal_lock(&primask);
    g_terminal_data.sample.raw[ch]       = raw;
    g_terminal_data.sample.filtered[ch]  = filtered;
    g_terminal_data.sample.voltage[ch]   = voltage;
    g_terminal_data.sample.temperature[ch] = temperature;
    terminal_unlock(primask);
}

void terminal_update_alarms(void)
{
    uint32_t primask;
    uint8_t new_alarm = 0U;
    float temp, v0;
    uint8_t debounce[4];
    const alarm_threshold_t *thr = flash_config_get_alarm_thresholds();

    /* 原子快照：读取需要检测的值和消抖计数器 */
    terminal_lock(&primask);
    temp = g_terminal_data.sample.temperature[ADC_CH_NTC];
    v0   = g_terminal_data.sample.voltage[ADC_CH_PB15];
    memcpy(debounce, g_terminal_data.alarm_debounce, sizeof(debounce));
    terminal_unlock(primask);

    /* 过温检测 */
    if (temp > thr->temp_high) {
        if (debounce[0] < ALARM_DEBOUNCE_CNT) {
            debounce[0]++;
        }
    } else {
        debounce[0] = 0U;
    }

    /* 欠温检测 */
    if (temp < thr->temp_low && temp > -50.0f) {
        if (debounce[1] < ALARM_DEBOUNCE_CNT) {
            debounce[1]++;
        }
    } else {
        debounce[1] = 0U;
    }

    /* 过压检测（通道0 PB15 电压） */
    if (v0 > thr->volt_high) {
        if (debounce[2] < ALARM_DEBOUNCE_CNT) {
            debounce[2]++;
        }
    } else {
        debounce[2] = 0U;
    }

    /* 欠压检测 */
    if (v0 < thr->volt_low) {
        if (debounce[3] < ALARM_DEBOUNCE_CNT) {
            debounce[3]++;
        }
    } else {
        debounce[3] = 0U;
    }

    /* 构建告警位图 */
    if (debounce[0] >= ALARM_DEBOUNCE_CNT) new_alarm |= ALARM_BIT_OVER_TEMP;
    if (debounce[1] >= ALARM_DEBOUNCE_CNT) new_alarm |= ALARM_BIT_UNDER_TEMP;
    if (debounce[2] >= ALARM_DEBOUNCE_CNT) new_alarm |= ALARM_BIT_OVER_VOLT;
    if (debounce[3] >= ALARM_DEBOUNCE_CNT) new_alarm |= ALARM_BIT_UNDER_VOLT;

    /* 原子写回：消抖计数器和告警状态 */
    terminal_lock(&primask);
    {
        uint8_t old_status = g_terminal_data.alarm_status;
        memcpy(g_terminal_data.alarm_debounce, debounce, sizeof(debounce));
        g_terminal_data.alarm_status = new_alarm;
        terminal_unlock(primask);

        /* 告警状态变化时通知回调（用于 LED 联动、事件日志）
           注意：回调在锁外执行，避免死锁 */
        if (g_alarm_cb != NULL && new_alarm != old_status) {
            g_alarm_cb(old_status, new_alarm);
        }
    }
}

/* --- Modbus 远程数据 --- */

void terminal_set_remote_value(uint16_t value)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.remote_value = value;
    terminal_unlock(primask);
}

void terminal_set_remote_value2(uint16_t value)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.remote_value2 = value;
    terminal_unlock(primask);
}

void terminal_set_remote_online(uint8_t online)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.remote_online = (online != 0U) ? 1U : 0U;
    terminal_unlock(primask);
}

void terminal_inc_modbus_ok(void)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.modbus_ok_count++;
    terminal_unlock(primask);
}

void terminal_inc_modbus_fail(void)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.modbus_fail_count++;
    terminal_unlock(primask);
}

/* --- 云端连接 --- */

void terminal_set_cloud_online(uint8_t online)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.cloud_online = (online != 0U) ? 1U : 0U;
    terminal_unlock(primask);
}

void terminal_inc_upload(void)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.upload_count++;
    terminal_unlock(primask);
}

void terminal_set_dht11_invalid(void)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.humidity_valid = 0U;
    terminal_unlock(primask);
}

/* --- 兼容旧代码 --- */

/* DHT11 温湿度 setter */
void terminal_set_dht11(uint8_t humidity, float temperature)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.humidity             = humidity;
    g_terminal_data.humidity_valid       = 1U;
    g_terminal_data.dht11_temperature    = temperature;
    terminal_unlock(primask);
}

/* --- 兼容旧代码 --- */

void terminal_set_local_value(uint16_t value)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.local_value = value;
    g_terminal_data.sample.filtered[ADC_CH_PB15] = value;
    terminal_unlock(primask);
}

/* 递增采样计数 */
void terminal_inc_sample_count(void)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.sample_count++;
    terminal_unlock(primask);
}

/* --- 全局快照 --- */

void terminal_get_snapshot(terminal_data_t *data)
{
    uint32_t primask;

    if (data == NULL) {
        return;
    }

    terminal_lock(&primask);
    *data = g_terminal_data;
    terminal_unlock(primask);
}

/* --- 告警回调 --- */

void terminal_set_alarm_callback(terminal_alarm_cb_t cb)
{
    g_alarm_cb = cb;
}

/* --- 系统运行时间 --- */

uint32_t terminal_get_uptime(void)
{
    uint32_t primask;
    uint32_t uptime;
    terminal_lock(&primask);
    uptime = g_terminal_data.uptime_seconds;
    terminal_unlock(primask);
    return uptime;
}

void terminal_inc_uptime(void)
{
    uint32_t primask;
    terminal_lock(&primask);
    g_terminal_data.uptime_seconds++;
    terminal_unlock(primask);
}
