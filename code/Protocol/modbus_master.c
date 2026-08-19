/**
 * @file    modbus_master.c
 * @brief   Modbus RTU master frame builder and parser
 *
 * Supports function codes: 0x03 (Read Holding Registers),
 * 0x06 (Write Single Register), 0x10 (Write Multiple Registers).
 * Includes CRC16 verification, exception code parsing, and
 * parameter validation.
 */

#include "modbus_master.h"
#include "crc16.h"
#include <stddef.h>

/**
 * @brief  Verify CRC16 of a received Modbus RTU frame.
 * @param  buf  Received frame buffer
 * @param  len  Total frame length (including CRC)
 * @return MODBUS_OK on match, MODBUS_ERR_CRC on mismatch, MODBUS_ERR_PARAM on invalid input
 */
static int modbus_check_crc(const uint8_t *buf, uint16_t len)
{
    uint16_t calc_crc;
    uint16_t recv_crc;

    /* Parameter validation: shortest Modbus frame is 4 bytes */
    if (buf == NULL || len < 4) {
        return MODBUS_ERR_PARAM;
    }

    /* Calculate CRC over all bytes except the last 2 (which are the CRC itself) */
    calc_crc = crc16_modbus(buf, (uint16_t)(len - 2));

    /* Extract received CRC (Modbus uses little-endian CRC byte order) */
    recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);

    return (calc_crc == recv_crc) ? MODBUS_OK : MODBUS_ERR_CRC;
}

/**
 * @brief  Build a Read Holding Registers (0x03) request frame.
 * @param  slave_id    Slave device address (1-247)
 * @param  start_addr  Starting register address
 * @param  quantity    Number of registers to read
 * @param  tx_buf      Output buffer for the request frame
 * @param  tx_buf_size Size of the output buffer
 * @return Frame length (always 8 bytes), or 0 on error
 */
uint16_t modbus_build_read_holding_req(uint8_t slave_id, uint16_t start_addr,
                                       uint16_t quantity, uint8_t *tx_buf,
                                       uint16_t tx_buf_size)
{
    uint16_t crc;

    if (tx_buf == NULL || tx_buf_size < 8 || quantity == 0) {
        return 0;
    }

    /* Build the 8-byte Modbus RTU request frame */
    tx_buf[0] = slave_id;                          /* Byte 0: Slave address */
    tx_buf[1] = 0x03;                              /* Byte 1: Function code (Read Holding Registers) */
    tx_buf[2] = (uint8_t)(start_addr >> 8);        /* Byte 2: Start address high */
    tx_buf[3] = (uint8_t)(start_addr & 0xFF);      /* Byte 3: Start address low */
    tx_buf[4] = (uint8_t)(quantity >> 8);          /* Byte 4: Quantity high */
    tx_buf[5] = (uint8_t)(quantity & 0xFF);        /* Byte 5: Quantity low */

    /* Append CRC16 (little-endian) */
    crc = crc16_modbus(tx_buf, 6);
    tx_buf[6] = (uint8_t)(crc & 0xFF);
    tx_buf[7] = (uint8_t)(crc >> 8);

    return 8;
}

/**
 * @brief  Build a Write Single Register (0x06) request frame.
 * @param  slave_id    Slave device address
 * @param  reg_addr    Register address to write
 * @param  value       Value to write
 * @param  tx_buf      Output buffer for the request frame
 * @param  tx_buf_size Size of the output buffer
 * @return Frame length (always 8 bytes), or 0 on error
 */
uint16_t modbus_build_write_single_req(uint8_t slave_id, uint16_t reg_addr,
                                       uint16_t value, uint8_t *tx_buf,
                                       uint16_t tx_buf_size)
{
    uint16_t crc;

    if (tx_buf == NULL || tx_buf_size < 8) {
        return 0;
    }

    /* Build the 8-byte Modbus RTU request frame */
    tx_buf[0] = slave_id;
    tx_buf[1] = 0x06;                              /* Function code: Write Single Register */
    tx_buf[2] = (uint8_t)(reg_addr >> 8);
    tx_buf[3] = (uint8_t)(reg_addr & 0xFF);
    tx_buf[4] = (uint8_t)(value >> 8);
    tx_buf[5] = (uint8_t)(value & 0xFF);

    crc = crc16_modbus(tx_buf, 6);
    tx_buf[6] = (uint8_t)(crc & 0xFF);
    tx_buf[7] = (uint8_t)(crc >> 8);

    return 8;
}

/**
 * @brief  Parse a Read Holding Registers (0x03) response frame.
 * @param  slave_id   Expected slave address
 * @param  quantity   Expected register count
 * @param  rx_buf     Received response buffer
 * @param  rx_len     Received data length
 * @param  regs       Output array for register values
 * @param  regs_size  Size of the output array
 * @return MODBUS_OK on success, or error code
 */
