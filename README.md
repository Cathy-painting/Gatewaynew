# 基于 CT117E-M4 的轻量化工业数据采集与云传输终端

本项目基于 `STM32G431RBT6`、FreeRTOS 和 CT117E-M4 开发板，实现一个轻量化工业数据采集与云传输终端。当前说明已按 `CT117E-M4产品手册.pdf` 里的原理图重新核对：板载 DAP 虚拟串口实际对应 `USART1/PA9/PA10`，外接 ESP8266 改为使用 J3 排针上的 `USART2/PA2/PA3`，避免和调试日志串口冲突。

项目遵守“不改 CubeMX 生成代码”的原则：CubeMX 生成目录只作为当前硬件初始化依据，业务代码和补充初始化都放在 `code/` 目录中。

## 当前实现

- FreeRTOS 多任务：`ledTask`、`sampleTask`、`modbusTask`、`logTask`、`cloudTask`。
- LD1-LD8 板载 LED：`PC8-PC15` 接 SN74HC573 数据输入，`PD2` 接锁存使能 `LE`。
- 本地模拟量采样：`ADC2_IN15/PB15`，对应手册中 J11/R37 电位器资源。
- 日志输出：`USART1/PA9/PA10`，对应板载 DAP 虚拟串口，`115200 8N1`。
- 云传输：ESP8266 AT 固件模块接 `USART2/PA2/PA3`，发布 MQTT JSON。
- RS485/Modbus RTU 主站：`USART3/PB10/PB11` + 外接 MAX485/SP3485，`PB13` 做方向控制。
- Modbus 功能码 `03`：读保持寄存器。
- Modbus 功能码 `06`：写单个保持寄存器接口。
- 统一终端状态：本地值、远程值、Modbus 在线状态、云在线状态、采样次数、通信成功/失败次数、上传次数。
- 异常处理：Modbus 超时、CRC/帧错误统计、连续失败离线、恢复上线、云上传失败重连。

## 和产品手册核对后的关键结论

| 手册原理图资源 | 实际连接 | 本项目处理 |
|---|---|---|
| LD1-LD8 | U1 SN74HC573，数据线 `PC8-PC15`，锁存 `PD2` | 代码使用 `bsp_led_write8()` 锁存输出，位值 `1` 表示点亮 |
| DAP 虚拟串口 | `PA9/USART1_TX`、`PA10/USART1_RX` | 日志从 USART1 输出 |
| J3 排针 | 引出 `PA1-PA7`，包含 `PA2/PA3` | ESP8266 使用 USART2 |
| J1 排针 | 引出 `PA11/PA12/PB10-PB15` | RS485 使用 `PB10/PB11/PB13` |
| PB15 模拟输入 | J11 接 R37 电位器 | ADC2 读取 PB15 |
| PB12 模拟输入 | J12 接 R38 电位器 | 当前未使用 |
| PA15/PB4 | J10/J9 接 555 信号资源 | 当前未使用 |
| PA11/PA12 | USB_DEVICE D-/D+ | 当前未使用，不建议拿去接普通串口 |
| PB8 | LCD_RS，同时和 BOOT0 相关 | 当前未使用，不建议外接新模块 |
| PC8-PC15 | 同时接 LED 锁存器和 LCD 数据线 | 当前使用 LED，不建议同时插 LCD 并驱动 |

## 需要购买的东西

最小演示版：

- CT117E-M4 / STM32G431RBT6 开发板。
- USB 数据线，用于供电、下载、查看 DAP 虚拟串口日志。
- MAX485 或 SP3485 TTL-RS485 模块，建议买 3.3 V 兼容款。
- USB-RS485 模块，或一个真实 Modbus RTU 从站设备。
- 杜邦线若干。

云传输版额外需要：

- ESP8266 AT 固件模块，例如 ESP-01S、ESP-12F 开发小板，必须支持 `AT+MQTTUSERCFG`、`AT+MQTTCONN`、`AT+MQTTPUB`。
- 稳定 3.3 V 电源模块，ESP8266 峰值电流建议至少 500 mA。
- USB-TTL 串口模块，用来先单独测试 ESP8266。
- MQTTX，用于 PC 端订阅 MQTT 数据。

