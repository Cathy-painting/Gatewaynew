#include "modbus_service.h"
#include "terminal_service.h"
#include "modbus_master.h"
#include "event_log.h"
#include "bsp_rs485.h"
#include "bsp_uart.h"
#include "log.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

/* --- 多从站轮询配置 --- */
/* 目前配置 1 个从站，可通过修改 slave_list 增加更多 */
static const uint8_t slave_list[] = {1};
#define MODBUS_SLAVE_COUNT  (sizeof(slave_list) / sizeof(slave_list[0]))

#define MODBUS_START_ADDR    0U
#define MODBUS_QUANTITY      2U
#define MODBUS_TIMEOUT_MS    200U
#define MODBUS_MAX_FAIL      3U
#define MODBUS_RX_BUF_SIZE   256U

/* 3.5 字符间隔：根据波特率动态计算
   Modbus RTU 帧间隔 ≥ 3.5 字符时间
   115200bps: 3.5 × 11 / 115200 ≈ 0.33ms → 向上取整 +1ms 余量 = 2ms
   9600bps:   3.5 × 11 / 9600  ≈ 4.0ms  → 向上取整 +1ms 余量 = 6ms */
#define MODBUS_BAUDRATE       115200U
#define MODBUS_CHAR_BITS      11U       /* 1 start + 8 data + 1 parity + 1 stop */
#define MODBUS_T35_US         ((3U * MODBUS_CHAR_BITS * 1000000UL) / MODBUS_BAUDRATE + \
                               (MODBUS_CHAR_BITS * 500000UL) / MODBUS_BAUDRATE)
#define MODBUS_FRAME_IDLE_MS  ((MODBUS_T35_US + 999U) / 1000U + 1U)

/* 轮询检查间隔（ms） */
#define MODBUS_POLL_INTERVAL_MS  1U

static uint8_t  current_slave_index = 0U;
static uint8_t  slave_online[MODBUS_MAX_SLAVES] = {0};
static uint8_t  slave_fail_streak[MODBUS_MAX_SLAVES] = {0};

/* USART3/RS485 互斥锁：modbusTask 和 cmdTask 共享 USART3 */
static osMutexId_t g_uart3_mutex = NULL;

/* 等待响应：数据长度稳定 IDLE_MS 后认为一帧结束 */
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
        osDelay(MODBUS_POLL_INTERVAL_MS);
    }

    if (cur_len > 0U) {
        return bsp_uart_copy_rx_buffer(&huart3, rx_buf, rx_buf_size);
    }
    return 0U;
}

/* 标记从站成功 */
static void modbus_mark_success(uint8_t slave_id)
{
    uint8_t idx = 0;
    for (uint8_t i = 0; i < MODBUS_SLAVE_COUNT; i++) {
        if (slave_list[i] == slave_id) { idx = i; break; }
    }

    slave_fail_streak[idx] = 0U;
    if (slave_online[idx] == 0U) {
        log_infof("[MODBUS] slave %u online\r\n", slave_id);
        event_log_add(EVENT_MODBUS_ONLINE, slave_id);
    }
    slave_online[idx] = 1U;
    terminal_set_remote_online(1U);
    terminal_inc_modbus_ok();
}

/* 标记从站失败 */
static void modbus_mark_fail(uint8_t slave_id, const char *reason, uint8_t exc_code)
{
    uint8_t idx = 0;
    for (uint8_t i = 0; i < MODBUS_SLAVE_COUNT; i++) {
        if (slave_list[i] == slave_id) { idx = i; break; }
    }

    terminal_inc_modbus_fail();
    if (slave_fail_streak[idx] < 255U) {
        slave_fail_streak[idx]++;
    }

    if (exc_code != 0) {
        const char *exc_name;
        switch (exc_code) {
            case MODBUS_EXC_ILLEGAL_FUNCTION: exc_name = "ILLEGAL_FUNC";  break;
            case MODBUS_EXC_ILLEGAL_ADDRESS:  exc_name = "ILLEGAL_ADDR";  break;
            case MODBUS_EXC_ILLEGAL_VALUE:    exc_name = "ILLEGAL_VAL";   break;
            case MODBUS_EXC_SLAVE_FAILURE:    exc_name = "SLAVE_FAIL";    break;
            case MODBUS_EXC_ACKNOWLEDGE:      exc_name = "ACK";           break;
            case MODBUS_EXC_BUSY:             exc_name = "BUSY";          break;
            default:                          exc_name = "?";             break;
        }
        log_infof("[MODBUS] slave %u %s exception=0x%02X(%s) fail=%u\r\n",
                  slave_id, reason, exc_code, exc_name, slave_fail_streak[idx]);
    } else {
        log_infof("[MODBUS] slave %u %s fail=%u\r\n",
                  slave_id, reason, slave_fail_streak[idx]);
    }

    if (slave_fail_streak[idx] >= MODBUS_MAX_FAIL) {
        if (slave_online[idx] != 0U) {
            log_infof("[MODBUS] slave %u offline\r\n", slave_id);
            event_log_add(EVENT_MODBUS_OFFLINE, slave_id);
        }
        slave_online[idx] = 0U;
        terminal_set_remote_online(0U);
        terminal_set_remote_value(0U);  /* 离线时清零旧数据 */
    }
}

void modbus_service_init(void)
{
    current_slave_index = 0U;
    for (uint8_t i = 0; i < MODBUS_MAX_SLAVES; i++) {
        slave_online[i] = 0U;
        slave_fail_streak[i] = 0U;
    }
    terminal_set_remote_online(0U);

    /* 创建 USART3/RS485 互斥锁 */
    g_uart3_mutex = osMutexNew(NULL);
    if (g_uart3_mutex == NULL) {
        log_info("[MODBUS] UART3 mutex create failed\r\n");
    }
}

