#include "bsp_dht11.h"
#include "log.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* DWT 周期计数器用于微秒级延时（80MHz = 12.5ns/cycle，80 cycle = 1us） */
#define DWT_DELAY_US(us)  do { \
    uint32_t _start = DWT->CYCCNT; \
    uint32_t _wait  = (uint32_t)((us) * (SystemCoreClock / 1000000UL)); \
    while ((DWT->CYCCNT - _start) < _wait) { } \
} while(0)

static void dht11_pin_out(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;  /* 开漏输出 */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

static void dht11_pin_in(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

static void dht11_pin_low(void)
{
    HAL_GPIO_WritePin(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_PIN_RESET);
}

static void dht11_pin_high(void)
{
    HAL_GPIO_WritePin(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_PIN_SET);
}

static uint8_t dht11_pin_read(void)
{
    return (HAL_GPIO_ReadPin(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}

/* 等待引脚变高，超时返回 0 */
static uint8_t dht11_wait_high(uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t wait  = (uint32_t)((timeout_us) * (SystemCoreClock / 1000000UL));
    while (!dht11_pin_read()) {
        if ((DWT->CYCCNT - start) > wait) return 0;
    }
    return 1;
}

/* 等待引脚变低，超时返回 0 */
static uint8_t dht11_wait_low(uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t wait  = (uint32_t)((timeout_us) * (SystemCoreClock / 1000000UL));
    while (dht11_pin_read()) {
        if ((DWT->CYCCNT - start) > wait) return 0;
    }
    return 1;
}

void bsp_dht11_init(void)
{
    /* 启用 DWT 周期计数器 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 启用 GPIOA 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 默认拉高 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
    dht11_pin_high();
}

uint8_t bsp_dht11_read(dht11_data_t *data)
{
    uint8_t buf[5] = {0};
    uint8_t i, j;

    if (data == NULL) return 0;
    data->valid = 0;

    /*
     * DHT11 单总线协议对时序要求严格（微秒级），FreeRTOS 任务切换会破坏时序。
     * 进入临界区：暂停任务调度，防止读取过程中被抢占。
     */
    taskENTER_CRITICAL();

    /* --- 步骤 1：MCU 发启动信号 --- */
    dht11_pin_out();
    dht11_pin_low();
    DWT_DELAY_US(18000);        /* 拉低 18ms */
    dht11_pin_high();
    DWT_DELAY_US(30);           /* 拉高 30us */

    /* --- 步骤 2：切换到输入，等待 DHT11 应答 --- */
    dht11_pin_in();

    /* DHT11 拉低 80us */
    if (!dht11_wait_low(100)) { taskEXIT_CRITICAL(); return 0; }
    uint32_t t_low_start = DWT->CYCCNT;
    if (!dht11_wait_high(100)) { taskEXIT_CRITICAL(); return 0; }
    /* 低电平持续时间 ≈ 80us 才有效 */
    if ((DWT->CYCCNT - t_low_start) < (40UL * (SystemCoreClock / 1000000UL))) {
        taskEXIT_CRITICAL(); return 0;
    }

    /* DHT11 拉高 80us */
    if (!dht11_wait_low(100)) { taskEXIT_CRITICAL(); return 0; }

    /* --- 步骤 3：读 40bit 数据 --- */
    /*
     * DHT11 每 bit 时序：50us 低电平 → 26-28us 高电平(bit=0) / 70us 高电平(bit=1)
     *
     * BUG FIX: 原代码在 wait_low 之后才记录 t_high_start，实际测量的是
     * 下一个 bit 的低电平持续时间（始终 50us），而不是当前 bit 的高电平持续时间。
     *
     * 正确做法：wait_high 成功后立即记录开始时间，然后 wait_low 等待高电平结束，
     * 两次之间的时间差才是高电平持续时间。
     */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 8; j++) {
            /* 等待当前 bit 的低电平结束（50us），引脚变高 */
            if (!dht11_wait_high(80)) { taskEXIT_CRITICAL(); return 0; }

            /* 开始测量高电平持续时间 */
            uint32_t t_high_start = DWT->CYCCNT;

            /* 等待高电平结束，引脚变低 */
            if (!dht11_wait_low(100)) { taskEXIT_CRITICAL(); return 0; }

            uint32_t t_high = DWT->CYCCNT - t_high_start;

            buf[i] <<= 1;
            /* 高电平 > 55us 判定为 bit 1，否则 bit 0 */
            if (t_high > (55UL * (SystemCoreClock / 1000000UL))) {
                buf[i] |= 1;
            }
        }
    }

    taskEXIT_CRITICAL();

    /* --- 步骤 4：校验 --- */
    if (((uint16_t)buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) return 0;

    data->humidity     = buf[0];
    data->humidity_dec = buf[1];
    data->temperature  = buf[2];
    data->temp_dec     = buf[3];
    data->checksum     = buf[4];
    data->valid        = 1;

    return 1;
}