int modbus_parse_read_holding_resp(uint8_t slave_id, uint16_t quantity,
                                   const uint8_t *rx_buf, uint16_t rx_len,
                                   uint16_t *regs, uint16_t regs_size)
{
    uint8_t byte_count;
    uint16_t expected_len;

    /* Parameter validation */
    if (rx_buf == NULL || regs == NULL || quantity == 0 || regs_size < quantity) {
        return MODBUS_ERR_PARAM;
    }

    /* Expected response length: address(1) + func(1) + byte_count(1) + data(N*2) + CRC(2) */
    expected_len = (uint16_t)(5 + quantity * 2);
    if (rx_len < expected_len) {
        return MODBUS_ERR_FRAME;
    }

    /* Step 1: Verify CRC */
    if (modbus_check_crc(rx_buf, expected_len) != MODBUS_OK) {
        return MODBUS_ERR_CRC;
    }

    /* Step 2: Check slave address matches */
    if (rx_buf[0] != slave_id) {
        return MODBUS_ERR_FRAME;
    }

    /* Step 3: Check for exception response (function code MSB set) */
    if ((rx_buf[1] & 0x80) != 0) {
        return MODBUS_ERR_EXCEPTION;
    }

    /* Step 4: Verify function code is 0x03 */
    if (rx_buf[1] != 0x03) {
        return MODBUS_ERR_FRAME;
    }

    /* Step 5: Verify byte count matches expected */
    byte_count = rx_buf[2];
    if (byte_count != quantity * 2) {
        return MODBUS_ERR_FRAME;
    }

    /* Step 6: Extract register values (Modbus uses big-endian for register data) */
    for (uint16_t i = 0; i < quantity; i++) {
        regs[i] = ((uint16_t)rx_buf[3 + i * 2] << 8) | rx_buf[4 + i * 2];
    }

    return MODBUS_OK;
}

/**
 * @brief  Parse a Write Single Register (0x06) response frame.
 *
 * On success, the slave echoes back the request frame.
 * @return MODBUS_OK on success, or error code
 */
int modbus_parse_write_single_resp(uint8_t slave_id, uint16_t reg_addr,
                                   uint16_t value, const uint8_t *rx_buf,
                                   uint16_t rx_len)
{
    uint16_t resp_addr;
    uint16_t resp_value;

    if (rx_buf == NULL || rx_len < 8) {
        return MODBUS_ERR_PARAM;
    }

    /* Verify CRC */
    if (modbus_check_crc(rx_buf, 8) != MODBUS_OK) {
        return MODBUS_ERR_CRC;
    }

    /* Check slave address and function code */
    if (rx_buf[0] != slave_id || rx_buf[1] != 0x06) {
        return MODBUS_ERR_FRAME;
    }

    /* Extract echoed address and value */
    resp_addr  = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
    resp_value = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

    /* Verify echo matches what we sent */
    if (resp_addr != reg_addr || resp_value != value) {
        return MODBUS_ERR_FRAME;
    }

    return MODBUS_OK;
}

/* ================================================================
   Function Code 0x10: Write Multiple Holding Registers
   ================================================================ */

uint16_t modbus_build_write_multi_req(uint8_t slave_id, uint16_t start_addr,
                                      uint16_t quantity, const uint16_t *values,
                                      uint8_t *tx_buf, uint16_t tx_buf_size)
{
    uint16_t byte_count;
    uint16_t frame_len;
    uint16_t crc;

    if (tx_buf == NULL || values == NULL || quantity == 0) return 0;

    byte_count = (uint16_t)(quantity * 2);
    frame_len  = (uint16_t)(9 + byte_count);
    if (tx_buf_size < frame_len) return 0;

    tx_buf[0] = slave_id;
    tx_buf[1] = 0x10;
    tx_buf[2] = (uint8_t)(start_addr >> 8);
    tx_buf[3] = (uint8_t)(start_addr & 0xFF);
    tx_buf[4] = (uint8_t)(quantity >> 8);
    tx_buf[5] = (uint8_t)(quantity & 0xFF);
    tx_buf[6] = (uint8_t)byte_count;

    for (uint16_t i = 0; i < quantity; i++) {
        tx_buf[7 + i * 2]     = (uint8_t)(values[i] >> 8);
        tx_buf[8 + i * 2]     = (uint8_t)(values[i] & 0xFF);
    }

    crc = crc16_modbus(tx_buf, (uint16_t)(frame_len - 2));
    tx_buf[frame_len - 2] = (uint8_t)(crc & 0xFF);
    tx_buf[frame_len - 1] = (uint8_t)(crc >> 8);

    return frame_len;
}

/**
 * @brief  Parse a Write Multiple Registers (0x10) response.
 *
 * Response format: address(1) + 0x10(1) + start_addr(2) + quantity(2) + CRC(2) = 8 bytes
 */
int modbus_parse_write_multi_resp(uint8_t slave_id, uint16_t start_addr,
                                  uint16_t quantity, const uint8_t *rx_buf,
                                  uint16_t rx_len)
{
    uint16_t resp_addr, resp_quantity;
    if (rx_buf == NULL || rx_len < 8) return MODBUS_ERR_PARAM;
    if (modbus_check_crc(rx_buf, 8) != MODBUS_OK) return MODBUS_ERR_CRC;
    if (rx_buf[0] != slave_id) return MODBUS_ERR_FRAME;
    if ((rx_buf[1] & 0x80) != 0) return MODBUS_ERR_EXCEPTION;
    if (rx_buf[1] != 0x10) return MODBUS_ERR_FRAME;
    resp_addr     = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
    resp_quantity = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];
    if (resp_addr != start_addr || resp_quantity != quantity)
        return MODBUS_ERR_FRAME;
    return MODBUS_OK;
}

/**
 * @brief  Extract exception code from a Modbus exception response.
 * @param  rx_buf  Received response buffer
 * @param  rx_len  Received data length
 * @return Exception code (1-6), or 0 if no exception
 */
uint8_t modbus_get_exception_code(const uint8_t *rx_buf, uint16_t rx_len)
{
    if (rx_buf == NULL || rx_len < 3) return 0;
    if ((rx_buf[1] & 0x80) == 0) return 0;
    return rx_buf[2];
}
