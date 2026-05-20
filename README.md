# 基于 STM32G431RBT6 与 FreeRTOS 的轻量化工业数据采集与云传输终端

## 📋 项目概述

这是一个完整的嵌入式工业数据采集系统，从 0 到 1 的保姆级全流程项目。基于 STM32G431RBT6 微控制器和 FreeRTOS 实时操作系统，实现本地数据采样、RS485/Modbus RTU 主站通信、以及通过 ESP8266 的 MQTT 云传输功能。

### 核心特性

- **多任务架构**：5 个独立任务（LED、采样、Modbus、日志、云传输）
- **本地采样**：支持 ADC 模拟量采集或假数据模拟
- **Modbus RTU 主站**：通过 RS485 读写外部设备寄存器（功能码 03/06）
- **云传输**：ESP8266 AT 指令控制，MQTT JSON 数据上传
- **异常处理**：通信超时、连续失败离线、自动恢复上线
- **日志系统**：实时串口日志输出，便于调试和演示

## 🎯 最终实现效果

1. STM32G431RBT6 运行 FreeRTOS
2. ledTask 控制 PB9/LD9 周期闪烁（500ms）
3. sampleTask 周期产生本地采样值（1000ms）
4. modbusTask 通过 USART3 + RS485 + Modbus RTU 读取外部寄存器（1000ms）
5. logTask 通过 USART2 打印系统日志和终端状态（2000ms）
6. cloudTask 通过 ESP8266 连接 Wi-Fi，用 MQTT 上传 JSON 数据（5000ms）
7. MQTTX 或手机 MQTT 客户端可以看到 STM32 上传的数据
8. 支持 Modbus 超时、连续失败离线、恢复上线等基础异常处理

## 🔧 硬件配置

### 开发板：STM32G431RBT6

| 资源 | 引脚 | 用途 | 说明 |
|------|------|------|------|
| LED 运行指示 | PB9 (LD9) | ledTask 闪烁 | 低电平点亮 |
| 日志串口 | USART2 (PA2/PA3) | 板载 DAP 虚拟串口 | 115200 8N1 |
| RS485 串口 | USART3 (PB10/PB11) | Modbus 通信 | 9600 8N1 |
| RS485 使能 | PB13 | RS485_DE 控制 | GPIO Output |
| ADC 采样 | PA1 或 PB15 | 本地模拟量输入 | 12-bit |
| ESP8266 串口 | USART1 (PA9/PA10) | 云传输（可选） | 115200 8N1 |

### 外部模块

- **MAX485 模块**：RS485 物理层转换
- **USB 转 RS485 模块**：用于测试和调试
- **ESP8266 模块**：Wi-Fi 和 MQTT 功能（可选）

## 📁 软件架构

### 目录结构

```
Gateway/
├── Core/                    # STM32 核心文件
│   ├── Src/
│   │   ├── main.c
│   │   ├── stm32g4xx_it.c
│   │   └── app_freertos.c
│   └── Inc/
├── Drivers/                 # HAL 驱动
├── Middlewares/             # FreeRTOS
├── code/                    # 应用代码
│   ├── App/                 # 应用层
│   ├── Bsp/                 # 板级驱动（LED、UART、ADC、RS485）
│   ├── Common/              # 通用工具（log、crc16）
│   ├── Protocol/            # 协议实现（modbus_master）
│   └── Service/             # 业务服务（terminal、sample、modbus、esp8266、cloud）
└── 文档/                    # 分阶段代码参考
```

### 任务设计

| 任务名 | 优先级 | 周期 | 栈大小 | 功能 |
|--------|--------|------|--------|------|
| ledTask | Low | 500ms | 128 | LED 闪烁指示 |
| sampleTask | Normal | 1000ms | 256 | 本地数据采样 |
| modbusTask | Normal | 1000ms | 512 | RS485 Modbus 轮询 |
| logTask | Low | 2000ms | 512 | 状态日志输出 |
| cloudTask | BelowNormal | 5000ms | 768 | MQTT 数据上传 |

### 数据流

```
ADC/假数据 → sampleTask → terminal_service
                              ↓
                        终端状态数据结构
                              ↓
                    ┌─────────┼─────────┐
                    ↓         ↓         ↓
              modbusTask  logTask  cloudTask
                    ↓         ↓         ↓
              RS485/Modbus  USART2  ESP8266/MQTT
```

## 📚 分阶段实现指南（29 批）

### 第 0 批：准备软件、硬件和心态
- 软件：CubeMX、Keil、STM32CubeG4、ST-Link 驱动、串口助手、MQTTX
- 硬件：开发板、ST-Link、USB 转 TTL、MAX485、USB 转 RS485、ESP8266
- 文件夹：00_资料、01_CubeMX工程、02_Keil工程、03_笔记、04_截图、05_串口日志、06_演示视频、07_简历材料