/* 单次轮询：轮询到 slave_list 中的当前从站，读完数据后切换到下一个 */
void modbus_service_poll_once(void)
{
    uint8_t  slave_id = slave_list[current_slave_index];
    uint8_t  tx_buf[8];
    uint8_t  rx_buf[MODBUS_RX_BUF_SIZE];
    uint16_t tx_len, rx_len;
    uint16_t regs[MODBUS_QUANTITY];
    int      result;

    /* 获取 RS485 互斥锁 */
    if (g_uart3_mutex != NULL) {
        if (osMutexAcquire(g_uart3_mutex, MODBUS_TIMEOUT_MS) != osOK) {
            return;  /* 获取锁失败，跳过本次轮询 */
        }
    }

    memset(rx_buf, 0, sizeof(rx_buf));

    /* 构建 0x03 读保持寄存器请求 */
    tx_len = modbus_build_read_holding_req(slave_id, MODBUS_START_ADDR,
                                           MODBUS_QUANTITY, tx_buf, sizeof(tx_buf));
    if (tx_len == 0U) {
        log_info("[MODBUS] build 03 request failed\r\n");
        goto unlock_next;
    }

    log_hex("[MODBUS] 03 tx: ", tx_buf, tx_len);
    bsp_uart_clear_rx_buffer(&huart3);
    bsp_rs485_send(tx_buf, tx_len);

    /* 等待响应 */
    rx_len = modbus_wait_response(rx_buf, sizeof(rx_buf), MODBUS_TIMEOUT_MS);
    if (rx_len == 0U) {
        modbus_mark_fail(slave_id, "timeout", 0);
        bsp_uart_clear_rx_buffer(&huart3);  /* 超时清空残留数据 */
        goto unlock_next;
    }

    log_hex("[MODBUS] 03 rx: ", rx_buf, rx_len);

    /* 解析响应 */
    result = modbus_parse_read_holding_resp(slave_id, MODBUS_QUANTITY,
                                            rx_buf, rx_len, regs,
                                            (uint16_t)(sizeof(regs) / sizeof(regs[0])));
    if (result == MODBUS_OK) {
        terminal_set_remote_value(regs[0]);
        terminal_set_remote_value2(regs[1]);
        modbus_mark_success(slave_id);
        log_infof("[MODBUS] slave %u 03 ok reg0=%u reg1=%u\r\n", slave_id, regs[0], regs[1]);
    } else if (result == MODBUS_ERR_EXCEPTION) {
        uint8_t exc = modbus_get_exception_code(rx_buf, rx_len);
        modbus_mark_fail(slave_id, "exception", exc);
    } else {
        log_infof("[MODBUS] 03 parse error=%d\r\n", result);
        modbus_mark_fail(slave_id, "frame", 0);
    }

unlock_next:
    /* 释放 RS485 互斥锁 */
    if (g_uart3_mutex != NULL) {
        osMutexRelease(g_uart3_mutex);
    }

    /* 切换到下一个从站 */
    current_slave_index++;
    if (current_slave_index >= MODBUS_SLAVE_COUNT) {
        current_slave_index = 0U;
    }
}

uint8_t modbus_service_write_single(uint16_t reg_addr, uint16_t value)
{
    uint8_t  slave_id = slave_list[0];  /* 默认写第一个从站 */
    uint8_t  tx_buf[8];
    uint8_t  rx_buf[MODBUS_RX_BUF_SIZE];
    uint16_t tx_len, rx_len;
    int      result;

    /* 获取 RS485 互斥锁 */
    if (g_uart3_mutex != NULL) {
        if (osMutexAcquire(g_uart3_mutex, MODBUS_TIMEOUT_MS) != osOK) {
            return 0U;
        }
    }

    memset(rx_buf, 0, sizeof(rx_buf));
    tx_len = modbus_build_write_single_req(slave_id, reg_addr, value, tx_buf, sizeof(tx_buf));
    if (tx_len == 0U) {
        log_info("[MODBUS] build 06 request failed\r\n");
        if (g_uart3_mutex != NULL) osMutexRelease(g_uart3_mutex);
        return 0U;
    }

    log_hex("[MODBUS] 06 tx: ", tx_buf, tx_len);
    bsp_uart_clear_rx_buffer(&huart3);
    bsp_rs485_send(tx_buf, tx_len);

    rx_len = modbus_wait_response(rx_buf, sizeof(rx_buf), MODBUS_TIMEOUT_MS);
    if (rx_len == 0U) {
        modbus_mark_fail(slave_id, "write-timeout", 0);
        bsp_uart_clear_rx_buffer(&huart3);  /* 超时清空残留数据 */
        if (g_uart3_mutex != NULL) osMutexRelease(g_uart3_mutex);
        return 0U;
    }

    log_hex("[MODBUS] 06 rx: ", rx_buf, rx_len);
    result = modbus_parse_write_single_resp(slave_id, reg_addr, value, rx_buf, rx_len);
    if (result == MODBUS_OK) {
        modbus_mark_success(slave_id);
        log_info("[MODBUS] 06 ok\r\n");
        if (g_uart3_mutex != NULL) osMutexRelease(g_uart3_mutex);
        return 1U;
    }

    log_infof("[MODBUS] 06 parse error=%d\r\n", result);
    modbus_mark_fail(slave_id, "write-frame", 0);
    if (g_uart3_mutex != NULL) osMutexRelease(g_uart3_mutex);
    return 0U;
}
