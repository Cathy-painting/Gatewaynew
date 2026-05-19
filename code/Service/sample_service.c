// 1. 引入采样服务的头文件（声明本函数，给外部调用）
#include "sample_service.h"

// 先跑通项目主线：本地采样先用假数据（0~4095 循环）
uint16_t sample_service_read_local(void)
{
    static uint16_t fake_value = 0;

    fake_value = (uint16_t)(fake_value + 10);
    if (fake_value > 4095) {
        fake_value = 0;
    }

    return fake_value;
}
