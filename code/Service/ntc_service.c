#include "ntc_service.h"
#include <math.h>

float ntc_convert_temperature(uint16_t adc_raw)
{
    float r_ntc, t_k;

    /* 边界保护：ADC 值接近 0 或 4095 说明传感器未连接或短路 */
    if (adc_raw <= 10 || adc_raw >= 4085) {
        return -273.15f;  /* 无效温度 */
    }

    /* 1. 计算 NTC 当前电阻 */
    r_ntc = NTC_R_FIXED / ((4095.0f / (float)adc_raw) - 1.0f);

    /* 2. B 值公式反推开尔文温度 */
    t_k = 1.0f / (1.0f / NTC_T0 + logf(r_ntc / NTC_R0) / NTC_B_VALUE);

    /* 3. 转摄氏度 */
    return t_k - 273.15f;
}

uint8_t ntc_is_connected(uint16_t adc_raw)
{
    return (adc_raw > 10 && adc_raw < 4085) ? 1U : 0U;
}
