#include "modbus_master.h"
#include "crc16.h"

static int modbus_check_crc(const uint8_t *buf, uint16_t len)
{
    uint16_t calc_crc;
    uint16_t recv_crc;

    if (buf == 0 || len < 4) {
        return MODBUS_ERR_PARAM;
    }

    calc_crc = crc16_modbus(buf, (uint16_t)(len - 2));
    recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);

    return (calc_crc == recv_crc) ? MODBUS_OK : MODBUS_ERR_CRC;
}

uint16_t modbus_build_read_holding_req(uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *tx_buf, uint16_t tx_buf_size)
{
    uint16_t crc;

    if (tx_buf == 0 || tx_buf_size < 8 || quantity == 0) {
        return 0;
    }

    tx_buf[0] = slave_id;
    tx_buf[1] = 0x03;
    tx_buf[2] = (uint8_t)(start_addr >> 8);
    tx_buf[3] = (uint8_t)(start_addr & 0xFF);
    tx_buf[4] = (uint8_t)(quantity >> 8);
    tx_buf[5] = (uint8_t)(quantity & 0xFF);

    crc = crc16_modbus(tx_buf, 6);
    tx_buf[6] = (uint8_t)(crc & 0xFF);
    tx_buf[7] = (uint8_t)(crc >> 8);

    return 8;
}

uint16_t modbus_build_write_single_req(uint8_t slave_id, uint16_t reg_addr, uint16_t value, uint8_t *tx_buf, uint16_t tx_buf_size)
{
    uint16_t crc;

    if (tx_buf == 0 || tx_buf_size < 8) {
        return 0;
    }

    tx_buf[0] = slave_id;
    tx_buf[1] = 0x06;
    tx_buf[2] = (uint8_t)(reg_addr >> 8);
    tx_buf[3] = (uint8_t)(reg_addr & 0xFF);
    tx_buf[4] = (uint8_t)(value >> 8);
    tx_buf[5] = (uint8_t)(value & 0xFF);

    crc = crc16_modbus(tx_buf, 6);
    tx_buf[6] = (uint8_t)(crc & 0xFF);
    tx_buf[7] = (uint8_t)(crc >> 8);

    return 8;
}

int modbus_parse_read_holding_resp(uint8_t slave_id, uint16_t quantity, const uint8_t *rx_buf, uint16_t rx_len, uint16_t *regs, uint16_t regs_size)
{
    uint8_t byte_count;
    uint16_t expected_len;

    if (rx_buf == 0 || regs == 0 || quantity == 0 || regs_size < quantity) {
        return MODBUS_ERR_PARAM;
    }

    expected_len = (uint16_t)(5 + quantity * 2);
    if (rx_len < expected_len) {
        return MODBUS_ERR_FRAME;
    }

    if (modbus_check_crc(rx_buf, expected_len) != MODBUS_OK) {
        return MODBUS_ERR_CRC;
    }

    if (rx_buf[0] != slave_id) {
        return MODBUS_ERR_FRAME;
    }

    if ((rx_buf[1] & 0x80) != 0) {
        return MODBUS_ERR_EXCEPTION;
    }

    if (rx_buf[1] != 0x03) {
        return MODBUS_ERR_FRAME;
    }

    byte_count = rx_buf[2];
    if (byte_count != quantity * 2) {
        return MODBUS_ERR_FRAME;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        regs[i] = ((uint16_t)rx_buf[3 + i * 2] << 8) | rx_buf[4 + i * 2];
    }

    return MODBUS_OK;
}

int modbus_parse_write_single_resp(uint8_t slave_id, uint16_t reg_addr, uint16_t value, const uint8_t *rx_buf, uint16_t rx_len)
{
    uint16_t resp_addr;
    uint16_t resp_value;

    if (rx_buf == 0 || rx_len < 8) {
        return MODBUS_ERR_PARAM;
    }

    if (modbus_check_crc(rx_buf, 8) != MODBUS_OK) {
        return MODBUS_ERR_CRC;
    }

    if (rx_buf[0] != slave_id || rx_buf[1] != 0x06) {
        return MODBUS_ERR_FRAME;
    }

    resp_addr = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
    resp_value = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

    if (resp_addr != reg_addr || resp_value != value) {
        return MODBUS_ERR_FRAME;
    }

    return MODBUS_OK;
}
