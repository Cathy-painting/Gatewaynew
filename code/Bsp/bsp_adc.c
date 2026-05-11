#include "bsp_adc.h"
#include "stm32g4xx_hal_adc.h"

uint16_t bsp_adc_read(void)
{
#ifdef HAL_ADC_MODULE_ENABLED
    uint16_t value = 0;

    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return 0;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 20) == HAL_OK) {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);
    return value;
#else
    return 0;
#endif
}
