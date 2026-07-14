
#include "modbus_service.h"
#include "terminal_service.h"
#include "modbus_master.h"
#include "bsp_rs485.h"
#include "bsp_uart.h"
#include "log.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define MODBUS_SLAVE_ID      1U
#define MODBUS_START_ADDR    0U
#define MODBUS_QUANTITY      2U
#define MODBUS_TIMEOUT_MS    200U
#define MODBUS_MAX_FAIL      3U
#define MODBUS_RX_BUF_SIZE   256U
#define MODBUS_FRAME_IDLE_MS 20U

static uint8_t modbus_online = 0U;
static uint8_t modbus_fail_streak = 0U;

static uint16_t modbus_wait_response(uint8_t *rx_buf, uint16_t rx_buf_size, uint32_t timeout_ms)
{
    uint32_t start_tick = osKernelGetTickCount();
    uint16_t last_len = 0U;
    uint16_t cur_len = 0U;
    uint32_t stable_tick = 0U;

    while ((osKernelGetTickCount() - start_tick) < timeout_ms) {
        cur_len = bsp_uart_get_rx_length(&huart3);
        if (cur_len > 0U) {
            if (cur_len == last_len) {
                if ((osKernelGetTickCount() - stable_tick) >= MODBUS_FRAME_IDLE_MS) {
                    return bsp_uart_copy_rx_buffer(&huart3, rx_buf, rx_buf_size);
                }
            } else {
                last_len = cur_len;
                stable_tick = osKernelGetTickCount();
            }
        }
        osDelay(5);
    }

    if (cur_len > 0U) {
        return bsp_uart_copy_rx_buffer(&huart3, rx_buf, rx_buf_size);
    }
    return 0U;
}

static void modbus_mark_success(void)
{
    modbus_fail_streak = 0U;
    if (modbus_online == 0U) {
        log_info("[MODBUS] device online\r\n");
    }
    modbus_online = 1U;
    terminal_set_remote_online(1U);
    terminal_inc_modbus_ok();
}

static void modbus_mark_fail(const char *reason)
{
    terminal_inc_modbus_fail();
    if (modbus_fail_streak < 255U) {
        modbus_fail_streak++;
    }

    log_infof("[MODBUS] %s fail=%u\r\n", reason, modbus_fail_streak);

    if (modbus_fail_streak >= MODBUS_MAX_FAIL) {
        if (modbus_online != 0U) {
            log_info("[MODBUS] device offline\r\n");
        }
        modbus_online = 0U;
        terminal_set_remote_online(0U);
    }
}

void modbus_service_init(void)
{
    modbus_online = 0U;
    modbus_fail_streak = 0U;
    terminal_set_remote_online(0U);
}

void modbus_service_poll_once(void)
{
    uint8_t tx_buf[8];
    uint8_t rx_buf[MODBUS_RX_BUF_SIZE];
    uint16_t tx_len;
    uint16_t rx_len;
    uint16_t regs[MODBUS_QUANTITY];
    int result;

    memset(rx_buf, 0, sizeof(rx_buf));
    tx_len = modbus_build_read_holding_req(MODBUS_SLAVE_ID,
                                           MODBUS_START_ADDR,
                                           MODBUS_QUANTITY,
                                           tx_buf,
                                           sizeof(tx_buf));
    if (tx_len == 0U) {
        log_info("[MODBUS] build 03 request failed\r\n");
        return;
    }

    log_hex("[MODBUS] 03 tx: ", tx_buf, tx_len);
    bsp_uart_clear_rx_buffer(&huart3);
    bsp_rs485_send(tx_buf, tx_len);

    rx_len = modbus_wait_response(rx_buf, sizeof(rx_buf), MODBUS_TIMEOUT_MS);
    if (rx_len == 0U) {
        modbus_mark_fail("timeout");
        return;
    }

    log_hex("[MODBUS] 03 rx: ", rx_buf, rx_len);
    result = modbus_parse_read_holding_resp(MODBUS_SLAVE_ID,
                                            MODBUS_QUANTITY,
                                            rx_buf,
                                            rx_len,
                                            regs,
                                            (uint16_t)(sizeof(regs) / sizeof(regs[0])));
    if (result == MODBUS_OK) {
        terminal_set_remote_value(regs[0]);
        modbus_mark_success();
        log_infof("[MODBUS] 03 ok reg0=%u reg1=%u\r\n", regs[0], regs[1]);
    } else {
        log_infof("[MODBUS] 03 parse error=%d\r\n", result);
        modbus_mark_fail("frame");
    }
}

uint8_t modbus_service_write_single(uint16_t reg_addr, uint16_t value)
{
    uint8_t tx_buf[8];
    uint8_t rx_buf[MODBUS_RX_BUF_SIZE];
    uint16_t tx_len;
    uint16_t rx_len;
    int result;

    memset(rx_buf, 0, sizeof(rx_buf));
    tx_len = modbus_build_write_single_req(MODBUS_SLAVE_ID,
                                           reg_addr,
                                           value,
                                           tx_buf,
                                           sizeof(tx_buf));
    if (tx_len == 0U) {
        log_info("[MODBUS] build 06 request failed\r\n");
        return 0U;
    }

    log_hex("[MODBUS] 06 tx: ", tx_buf, tx_len);
    bsp_uart_clear_rx_buffer(&huart3);
    bsp_rs485_send(tx_buf, tx_len);

    rx_len = modbus_wait_response(rx_buf, sizeof(rx_buf), MODBUS_TIMEOUT_MS);
    if (rx_len == 0U) {
        modbus_mark_fail("write-timeout");
        return 0U;
    }

    log_hex("[MODBUS] 06 rx: ", rx_buf, rx_len);
    result = modbus_parse_write_single_resp(MODBUS_SLAVE_ID, reg_addr, value, rx_buf, rx_len);
    if (result == MODBUS_OK) {
        modbus_mark_success();
        log_info("[MODBUS] 06 ok\r\n");
        return 1U;
    }

    log_infof("[MODBUS] 06 parse error=%d\r\n", result);
    modbus_mark_fail("write-frame");
    return 0U;                                                                                          
}
