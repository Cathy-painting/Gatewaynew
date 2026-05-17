// 1. 引入采样服务的头文件（声明本函数，给外部调用）
#include "sample_service.h"
// 2. 引入底层ADC硬件驱动头文件（操作硬件的工具）
#include "bsp_adc.h"


// 3. 定义函数：读取本地采样值
// 返回值：uint16_t = 16位无符号整数（ADC采集出来的数字值）
uint16_t sample_service_read_local(void)
{
    // 4. 直接调用底层ADC读取函数，把结果返回
    return bsp_adc_read();
}