### 第 1-6 批：基础工程（第 1 周）
- **第 1 批**：CubeMX 最小工程，LED + USART2 日志
- **第 2 批**：开启 FreeRTOS，跑 ledTask + logTask
- **第 3 批**：Queue 演示，理解任务间传数据
- **第 4 批**：工程分层（App/Bsp/Common/Protocol/Service）
- **第 5 批**：本地采样任务，先用假数据
- **第 6 批**：可选 ADC 真实采样

### 第 7-11 批：RS485/Modbus 通信（第 2-3 周）
- **第 7 批**：USART3 + RS485 物理链路
  - **关键问题**：USART3 中断未在 CubeMX 中使能
  - **解决方案**：在 NVIC Settings 中勾选 "USART3 global interrupt"
- **第 8 批**：USART3 单字节中断接收
- **第 9 批**：MAX485 接线和 RS485 收发
- **第 10 批**：CRC16 Modbus 校验
- **第 11 批**：Modbus RTU 03 读保持寄存器

### 第 12-16 批：系统整合（第 3-4 周）
- **第 12 批**：Modbus 06 写单寄存器
- **第 13 批**：modbusTask 周期轮询
- **第 14 批**：终端状态整合
- **第 15 批**：日志系统优化
- **第 16 批**：本地版项目验收（可投简历）

### 第 17-24 批：云传输和演示（第 5-6 周）
- **第 17 批**：ESP8266 串口方案选择
- **第 18 批**：ESP8266 AT 指令通信
- **第 19 批**：Wi-Fi 连接
- **第 20 批**：MQTT Broker 准备
- **第 21 批**：ESP8266 MQTT 连接
- **第 22 批**：上传终端真实状态
- **第 23 批**：完整项目联调
- **第 24 批**：错误处理和稳定性增强

### 第 25-29 批：演示和简历
- **第 25 批**：演示材料准备（视频、截图）
- **第 26 批**：简历写法
- **第 27 批**：面试讲解稿
- **第 28 批**：1 个半月时间安排
- **第 29 批**：每日笔记模板

## 🔑 关键代码模块

### BSP 层（板级驱动）

**bsp_uart.c** - UART 驱动
```c
void bsp_uart2_send_string(const char *str);  // UART2 发送（日志）
void bsp_uart3_send_string(const char *str);  // UART3 发送（RS485）
void bsp_uart3_rx_start(void);                // UART3 启动中断接收
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);  // 中断回调
```

**bsp_rs485.c** - RS485 驱动
```c
void bsp_rs485_set_tx_mode(void);   // 设置发送模式
void bsp_rs485_set_rx_mode(void);   // 设置接收模式
void bsp_rs485_send(const uint8_t *data, uint16_t len);  // 发送数据
```

### Protocol 层（协议实现）

**modbus_master.c** - Modbus RTU 主站
```c
uint16_t modbus_build_read_holding_req(uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *tx_buf);
uint8_t modbus_parse_read_holding_resp(const uint8_t *rx_buf, uint16_t rx_len, uint16_t *reg_values, uint16_t max_regs);
uint16_t modbus_build_write_single_req(uint8_t slave_id, uint16_t reg_addr, uint16_t value, uint8_t *tx_buf);
uint8_t modbus_verify_write_single_resp(const uint8_t *rx_buf, uint16_t rx_len, uint8_t slave_id, uint16_t reg_addr, uint16_t value);
```

**crc16.c** - CRC16 校验
```c
uint16_t crc16_modbus(const uint8_t *data, uint16_t len);  // Modbus RTU CRC16
```

### Service 层（业务服务）

**terminal_service.c** - 终端状态管理
```c
typedef struct {
    uint16_t local_value;        // 本地采样值
    uint16_t remote_value;       // Modbus 远程值
    uint8_t remote_online;       // 远程设备在线状态
    uint32_t sample_count;       // 采样次数
    uint32_t modbus_ok_count;    // Modbus 成功次数
    uint32_t modbus_fail_count;  // Modbus 失败次数
    uint32_t upload_count;       // 上传次数
} terminal_data_t;

void terminal_init(void);
void terminal_set_local_value(uint16_t value);
void terminal_set_remote_value(uint16_t value);
void terminal_set_remote_online(uint8_t online);
void terminal_get_snapshot(terminal_data_t *out);
```

**modbus_service.c** - Modbus 服务
```c
void modbus_service_init(void);
void modbus_service_poll(void);  // 周期轮询
uint8_t modbus_service_is_online(void);
uint16_t modbus_service_get_remote_value(void);
```

