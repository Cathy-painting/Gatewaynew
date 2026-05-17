// 引入自己的头文件，声明所有Modbus函数
#include "modbus_master.h"
// 引入之前学的CRC16校验函数，用来生成和验证"防伪码"
#include "crc16.h"

// 函数功能：检查收到的Modbus数据的CRC防伪码对不对
// 参数：buf=收到的数据，len=数据总长度
// 返回值：MODBUS_OK=正确，其他=错误
static int modbus_check_crc(const uint8_t *buf, uint16_t len)
{
    uint16_t calc_crc;  // 我们自己算出来的CRC
    uint16_t recv_crc;  // 设备发过来的CRC

    // 安全检查：数据为空 或者 长度小于4（Modbus最短的帧也有4个字节）
    if (buf == 0 || len < 4) {
        return MODBUS_ERR_PARAM;
    }

    // 第一步：计算前 len-2 个字节的CRC（因为最后2个字节是设备发过来的CRC）
    calc_crc = crc16_modbus(buf, (uint16_t)(len - 2));

    // 第二步：提取设备发过来的CRC
    // ⚠️ Modbus规定：CRC是低字节在前，高字节在后
    recv_crc = (uint16_t)buf[len - 2] | ((uint16_t)buf[len - 1] << 8);

    // 第三步：比较两个CRC，相等就返回正确，否则返回CRC错误
    return (calc_crc == recv_crc) ? MODBUS_OK : MODBUS_ERR_CRC;
}

// 函数功能：构建"读保持寄存器"的请求帧
// 参数：
// slave_id：要和哪个设备说话（设备地址，1~247）
// start_addr：从第几个寄存器开始读
// quantity：读几个寄存器
// tx_buf：用来存构建好的请求帧的缓冲区
// tx_buf_size：缓冲区的大小
// 返回值：构建好的帧的长度（固定8字节），失败返回0
uint16_t modbus_build_read_holding_req(uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *tx_buf, uint16_t tx_buf_size)
{
    uint16_t crc;

    // 安全检查：缓冲区为空 或者 缓冲区不够8字节 或者 读0个寄存器
    if (tx_buf == 0 || tx_buf_size < 8 || quantity == 0) {
        return 0;
    }

    // 按照Modbus标准格式，填充请求帧的每一个字节
    tx_buf[0] = slave_id;          // 第1字节：设备地址（"喂，1号设备，我找你"）
    tx_buf[1] = 0x03;              // 第2字节：功能码（03 = "我要读你的保持寄存器"）
    tx_buf[2] = (uint8_t)(start_addr >> 8);   // 第3字节：起始地址的高字节
    tx_buf[3] = (uint8_t)(start_addr & 0xFF); // 第4字节：起始地址的低字节
    tx_buf[4] = (uint8_t)(quantity >> 8);     // 第5字节：寄存器数量的高字节
    tx_buf[5] = (uint8_t)(quantity & 0xFF);   // 第6字节：寄存器数量的低字节

    // 计算前6个字节的CRC，填充到最后两个字节
    crc = crc16_modbus(tx_buf, 6);
    tx_buf[6] = (uint8_t)(crc & 0xFF);   // CRC低字节在前
    tx_buf[7] = (uint8_t)(crc >> 8);     // CRC高字节在后

    // 返回帧的长度（固定8字节）
    return 8;
}

// 函数功能：构建"写单个寄存器"的请求帧
// 参数：
// slave_id：设备地址
// reg_addr：要写的寄存器地址
// value：要写的值
// tx_buf：发送缓冲区
// tx_buf_size：缓冲区大小
// 返回值：帧长度（固定8字节），失败返回0
uint16_t modbus_build_write_single_req(uint8_t slave_id, uint16_t reg_addr, uint16_t value, uint8_t *tx_buf, uint16_t tx_buf_size)
{
    uint16_t crc;

    if (tx_buf == 0 || tx_buf_size < 8) {
        return 0;
    }

    // 和读请求几乎一模一样，只是功能码变成了0x06（写单个寄存器）
    tx_buf[0] = slave_id;
    tx_buf[1] = 0x06;              // 功能码：06 = "我要写你的一个寄存器"
    tx_buf[2] = (uint8_t)(reg_addr >> 8);
    tx_buf[3] = (uint8_t)(reg_addr & 0xFF);
    tx_buf[4] = (uint8_t)(value >> 8);    // 要写的值的高字节
    tx_buf[5] = (uint8_t)(value & 0xFF);  // 要写的值的低字节

    crc = crc16_modbus(tx_buf, 6);
    tx_buf[6] = (uint8_t)(crc & 0xFF);
    tx_buf[7] = (uint8_t)(crc >> 8);

    return 8;
}

