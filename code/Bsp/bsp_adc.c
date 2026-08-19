#include "bsp_adc.h"
#include "adc.h"
#include "log.h"

/* 使用软件触发 + 轮询读通道，避免 DMA 未跑通时一直读到 0。
 * 采样周期 200ms，轮询足够快，联调更可靠。 */

static uint8_t adc_ready = 0U;

static uint16_t adc_read_one(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t value = 0U;

    if (adc_ready == 0U) {
        return 0U;
    }

    sConfig.Channel      = channel;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;

    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) {
        return 0U;
    }

    if (HAL_ADC_Start(&hadc2) != HAL_OK) {
        return 0U;
    }

    if (HAL_ADC_PollForConversion(&hadc2, 20U) == HAL_OK) {
        value = (uint16_t)HAL_ADC_GetValue(&hadc2);
        if (value > 4095U) {
            value = 4095U;
        }
    }

    (void)HAL_ADC_Stop(&hadc2);
    return value;
}

void bsp_adc_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    ADC_ChannelConfTypeDef sConfig = {0};

    /* PB15 / PB12 / PA0 → 模拟输入 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin  = GPIO_PIN_15 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 重新配置为单通道软件触发（轮询），最稳妥 */
    (void)HAL_ADC_Stop(&hadc2);

    hadc2.Instance                   = ADC2;
    hadc2.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc2.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc2.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc2.Init.GainCompensation      = 0;
    hadc2.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc2.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc2.Init.LowPowerAutoWait      = DISABLE;
    hadc2.Init.ContinuousConvMode    = DISABLE;
    hadc2.Init.NbrOfConversion       = 1;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc2.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc2.Init.DMAContinuousRequests = DISABLE;
    hadc2.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc2.Init.OversamplingMode      = DISABLE;

    if (HAL_ADC_Init(&hadc2) != HAL_OK) {
        log_info("[ADC] HAL_ADC_Init failed\r\n");
        adc_ready = 0U;
        return;
    }

    sConfig.Channel      = ADC_CHANNEL_15;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) {
        log_info("[ADC] config failed\r\n");
        adc_ready = 0U;
        return;
    }

    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
        log_info("[ADC] calibration failed\r\n");
    }

    adc_ready = 1U;
    log_info("[ADC] init OK: poll mode PB15+PB12+PA0\r\n");
}

void bsp_adc_start(void)
{
    /* 轮询模式无需启动 DMA */
}

void bsp_adc_stop(void)
{
    (void)HAL_ADC_Stop(&hadc2);
}

uint16_t bsp_adc_get_raw(uint8_t ch)
{
    switch (ch) {
    case BSP_ADC_CH_PB15:
        return adc_read_one(ADC_CHANNEL_15);
    case BSP_ADC_CH_PB12:
        return adc_read_one(ADC_CHANNEL_12);
    case BSP_ADC_CH_NTC:
        return adc_read_one(ADC_CHANNEL_1); /* PA0 = ADC2_IN1 */
    default:
        return 0U;
    }
}