**log.c** - 日志系统
```c
void log_init(void);
void log_info(const char *msg);
void log_error(const char *msg);
void log_debug(const char *msg);
```

**esp8266_service.c** - ESP8266 服务
```c
void esp8266_service_init(void);
void esp8266_send_cmd(const char *cmd);
uint8_t esp8266_wait_response(const char *expect, uint32_t timeout_ms);
uint8_t esp8266_connect_wifi(const char *ssid, const char *password);
uint8_t esp8266_connect_mqtt(const char *broker, uint16_t port);
uint8_t esp8266_mqtt_publish(const char *topic, const char *payload);
```

## ⚙️ CubeMX 配置检查清单

### 系统配置
- [ ] SYS Debug: Serial Wire
- [ ] RCC HSE: Crystal/Ceramic Resonator (24MHz)
- [ ] Timebase Source: TIM6 或 TIM7（不要用 SysTick）

### GPIO 配置
- [ ] PB9: GPIO_Output (LED_RUN)
- [ ] PB13: GPIO_Output (RS485_DE)

### UART 配置
- [ ] USART2: Asynchronous, 115200, 8N1
- [ ] USART3: Asynchronous, 9600, 8N1
- [ ] **USART3 NVIC: 勾选 "USART3 global interrupt"**（第 7 批关键）
- [ ] USART1: Asynchronous, 115200, 8N1（可选，ESP8266）

### ADC 配置（可选）
- [ ] ADC1: 12-bit, Right alignment
- [ ] ADC1 Channel: PA1 或 PB15

### FreeRTOS 配置
- [ ] Interface: CMSIS_V2
- [ ] Tasks: ledTask, sampleTask, modbusTask, logTask, cloudTask
- [ ] Queues: sampleQueue（可选）

## 📊 串口日志格式

### 日志前缀说明

| 前缀 | 含义 | 来源 |
|------|------|------|
| [BOOT] | 系统启动 | main.c |
| [RTOS] | FreeRTOS 事件 | app_freertos.c |
| [SAMPLE] | 本地采样 | sampleTask |
| [MODBUS] | Modbus 通信 | modbusTask |
| [STATE] | 终端状态 | logTask |
| [CLOUD] | 云传输 | cloudTask |
| [ERROR] | 错误信息 | 各模块 |

### 典型日志输出

```
[BOOT] system start
[RTOS] tasks running
[SAMPLE] local=1234
[MODBUS] TX: 01 03 00 00 00 02 C4 0B
[MODBUS] RX OK: remote=456
[STATE] local=1234 remote=456 online=1 sample=100 mb_ok=95 mb_fail=5 upload=0
[CLOUD] mqtt publish ok
```

## 📡 Modbus RTU 通信

### 03 功能码（读保持寄存器）

**请求帧**：`[从站地址] [功能码] [起始地址H] [起始地址L] [数量H] [数量L] [CRC_L] [CRC_H]`

**响应帧**：`[从站地址] [功能码] [字节数] [数据...] [CRC_L] [CRC_H]`

**例子**：
- 请求：`01 03 00 00 00 02 C4 0B`（从站 1，读寄存器 0，数量 2）
- 响应：`01 03 04 00 7B 01 C8 XX XX`（寄存器 0 = 123，寄存器 1 = 456）

### 06 功能码（写单寄存器）

**请求帧**：`[从站地址] [功能码] [寄存器地址H] [寄存器地址L] [值H] [值L] [CRC_L] [CRC_H]`

**响应帧**：原样返回请求帧

### CRC16 计算

Modbus RTU 使用 CRC16-CCITT，初始值 0xFFFF，多项式 0xA001，低字节先发。

## 🌐 MQTT 数据格式

### 上传主题
```
gateway/stm32/data
```

### JSON 格式
```json
{
  "local": 1234,
  "remote": 456,
  "online": 1,
  "mb_ok": 95,
  "mb_fail": 5,
  "upload": 10
}
```

## 🐛 常见问题

### Q1：第七批串口不打印
**原因**：USART3 中断未在 CubeMX 中使能

**解决**：
1. 打开 `.ioc` 文件
2. Connectivity → USART3 → NVIC Settings
3. 勾选 "USART3 global interrupt"
4. 生成代码

### Q2：Modbus 通信超时
**可能原因**：RS485 接线错误、波特率不匹配、从站地址错误

**调试步骤**：
1. 检查 RS485 A/B 接线
2. 用串口助手验证波特率
3. 查看 USART2 日志中的 TX HEX
4. 用 USB 转 RS485 模块手动发送测试

### Q3：LED 不闪烁
**可能原因**：PB9 配置错误、LED 极性反接、FreeRTOS 任务未运行

