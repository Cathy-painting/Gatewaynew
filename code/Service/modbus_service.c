#include "modbus_service.h"
#include "terminal_service.h"
#include "modbus_master.h"
#include "bsp_rs485.h"
#include "bsp_uart.h"
#include "log.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* 显式 extern 声明，避免 #70 incomplete type 错误 */
extern uint8_t uart3_rx_buf[256];
extern uint16_t uart3_rx_len;

#define MODBUS_SLAVE_ID      1
#define MODBUS_START_ADDR    0
#define MODBUS_QUANTITY      2
#define MODBUS_TIMEOUT_MS    200
#define MODBUS_MAX_FAIL      3

static uint32_t modbus_fail_count = 0;
static uint8_t  modbus_online     = 0;

void modbus_service_poll_once(void)
{
    uint8_t  tx_buf[8];
    uint16_t tx_len;
    uint16_t regs[MODBUS_QUANTITY];
    char     log_buf[128];
    int      result;

    tx_len = modbus_build_read_holding_req(MODBUS_SLAVE_ID,
                                           MODBUS_START_ADDR,
                                           MODBUS_QUANTITY,
                                           tx_buf, sizeof(tx_buf));
    if (tx_len == 0)
    {
        log_info("[MODBUS] build request failed\r\n");
        return;
    }

    snprintf(log_buf, sizeof(log_buf),
             "[MODBUS] tx: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
             tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3],
             tx_buf[4], tx_buf[5], tx_buf[6], tx_buf[7]);
    log_info(log_buf);

    uart3_rx_len = 0;
    memset(uart3_rx_buf, 0, sizeof(uart3_rx_buf));

    bsp_rs485_send(tx_buf, tx_len);

    osDelay(MODBUS_TIMEOUT_MS);

    if (uart3_rx_len == 0)
    {
        modbus_fail_count++;
        terminal_inc_modbus_fail();
        snprintf(log_buf, sizeof(log_buf),
                 "[MODBUS] timeout fail=%lu\r\n",
                 (unsigned long)modbus_fail_count);
        log_info(log_buf);
    }
    else
    {
        result = modbus_parse_read_holding_resp(MODBUS_SLAVE_ID,
                                                MODBUS_QUANTITY,
                                                uart3_rx_buf,
                                                uart3_rx_len,
                                                regs, sizeof(regs) / sizeof(regs[0]));

        if (result == MODBUS_OK)
        {
            terminal_set_remote_value(regs[0]);
            terminal_set_remote_online(1);
            terminal_inc_modbus_ok();
            modbus_fail_count = 0;
            modbus_online     = 1;
            snprintf(log_buf, sizeof(log_buf),
                     "[MODBUS] rx ok reg0=%u reg1=%u\r\n",
                     regs[0], regs[1]);
            log_info(log_buf);
        }
        else
        {
            modbus_fail_count++;
            terminal_inc_modbus_fail();
            snprintf(log_buf, sizeof(log_buf),
                     "[MODBUS] rx error code=%d fail=%lu\r\n",
                     result, (unsigned long)modbus_fail_count);
            log_info(log_buf);
        }
    }

    if (modbus_fail_count >= MODBUS_MAX_FAIL)
    {
        if (modbus_online)
        {
            modbus_online = 0;
            terminal_set_remote_online(0);
            log_info("[MODBUS] device offline\r\n");
        }
    }
    else
    {
        if (!modbus_online && modbus_fail_count == 0)
        {
            log_info("[MODBUS] device online\r\n");
        }
    }
}