推荐软件：

- Keil MDK-ARM。
- STM32CubeMX。
- STM32CubeG4 固件包。
- 串口助手，例如 SSCOM、XCOM、PuTTY、MobaXterm。
- MQTTX。

## 实际引脚分配

| 功能 | STM32 引脚 | 外设/接口 | 说明 |
|---|---|---|---|
| LD1-LD8 数据 | `PC8-PC15` | GPIO 输出 | 接 SN74HC573 的 `1D-8D` |
| LD1-LD8 锁存 | `PD2` | GPIO 输出 | 接 SN74HC573 `LE` |
| 调试日志 | `PA9/PA10` | USART1 | 板载 DAP 虚拟串口，115200 8N1 |
| ESP8266 | `PA2/PA3` | USART2 | J3 排针引出，115200 8N1 |
| RS485 数据 | `PB10/PB11` | USART3 | J1 排针引出，Modbus RTU 主站 |
| RS485 方向 | `PB13` | GPIO 输出 | 接 MAX485 `DE` 和 `/RE`，高发低收 |
| ADC 采样 | `PB15` | ADC2_IN15 | J11/R37 电位器，或外部 0-3.3 V 模拟量 |
| 下载调试 | `PA13/PA14` | SWDIO/SWCLK | 板载 DAP 下载调试 |
| 外部晶振 | `PF0/PF1` | HSE | 手册为 24 MHz |

## 接线说明

### 1. 日志串口

优先使用板载 DAP 虚拟串口，不需要额外接 USB-TTL：

1. 用开发板的 DAP USB 接电脑。
2. 在 Windows 设备管理器里找到新增的 USB 串行设备 COM 口。
3. 串口助手打开该 COM 口。
4. 参数设置为 `115200 8N1`。
5. 运行后应看到 `[BOOT] system start`、`[LOG]`、`[STATE]` 等日志。

注意：日志现在走 `USART1/PA9/PA10`，这是产品手册里 DAP 虚拟串口接到目标 MCU 的实际连接。不要再把 ESP8266 接到 PA9/PA10，否则会和板载 DAP 串口冲突。

### 2. MAX485 / RS485

STM32 到 MAX485：

| STM32 | MAX485/SP3485 模块 |
|---|---|
| `PB10 / USART3_TX` | `DI` |
| `PB11 / USART3_RX` | `RO` |
| `PB13` | `DE` 和 `/RE` 短接后接这里 |
| `GND` | `GND` |
| `3.3 V` 或 `5 V` | `VCC`，按模块要求供电 |

MAX485 到 USB-RS485 或 Modbus 从站：

| MAX485 | USB-RS485 / 从站 |
|---|---|
| `A` | `A` |
| `B` | `B` |
| `GND` | `GND`，调试时建议共地 |

如果一直超时，可以把 A/B 对调一次。不同模块对 A/B、D+/D- 的标注可能不完全一致。

### 3. ESP8266

先用 USB-TTL 单独测试 ESP8266：

| USB-TTL | ESP8266 |
|---|---|
| `TX` | `RX` |
| `RX` | `TX` |
| 稳定 `3.3 V` | `VCC` |
| `GND` | `GND` |

ESP8266 启动脚：

- `EN/CH_PD` 拉高到 3.3 V。
- `GPIO0` 正常运行时拉高到 3.3 V。
- 发送 `AT\r\n` 能返回 `OK` 后，再接入 STM32。

STM32 到 ESP8266：

| STM32 | ESP8266 |
|---|---|
| `PA2 / USART2_TX` | `RX` |
| `PA3 / USART2_RX` | `TX` |
| `GND` | `GND` |

ESP8266 不建议直接使用开发板弱 3.3 V 引脚供电。如果一连 Wi-Fi 就重启，优先检查 3.3 V 供电电流。

### 4. PB15 ADC

推荐直接使用板上 J11/R37 电位器资源：

1. 确认 J11 跳线连接到 PB15 对应电位器。
2. 运行程序。
3. 调节 R37。
4. 查看日志中的 `[SAMPLE] local=...` 是否变化。

如果使用外接电位器：

