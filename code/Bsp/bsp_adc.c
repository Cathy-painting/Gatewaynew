#include "bsp_adc.h"
#include "stm32g4xx_hal_ADC.h"
#include "adc.h"

//因为要返回0-4095的值所以是uint16_t
uint16_t bsp_adc_read(void)
{
	#ifdef HAL_ADC_MODULE_ENABLED
	 uint16_t value=0;
	//如果启动成功
	if(HAL_ADC_Start(&hadc2) != HAL_OK){
		 return 0;
	}
	if(HAL_ADC_PollForConversion(&hadc2,20) == HAL_OK)
	{
		value = (uint16_t)HAL_ADC_GetValue(&hadc2);
	}
	HAL_ADC_Stop(&hadc2);
	return value;
	#else
	return 0;
	#endif	
}
