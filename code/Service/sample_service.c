#include "sample_service.h"
#include "bsp_adc.h"

uint16_t sample_service_read_local(void)
{
    return bsp_adc_read();
}