| 电位器 | STM32 |
|---|---|
| 一端 | `3.3 V` |
| 另一端 | `GND` |
| 中间滑动端 | `PB15 / ADC2_IN15` |

PB15 输入电压必须在 `0 V` 到 `3.3 V` 之间，不能接 5 V 模拟量。

## CubeMX 当前配置

当前工程配置和手册核对后可继续使用：

- MCU：`STM32G431RBT6`。
- SYS：Serial Wire。
- HSE：24 MHz 外部晶振。
- 系统时钟：HSE 经 PLL 到 80 MHz。
- FreeRTOS：CMSIS_V2。
- ADC2：`PB15 / ADC2_IN15`。
- USART1：`PA9/PA10`，115200 8N1，中断开启，用于 DAP 日志。
- USART2：`PA2/PA3`，115200 8N1，中断开启，用于 ESP8266。
- USART3：`PB10/PB11`，115200 8N1，中断开启，用于 RS485/Modbus。
- GPIO：`PC8-PC15` 和 `PD2` 用于 LD1-LD8 锁存输出。

手写代码额外处理：

- 当前 `Gateway.ioc` 没有把 `PB13` 配成 GPIO 输出。为避免改 CubeMX 生成代码，项目在 `bsp_rs485_init()` 中运行时配置 `PB13` 为 RS485 方向控制输出。

如果重新用 CubeMX 生成代码，请确认不要覆盖手写目录 `code/`。业务逻辑都在 `code/` 下。

## 软件结构

```text
code/
  App/
    app_tasks.c              FreeRTOS 任务业务入口
  Bsp/
    bsp_led.c/h              LD1-LD8 锁存 LED 驱动
    bsp_uart.c/h             USART 发送与中断接收缓冲
    bsp_rs485.c/h            MAX485 方向控制与发送
    bsp_adc.c/h              ADC2/PB15 采样
  Common/
    crc16.c/h                Modbus CRC16
    log.c/h                  USART1 DAP 日志输出
  Procotol/
    modbus_master.c/h        Modbus RTU 帧构建与解析
  Service/
    sample_service.c/h       本地 ADC 采样服务
    modbus_service.c/h       Modbus 轮询和 06 写接口
    cloud_service.c/h        ESP8266 Wi-Fi/MQTT 上传
    terminal_service.c/h     终端状态统一管理
    terminal_data.h          终端状态结构体
```

## FreeRTOS 任务

| 任务 | 周期 | 功能 |
|---|---:|---|
| `ledTask` | 500 ms | 翻转 LD1，表示系统运行 |
| `sampleTask` | 1000 ms | 读取 PB15 ADC，更新本地采样值 |
| `modbusTask` | 1000 ms | 读取 Modbus 从站寄存器 0 和 1 |
| `logTask` | 2000 ms | 打印终端完整状态 |
| `cloudTask` | 5000 ms | 通过 ESP8266 上传 MQTT JSON |

## 运行日志示例

启动后：

```text
[BOOT] system start
[SAMPLE] local=1234
[MODBUS] 03 tx: 01 03 00 00 00 02 C4 0B
[STATE] local=1234 remote=0 mb_online=0 cloud=0 sample=1 mb_ok=0 mb_fail=0 upload=0
```

Modbus 成功：

```text
[MODBUS] 03 rx: 01 03 04 00 7B 00 01 ...
[MODBUS] device online
[MODBUS] 03 ok reg0=123 reg1=1
```

云端连接成功：

```text
[CLOUD] check esp8266
[CLOUD] wifi connecting
[CLOUD] wifi connected
[CLOUD] mqtt connecting
[CLOUD] mqtt connected
[CLOUD] publish ok
```

MQTTX 收到的 JSON：

```json
{"local":1234,"remote":456,"online":1,"cloud":1,"sample":10,"mb_ok":8,"mb_fail":2,"upload":3}
```

## MQTTX 配置

1. 打开 MQTTX。
2. 新建连接。
3. Host 填 `broker.emqx.io`。
4. Port 填 `1883`。
5. Client ID 填 `gateway_pc_test` 或其他不重复 ID。
6. Username 和 Password 留空。
7. 新建订阅，Topic 填 `gateway/stm32/data`，QoS 选 `0`。
8. 开发板运行且 ESP8266 联网成功后，MQTTX 会收到 JSON。