// 函数功能：解析设备回复的"读保持寄存器"数据
// 参数：
// slave_id：期望的设备地址
// quantity：期望读的寄存器数量
// rx_buf：收到的设备回复数据
// rx_len：收到的数据长度
// regs：用来存解析出来的寄存器值的数组
// regs_size：数组的大小
// 返回值：MODBUS_OK=解析成功，其他=错误
int modbus_parse_read_holding_resp(uint8_t slave_id, uint16_t quantity, const uint8_t *rx_buf, uint16_t rx_len, uint16_t *regs, uint16_t regs_size)
{
    uint8_t byte_count;
    uint16_t expected_len;

    // 安全检查：参数为空 或者 数组不够大
    if (rx_buf == 0 || regs == 0 || quantity == 0 || regs_size < quantity) {
        return MODBUS_ERR_PARAM;
    }

    // 计算我们期望收到的回复长度：5 + 寄存器数量*2
		
    // （地址1 + 功能码1 + 字节数1 + 数据N*2 + CRC2 = 5 + 2N）
    expected_len = (uint16_t)(5 + quantity * 2);
    if (rx_len < expected_len) {
			//收到的数据长度<     期望收到的回复
        return MODBUS_ERR_FRAME;
    }

    // 第一步：检查CRC防伪码对不对
    if (modbus_check_crc(rx_buf, expected_len) != MODBUS_OK) {
        return MODBUS_ERR_CRC;
    }

    // 第二步：检查是不是我们要找的那个设备回复的
    if (rx_buf[0] != slave_id) {
        return MODBUS_ERR_FRAME;
    }

    // 第三步：检查设备有没有返回异常
    // 如果功能码的最高位是1，说明设备出错了
    if ((rx_buf[1] & 0x80) != 0) {
        return MODBUS_ERR_EXCEPTION;
    }

    // 第四步：检查功能码是不是我们要的0x03（读保持寄存器）
    if (rx_buf[1] != 0x03) {
        return MODBUS_ERR_FRAME;
    }

    // 第五步：检查字节数对不对（应该等于寄存器数量*2）
    byte_count = rx_buf[2];
    if (byte_count != quantity * 2) {
        return MODBUS_ERR_FRAME;
    }

    // 第六步：提取每个寄存器的值
    // ⚠️ Modbus规定：寄存器值是高字节在前，低字节在后（和CRC相反！）
    for (uint16_t i = 0; i < quantity; i++) {
        regs[i] = ((uint16_t)rx_buf[3 + i * 2] << 8) | rx_buf[4 + i * 2];
    }

    // 所有检查都通过，返回成功
    return MODBUS_OK;
}

// 函数功能：解析设备回复的"写单个寄存器"数据
// 设备写成功后，会把你发的请求原封不动发回来
int modbus_parse_write_single_resp(uint8_t slave_id, uint16_t reg_addr, uint16_t value, const uint8_t *rx_buf, uint16_t rx_len)
{
    uint16_t resp_addr;
    uint16_t resp_value;

    if (rx_buf == 0 || rx_len < 8) {
        return MODBUS_ERR_PARAM;
    }

    // 检查CRC
    if (modbus_check_crc(rx_buf, 8) != MODBUS_OK) {
        return MODBUS_ERR_CRC;
    }

    // 检查设备地址和功能码
    if (rx_buf[0] != slave_id || rx_buf[1] != 0x06) {
        return MODBUS_ERR_FRAME;
    }

    // 提取设备回复的地址和值
    resp_addr = ((uint16_t)rx_buf[2] << 8) | rx_buf[3];
    resp_value = ((uint16_t)rx_buf[4] << 8) | rx_buf[5];

    // 检查是不是和我们发的一样
    if (resp_addr != reg_addr || resp_value != value) {
        return MODBUS_ERR_FRAME;
    }

    return MODBUS_OK;
}
