#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <stdint.h>

#define MODBUS_OK              0
#define MODBUS_ERR_PARAM      -1
#define MODBUS_ERR_CRC        -2
#define MODBUS_ERR_FRAME      -3
#define MODBUS_ERR_EXCEPTION  -4

uint16_t modbus_build_read_holding_req(uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *tx_buf, uint16_t tx_buf_size);
uint16_t modbus_build_write_single_req(uint8_t slave_id, uint16_t reg_addr, uint16_t value, uint8_t *tx_buf, uint16_t tx_buf_size);
int modbus_parse_read_holding_resp(uint8_t slave_id, uint16_t quantity, const uint8_t *rx_buf, uint16_t rx_len, uint16_t *regs, uint16_t regs_size);
int modbus_parse_write_single_resp(uint8_t slave_id, uint16_t reg_addr, uint16_t value, const uint8_t *rx_buf, uint16_t rx_len);

#endif
