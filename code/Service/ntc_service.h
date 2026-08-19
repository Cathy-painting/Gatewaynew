#ifndef NTC_SERVICE_H
#define NTC_SERVICE_H

#include <stdint.h>

/* ========================================================================
   NTC 热敏电阻温度采集模块

   硬件：NTC 10kΩ@25°C (B=3950K) + 10kΩ 分压电阻接 3.3V
   接线：PA0 ← NTC 与 10kΩ 分压电阻中点
         ┌─ 3.3V ─┐
         │         │
        10kΩ      NTC
         │         │
         └─ PA0 ──┘
              │
             GND

   公式：R_ntc = R_fixed / (4095/adc_raw - 1)
        T_K   = 1 / (1/T0 + ln(R_ntc/R0)/B)
        T_C   = T_K - 273.15
   ======================================================================== */

#define NTC_R_FIXED     10000.0f   /* 分压电阻 10kΩ */
#define NTC_R0          10000.0f   /* 25°C 时 NTC 电阻 */
#define NTC_T0          298.15f    /* 25°C = 298.15K */
#define NTC_B_VALUE     3950.0f    /* B 值 */

/* 将 NTC 通道的 ADC 原始值转换为温度（℃）
   adc_raw: 0-4095 的 ADC 原始值
   返回温度（℃），如果 adc_raw 接近 0 或 4095 说明传感器未连接 */
float ntc_convert_temperature(uint16_t adc_raw);

/* 检查 NTC 传感器是否已连接
   返回 1 已连接，0 未连接 */
uint8_t ntc_is_connected(uint16_t adc_raw);

#endif
