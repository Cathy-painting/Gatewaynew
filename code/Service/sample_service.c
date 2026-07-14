#include "sample_service.h"
#include "bsp_adc.h"
#include "terminal_service.h"
#include "log.h"
#include <stdio.h>

uint16_t sample_service_read_local(void)
{
    return bsp_adc_read();
}

void sample_service_process(void)
{
    uint16_t adc_value = sample_service_read_local();

    terminal_set_local_value(adc_value);
    log_infof("[SAMPLE] local=%u\r\n", adc_value);
}