**调试步骤**：
1. 检查 CubeMX 中 PB9 是否配置为 GPIO_Output
2. 检查 User Label 是否为 LED_RUN
3. 查看 USART2 日志是否有输出
4. 用示波器测量 PB9 电压

### Q4：MQTT 发布失败
**可能原因**：Wi-Fi 未连接、Broker 地址错误、网络不通

**调试步骤**：
1. 检查 ESP8266 是否连接 Wi-Fi（AT+CWJAP?）
2. 检查 Broker 地址和端口
3. 用 MQTTX 客户端测试 Broker 连接
4. 查看 ESP8266 返回的错误信息

## 📈 性能指标

| 指标 | 值 |
|------|-----|
| CPU 使用率 | < 30% |
| 内存使用 | < 50KB |
| 采样延迟 | < 10ms |
| Modbus 响应时间 | 200-500ms |
| MQTT 上传周期 | 5s |
| 系统启动时间 | < 2s |

## 🚀 快速开始

### 编译步骤
1. 打开 CubeMX，加载 `.ioc` 文件
2. 验证配置（见上文检查清单）
3. 生成代码
4. Keil 打开工程
5. 点击 Build 编译
6. 点击 Download 下载

### 验收标准
- [ ] 编译 0 Error
- [ ] LED 闪烁正常
- [ ] USART2 日志输出清楚
- [ ] 本地采样值周期更新
- [ ] Modbus 通信正常
- [ ] 离线检测正常
- [ ] 恢复上线正常
- [ ] MQTT 发布成功

## 📝 简历描述

### 完整云传输版
基于 STM32G431RBT6 和 FreeRTOS 设计轻量化工业数据采集与云传输终端，实现本地模拟量采集、RS485/Modbus RTU 主站轮询、ESP8266 联网和 MQTT 数据上传。项目采用多任务架构划分采样、通信、日志和云传输模块，并设计统一数据结构管理终端状态，支持通信超时、重试、离线检测和恢复上线日志输出。

### 本地版
基于 STM32G431RBT6 和 FreeRTOS 设计轻量化工业数据采集终端，实现本地数据采样、RS485/Modbus RTU 主站轮询和串口日志监控。项目采用多任务架构划分采样、通信和日志模块，封装基础 BSP 接口，并加入 Modbus CRC 校验、通信超时、连续失败离线和恢复上线机制。

## 🎤 面试讲解稿

**1 分钟项目介绍**：
我做的是一个基于 STM32G431 和 FreeRTOS 的轻量化工业数据采集与云传输终端。项目里 STM32 负责本地采样，同时通过 RS485 以 Modbus RTU 主站方式读取外部寄存器数据，再把本地值和远程值整合成终端状态。软件上我用 FreeRTOS 分成采样任务、Modbus 通信任务、日志任务和云上传任务，并通过统一的数据结构管理状态。后面通过 ESP8266 AT 指令连接 Wi-Fi，用 MQTT 把 JSON 数据上传到 Broker，在 MQTTX 或手机端查看。项目里我还做了 CRC 校验、超时重试和离线检测，方便演示异常恢复。

**常见问题**：
1. **为什么用 FreeRTOS？** 因为项目里有多个周期性功能，FreeRTOS 可以把这些功能拆成独立任务，让结构更清楚，也方便后续扩展和调试。
2. **你用了哪些任务？** ledTask、sampleTask、modbusTask、logTask 和 cloudTask。
3. **Modbus RTU 03 一帧是什么结构？** 03 请求帧包括从站地址、功能码、起始寄存器地址、寄存器数量和 CRC16。
4. **怎么判断设备离线？** 连续失败达到阈值（3 次）才把 online 置为 0，后续成功一次就恢复。
5. **ESP8266 怎么上传？** 用 STM32 通过串口发送 AT 指令控制 ESP8266，连接 Wi-Fi 和 MQTT，上传 JSON 数据。
6. **项目里最难的点是什么？** 把各个 demo 串成一个稳定的系统，避免串口日志、Modbus 接收、任务周期和 ESP8266 返回数据之间互相干扰。

## 📚 参考资源

- [STM32G431 数据手册](https://www.st.com/resource/en/datasheet/stm32g431cb.pdf)
- [FreeRTOS 官方文档](https://www.freertos.org/Documentation/161204_Mastering_the_FreeRTOS_Real_Time_Kernel.pdf)
- [Modbus RTU 规范](http://www.modbus.org/docs/Modbus_over_serial_line_V1_02.pdf)
- [MQTT 3.1.1 规范](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)

## 📄 许可证

MIT License

---

**最后更新**：2026 年 5 月 19 日

**项目状态**：完整文档（第 1-29 批）

**核心原则**：先跑通，再优化；先假数据，再真硬件；先串口日志看到结果，再做复杂功能
