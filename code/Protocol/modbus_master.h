#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#include <stdint.h>

/* Return codes */
#define MODBUS_OK              0
#define MODBUS_ERR_PARAM      -1
#define MODBUS_ERR_CRC        -2
#define MODBUS_ERR_FRAME      -3
#define MODBUS_ERR_EXCEPTION  -4

/* Exception codes (when function code MSB is set in response) */
#define MODBUS_EXC_ILLEGAL_FUNCTION   0x01
#define MODBUS_EXC_ILLEGAL_ADDRESS    0x02
#define MODBUS_EXC_ILLEGAL_VALUE      0x03
#define MODBUS_EXC_SLAVE_FAILURE      0x04
#define MODBUS_EXC_ACKNOWLEDGE        0x05
#define MODBUS_EXC_BUSY               0x06

/* Function Code 0x03: Read Holding Registers */
uint16_t modbus_build_read_holding_req(uint8_t slave_id, uint16_t start_addr,
                                       uint16_t quantity, uint8_t *tx_buf,
                                       uint16_t tx_buf_size);
int modbus_parse_read_holding_resp(uint8_t slave_id, uint16_t quantity,
                                   const uint8_t *rx_buf, uint16_t rx_len,
                                   uint16_t *regs, uint16_t regs_size);

/* Function Code 0x06: Write Single Register */
uint16_t modbus_build_write_single_req(uint8_t slave_id, uint16_t reg_addr,
                                       uint16_t value, uint8_t *tx_buf,
                                       uint16_t tx_buf_size);
int modbus_parse_write_single_resp(uint8_t slave_id, uint16_t reg_addr,
                                   uint16_t value, const uint8_t *rx_buf,
                                   uint16_t rx_len);

/* Function Code 0x10: Write Multiple Registers */
uint16_t modbus_build_write_multi_req(uint8_t slave_id, uint16_t start_addr,
                                      uint16_t quantity, const uint16_t *values,
                                      uint8_t *tx_buf, uint16_t tx_buf_size);
int modbus_parse_write_multi_resp(uint8_t slave_id, uint16_t start_addr,
                                  uint16_t quantity, const uint8_t *rx_buf,
                                  uint16_t rx_len);

/* Extract exception code from exception response */
uint8_t modbus_get_exception_code(const uint8_t *rx_buf, uint16_t rx_len);

#endif
