#include "dht11_service.h"
#include "terminal_service.h"
#include "log.h"
#include "cmsis_os.h"

/* DHT11 最小采样间隔约 1s，过快读取会失败并长时间占用总线 */
#define DHT11_MIN_INTERVAL_MS  1500U

void dht11_service_init(void)
{
    bsp_dht11_init();
    log_info("[DHT11] init OK, PA1 ready\r\n");
}

void dht11_service_read(void)
{
    static uint32_t last_ms = 0U;
    uint32_t now = osKernelGetTickCount();
    dht11_data_t data;

    if ((now - last_ms) < DHT11_MIN_INTERVAL_MS) {
        return;
    }
    last_ms = now;

    if (!bsp_dht11_read(&data)) {
        terminal_set_dht11_invalid();
        return;
    }

    terminal_set_dht11(data.humidity, (float)data.temperature);
    log_infof("[DHT11] humidity=%u%% temp=%uC\r\n", data.humidity, data.temperature);
}