云传输参数在 [cloud_service.h](E:/AMy_Project/Gateway/code/Service/cloud_service.h) 中修改：

```c
#define CLOUD_WIFI_SSID       "mywifi"
#define CLOUD_WIFI_PASSWORD   "12345678"
#define CLOUD_MQTT_HOST       "broker.emqx.io"
#define CLOUD_MQTT_PORT       1883U
#define CLOUD_MQTT_TOPIC      "gateway/stm32/data"
#define CLOUD_MQTT_CLIENT_ID  "stm32_gateway_g431"
```

建议：

- 使用 2.4 GHz Wi-Fi。
- Wi-Fi 名称和密码先用简单 ASCII 字符。
- 先用 USB-TTL 单独确认 ESP8266 能执行 `AT`、`AT+CWJAP`、`AT+MQTTUSERCFG`、`AT+MQTTCONN`、`AT+MQTTPUB`。

## 上电调试顺序

1. 用 Keil 编译并下载。
2. 打开板载 DAP 虚拟 COM 口，参数 `115200 8N1`。
3. 确认出现 `[BOOT] system start`。
4. 确认 LD1 每 500 ms 左右翻转。
5. 调节 J11/R37 或 PB15 输入电压，确认 `[SAMPLE] local=...` 会变化。
6. 接 MAX485 和 Modbus 从站。
7. 确认出现 Modbus TX/RX 日志，并最终看到 `mb_online=1`。
8. 断开 RS485，确认失败计数增加并最终 `device offline`。
9. 恢复 RS485，确认重新 `device online`。
10. 用 USB-TTL 单独测试 ESP8266。
11. ESP8266 接到 `PA2/PA3` 后运行，确认 MQTTX 收到 JSON。

## 常见问题

### 没有日志

- 确认打开的是板载 DAP 虚拟 COM 口，不是普通 USB_DEVICE 口。
- 串口参数必须是 `115200 8N1`。
- 当前日志走 `USART1/PA9/PA10`。
- 不要把 ESP8266 接到 PA9/PA10。

### Modbus 一直超时

- 检查 `PB10` 是否接 MAX485 `DI`。
- 检查 `PB11` 是否接 MAX485 `RO`。
- 检查 `PB13` 是否接 `DE` 和 `/RE`。
- 检查 A/B 是否需要对调。
- 检查 USART3 波特率是否和从站一致，当前为 `115200 8N1`。
- 检查从站地址是否为 `1`。
- 检查从站是否支持从保持寄存器地址 `0` 开始读 `2` 个寄存器。

### ESP8266 没反应

- 检查 ESP8266 供电，优先怀疑电流不够。
- 检查 `PA2` 接 ESP8266 `RX`，`PA3` 接 ESP8266 `TX`。
- 检查 `EN/CH_PD` 是否拉高。
- 检查 ESP8266 AT 固件波特率是否为 115200。
- 先用 USB-TTL 直接发送 `AT\r\n`，确认模块本身正常。

### MQTT 指令返回 ERROR

- 一些 ESP8266 AT 固件不支持 MQTT 指令。
- 可以刷支持 MQTT 的 Espressif AT 固件。
- 如果暂时不做云传输，本地采样 + RS485/Modbus + 日志已经可以作为核心演示。

## 项目简历描述

完整云传输版：

基于 STM32G431RBT6 和 FreeRTOS 设计轻量化工业数据采集与云传输终端，实现本地模拟量采集、RS485/Modbus RTU 主站轮询、ESP8266 联网和 MQTT 数据上传。项目采用多任务架构划分采样、通信、日志和云传输模块，并设计统一数据结构管理终端状态，支持通信超时、连续失败离线、恢复上线和上传失败重连等基础异常处理。

本地稳定版：

基于 STM32G431RBT6 和 FreeRTOS 设计轻量化工业数据采集终端，实现本地数据采样、RS485/Modbus RTU 主站轮询和串口日志监控。项目采用多任务架构划分采样、通信和日志模块，封装基础 BSP 接口，并加入 Modbus CRC 校验、通信超时、连续失败离线和恢复上线机制。
