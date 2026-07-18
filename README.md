# 基于 CT117E-M4 的轻量化工业数据采集与云传输终端

本项目基于 `STM32G431RBT6`、FreeRTOS 和 CT117E-M4 开发板，实现一个轻量化工业数据采集与云传输终端。当前说明已按 `CT117E-M4产品手册.pdf` 里的原理图重新核对：板载 DAP 虚拟串口实际对应 `USART1/PA9/PA10`，外接 ESP8266 改为使用 J3 排针上的 `USART2/PA2/PA3`，避免和调试日志串口冲突。

项目遵守"不改 CubeMX 生成代码"的原则：CubeMX 生成目录只作为当前硬件初始化依据，业务代码和补充初始化都放在 `code/` 目录中。

## 当前实现

- **FreeRTOS 多任务**：`ledTask`、`sampleTask`、`modbusTask`、`logTask`、`cloudTask`。
- **LED 指示**：LD1-LD8 板载 LED，`PC8-PC15` 接 SN74HC573 数据输入，`PD2` 接锁存使能 `LE`；`bsp_led_write8()` 锁存输出。
- **本地模拟量采样**：`ADC2_IN15/PB15`，对应手册中 J11/R37 电位器资源。
- **日志输出**：`USART1/PA9/PA10`，对应板载 DAP 虚拟串口，`115200 8N1`。
- **RS485/Modbus RTU 主站**：`USART3/PB10/PB11` + 外接 MAX485/SP3485，`PB13` 做方向控制。支持功能码 `03`（读保持寄存器）和 `06`（写单个保持寄存器）。
- **云传输（ESP8266 HTTP Server）**：ESP8266 AT 固件模块接 `USART2/PA2/PA3`，ESP8266 作为 HTTP 服务器（端口 80），手机浏览器直接访问 ESP8266 IP 查看实时数据。内置 8 卡片深色主题仪表盘（本地值、远程值、Modbus 状态、云端状态、采样次数、通信成功数、失败数、上传次数）。
- **统一终端状态**：`terminal_service` 管理全部状态变量，避免全局变量分散。
- **异常处理**：Modbus 超时重试、连续失败离线、恢复上线自愈；云服务连接失败重连机制。

## 最终架构方案

```
┌──────────┐    USART2     ┌──────────┐    WiFi     ┌─────────────┐
│  STM32   │ ←──────────→ │ ESP8266  │ ←─────────→ │ 手机浏览器   │
│ (主站)   │  AT指令/数据   │(HTTP Srv)│  TCP:80     │             │
└──────────┘              └──────────┘              └─────────────┘

┌──────────┐  USART3+RS485 ┌──────────┐
│  STM32   │ ←──────────→ │Modbus从站 │
│ (主站)   │   Modbus RTU   │ (ID=1)   │
└──────────┘              └──────────┘
```

> **方案决策**：因 ESP8266 基础 AT 固件不支持 MQTT AT 指令（`AT+MQTTUSERCFG` 返回 ERROR），本项目采用 HTTP Server 模式：ESP8266 开启 TCP Server，手机浏览器直接连接 ESP8266 的 WiFi IP 即可查看实时数据，无需服务器中转。

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

**最小演示版（本地采样 + Modbus）：**

- CT117E-M4 / STM32G431RBT6 开发板。
- USB 数据线，用于供电、下载、查看 DAP 虚拟串口日志。
- MAX485 或 SP3485 TTL-RS485 模块，建议买 3.3V 兼容款。
- USB-RS485 模块，或一个真实 Modbus RTU 从站设备。
- 杜邦线若干。

**云传输版额外需要：**

- ESP8266 AT 固件模块（ESP-01S 或 ESP-12F 开发小板），基础 AT 固件即可，无需 MQTT 支持。
- **稳定 3.3V 电源模块（关键！）**：ESP8266 峰值电流约 300-500mA，必须独立供电。
  - 推荐：AMS1117-3.3 稳压模块，输入 5V 输出 3.3V。
  - 次选：开发板自带 3.3V 引脚（确认能提供足够电流）。
  - 避免：USB-TTL 模块的 3.3V 输出（电流不足，WiFi 发射时会重启）。
- USB-TTL 串口模块，用来先单独测试 ESP8266。

**推荐软件：**

- Keil MDK-ARM
- STM32CubeMX
- STM32CubeG4 固件包
- 串口助手（SSCOM、XCOM、PuTTY、MobaXterm）
- Modbus Slave（调试 Modbus 从站模拟）
- 手机/PC 浏览器（查看 ESP8266 HTTP 仪表盘）

## 实际引脚分配

| 功能 | STM32 引脚 | 外设/接口 | 说明 |
|---|---|---|---|
| LD1-LD8 数据 | `PC8-PC15` | GPIO 输出 | 接 SN74HC573 的 `1D-8D` |
| LD1-LD8 锁存 | `PD2` | GPIO 输出 | 接 SN74HC573 `LE` |
| 调试日志 | `PA9/PA10` | USART1 | 板载 DAP 虚拟串口，115200 8N1 |
| ESP8266 | `PA2/PA3` | USART2 | J3 排针引出，115200 8N1 |
| RS485 数据 | `PB10/PB11` | USART3 | J1 排针引出，Modbus RTU 主站 |
| RS485 方向 | `PB13` | GPIO 输出 | 接 MAX485 `DE` 和 `/RE`，高发低收 |
| ADC 采样 | `PB15` | ADC2_IN15 | J11/R37 电位器，或外部 0-3.3V 模拟量 |
| 下载调试 | `PA13/PA14` | SWDIO/SWCLK | 板载 DAP 下载调试 |

## 接线说明

### 1. 日志串口

优先使用板载 DAP 虚拟串口，不需要额外接 USB-TTL：

1. 用开发板的 DAP USB 接电脑。
2. 在 Windows 设备管理器里找到新增的 USB 串行设备 COM 口。
3. 串口助手打开该 COM 口。
4. 参数设置为 `115200 8N1`。
5. 运行后应看到 `[BOOT] system start`、`[STATE]` 等日志。

> 注意：日志走 `USART1/PA9/PA10`，不要把 ESP8266 接到 PA9/PA10，否则会和板载 DAP 串口冲突。

### 2. MAX485 / RS485（Modbus 通信）

**STM32 到 MAX485：**

| STM32 | MAX485/SP3485 模块 | 说明 |
|---|---|---|
| `PB10 / USART3_TX` | `DI` | MCU 发 -> 485 发 |
| `PB11 / USART3_RX` | `RO` | 485 收 -> MCU 收 |
| `PB13` | `DE` 和 `/RE`（短接后接 PB13） | **关键：必须短接！** 高电平发送，低电平接收 |
| `GND` | `GND` | 必须共地 |
| `3.3V` 或 `5V` | `VCC` | 按模块供电要求（3.3V 模块接 3.3V，5V 模块接 5V） |

**MAX485 到 USB-RS485 或 Modbus 从站：**

| MAX485 | USB-RS485 / 从站 |
|---|---|
| `A` | `A`（或 D+） |
| `B` | `B`（或 D-） |
| `GND` | `GND`（调试阶段建议共地） |

> **常见坑**：不同模块 A/B 标注可能不一致，超时时优先对调 A/B 测试。

### 3. ESP8266（HTTP 服务器模式）

**接线分两步：先 USB-TTL 单独测试，再接入 STM32。**

#### 3.1 USB-TTL 单独测试 ESP8266

| USB-TTL | ESP8266 |
|---|---|
| `TX` | `RX` |
| `RX` | `TX` |
| 稳定 `3.3V`（独立供电！） | `VCC`、`EN/CH_PD`、`RST`、`IO0` |
| `GND` | `GND` |

> **供电要点**：ESP8266 WiFi 发射时峰值电流 ~300-500mA。USB-TTL 的 3.3V 引脚电流不足，必须使用独立稳压电源。推荐将所有 3.3V 引脚（VCC、RST、IO0、EN）都接到稳压模块输出。

测试：发送 `AT\r\n` 应返回 `OK`。

#### 3.2 STM32 到 ESP8266

| STM32 | ESP8266 |
|---|---|
| `PA2 / USART2_TX` | `RX` |
| `PA3 / USART2_RX` | `TX` |
| `GND` | `GND` |
| — | `VCC`、`RST`、`IO0` 接独立 3.3V 电源 |

> **重要**：ESP8266 的 VCC/GND 接独立供电，**不要**从开发板 3.3V 引脚取电（除非确认开发板能稳定提供 500mA）。

### 4. PB15 ADC

推荐直接使用板上 J11/R37 电位器资源：

1. 确认 J11 跳线连接到 PB15 对应电位器。
2. 运行程序。
3. 调节 R37。
4. 查看日志中的 `[SAMPLE] local=...` 是否变化。

PB15 输入电压必须在 `0V` 到 `3.3V` 之间，不能接 5V。

## CubeMX 当前配置

- MCU：`STM32G431RBT6`
- SYS：Serial Wire
- HSE：24 MHz 外部晶振，PLL 到 80 MHz
- FreeRTOS：CMSIS_V2
- ADC2：`PB15 / ADC2_IN15`
- USART1：`PA9/PA10`，115200 8N1，中断开启（DAP 日志）
- USART2：`PA2/PA3`，115200 8N1，中断开启（ESP8266）
- USART3：`PB10/PB11`，115200 8N1，中断开启（RS485/Modbus）
- GPIO：`PC8-PC15`、`PD2` 用于 LD1-LD8 锁存输出
- PB13 由 `bsp_rs485_init()` 运行时配置为 RS485 方向控制

## 软件结构

```text
code/
  App/
    app_tasks.c              FreeRTOS 任务业务入口（5 个任务）
  Bsp/
    bsp_led.c/h              LD1-LD8 锁存 LED 驱动
    bsp_uart.c/h             USART1/2/3 中断接收缓冲 + 发送
    bsp_rs485.c/h            MAX485 方向控制（DE/RE）与发送
    bsp_adc.c/h              ADC2/PB15 采样
  Common/
    crc16.c/h                Modbus CRC16 校验
    log.c/h                  USART1 DAP 日志输出
  Procotol/
    modbus_master.c/h        Modbus RTU 帧构建（03/06）与解析
  Service/
    sample_service.c/h       本地 ADC 采样 + 假数据
    modbus_service.c/h       Modbus 轮询引擎 + 异常处理
    cloud_service.c/h        ESP8266 HTTP Server（AT 指令驱动）
    terminal_service.c/h     终端状态统一管理（单例模式）
    terminal_data.h          终端状态结构体定义
```

## FreeRTOS 任务

| 任务 | 周期 | 功能 |
|---|---:|---|
| `ledTask` | 500 ms | 翻转 LD1，表示系统运行 |
| `sampleTask` | 1000 ms | 读取 PB15 ADC，更新本地采样值 |
| `modbusTask` | 1000 ms | 读取 Modbus 从站保持寄存器 0 和 1 |
| `logTask` | 2000 ms | 打印终端完整状态到串口 |
| `cloudTask` | 5000 ms | ESP8266 HTTP Server：处理浏览器请求并返回实时数据页面 |

## 运行日志示例

**启动：**

```
[BOOT] system start
[SAMPLE] local=1234
[MODBUS] 03 tx: 01 03 00 00 00 02 C4 0B
[STATE] local=1234 remote=0 mb_online=0 cloud=0 sample=1 mb_ok=0 mb_fail=0 upload=0
```

**Modbus 成功：**

```
[MODBUS] 03 rx: 01 03 04 00 7B 00 00 8A 2A
[MODBUS] 03 ok reg0=123 reg1=0
```

**云端（ESP8266 HTTP Server）连接成功：**

```
[CLOUD] publish_once enter
[CLOUD] check esp8266
[CLOUD] wifi connecting
[CLOUD] wifi connected
[CLOUD] esp8266 ip=192.168.43.112
[CLOUD] http server started
```

**手机浏览器访问后：**

```
[CLOUD] browser request link=0 len=...
[CLOUD] HTTP response sent
```

## HTTP 仪表盘说明

手机浏览器访问 ESP8266 的 IP 地址（如 `http://192.168.43.112`），页面为深色主题（Tailwind 风格），2x4 网格布局显示 8 张数据卡片：

| 卡片 | 数据字段 | 颜色 |
|---|---|---|
| 本地 | `local_value`（ADC 采样值） | 蓝色 #38bdf8 |
| 远程 | `remote_value`（Modbus 寄存器值） | 绿色 #22c55e |
| Modbus | `remote_online` ON/OFF | 绿/红动态 |
| 云端 | `cloud_ready` ON/OFF | 绿/红动态 |
| 采样 | `sample_count`（采样次数） | 橙色 #f59e0b |
| 成功 | `modbus_ok_count`（Modbus 成功次数） | 青绿 #a3e635 |
| 失败 | `modbus_fail_count`（Modbus 失败次数） | 红色 #ef4444 |
| 上传 | `upload_count`（HTTP 响应次数） | 粉色 #f472b6 |

> **技术细节**：HTML 采用 CSS 类 + 少量内联样式，总大小约 1150 字节，确保能在 ESP8266 TCP 缓冲区限制内完整传输。

## Wi-Fi 与 云端配置

在 [cloud_service.h](code/Service/cloud_service.h) 中修改：

```c
#define CLOUD_WIFI_SSID       "你的WiFi名称"
#define CLOUD_WIFI_PASSWORD   "你的WiFi密码"
#define CLOUD_SERVER_PORT     80U    /* HTTP 服务器端口 */
```

建议：
- 使用 2.4 GHz Wi-Fi（手机热点即可）。
- WiFi 名称和密码先用简单 ASCII 字符。
- 先用 USB-TTL 单独确认 ESP8266 能执行 `AT`、`AT+CWJAP`、`AT+CIPSERVER`。

## 上电调试顺序

1. 仅 USB：确认 `[BOOT] system start` 与周期 `[STATE]` 日志出现。
2. 调 PB15：确认 `[SAMPLE] local=...` 随电位器变化。
3. 接 RS485 + Modbus 从站：确认 `03 tx/rx` 日志、`mb_online=1`。
4. 断开 RS485：确认失败计数增加并最终 `device offline`。
5. 恢复 RS485：确认重新 `device online`。
6. USB-TTL 单独测试 ESP8266：`AT` 返回 `OK`。
7. ESP8266 接入 STM32 USART2：确认 `wifi connected`、`http server started`。
8. 手机连接同一 WiFi，浏览器访问 ESP8266 IP：确认 8 张卡片显示实时数据。

---

## 调试问题总结（面试备用）

以下是本项目开发过程中实际遇到的典型问题和解决思路，适合在面试中展示问题分析和解决能力。

### 一、RS485 + Modbus 通信调试

#### 问题 1：Modbus 一直超时，OLED 显示 "Modbus: OFFLINE"

**现象**：`modbusTask` 不断发送 03 请求帧，但串口日志只有 `tx` 没有 `rx`，失败计数持续增长。

**排查过程**：

1. **MAX485 方向控制**：检查 `PB13` 是否接到 `DE` 和 `/RE`，且 `DE` 与 `/RE` 必须短接。本项目中 `bsp_rs485_init()` 运行时配置 PB13 为推挽输出，发送前拉高、发送完拉低。

2. **A/B 信号线**：不同厂商的 485 模块对 A/B 标注不一致。超时时优先对调 A/B 线测试。

3. **共地问题**：STM32 端 MAX485 的 GND 必须与 USB-RS485 模块的 GND 连接，否则 RS485 差分信号无参考地。

4. **USB-RS485 模块模式开关**：部分 USB-RS485 模块有 RS485/RS232 切换开关。拨到 RS232 模式时 TX/RX 直连但 RS485 差分驱动不工作。必须确保开关在 RS485 位置。

5. **Modbus Slave 软件配置**：
   - Slave ID 必须与代码中 `MODBUS_SLAVE_ID` 一致（本项目默认=1）。
   - Holding Registers 表格要从地址 0 开始（不是 1），代码读取地址 0 和 1 共 2 个寄存器。
   - 连接端口选 USB-RS485 对应的 COM 口。
   - 波特率、数据位、校验位必须与 STM32 端一致（本项目：115200 8N1）。

6. **TTL 直连测试**：旁路 MAX485，用 USB 转 TTL 模块直接连接 PB10(RX-TTL) 和 PB11(TX-TTL)，可验证 USART3 配置和 Modbus 协议栈本身是否正确，排除 485 物理层的问题。

**最终解决**：确认 DE/RE 短接并接到 PB13、A/B 正确对应、Modbus Slave 从地址 0 填写数据、共地连接正确后，通信恢复正常。

#### 问题 2：LoopBack 回环测试无数据

**现象**：OLED 显示 `LoopBack: N/A`，串口助手只能收到 `00`。

**原因**：回环测试需要将 PB10(TX) 与 PB11(RX) 直接短接，但 MAX485 模块介入后破坏了回环路径。正确做法是测试时临时将 PB10 和 PB11 用杜邦线短接，测试完再恢复。

---

### 二、ESP8266 供电问题

#### 问题 3：ESP8266 反复重启，WiFi 连接不稳定

**现象**：串口日志显示 `cloud` 状态在 ON 和 OFF 之间反复跳变，ESP8266 不断输出启动信息。

**根因**：**供电不足**。ESP8266 在 WiFi 发射时峰值电流达到 300-500mA，USB-TTL 模块的板载 3.3V LDO 通常只能提供 100-200mA，无法满足 ESP8266 峰值需求，导致电压跌落、模块复位重启。

**解决方案对比**：

| 方案 | 说明 | 效果 |
|---|---|---|
| USB-TTL 的 3.3V 引脚 | 直接由 USB-TTL 供电 | 失败，电流不足 |
| 两个 USB-TTL 并联供电 | 两个模块的 3.3V 并联 | 勉强可用，不稳定 |
| 开发板 3.3V 引脚 | 从 STM32 开发板取电 | 需确认开发板 3.3V 稳压能力 |
| AMS1117-3.3 稳压模块 | 输入 5V，输出 3.3V，最大 1A | **推荐方案**，稳定可靠 |

**最佳接线方案（推荐）**：

```
USB 5V → AMS1117-3.3 → 3.3V → ESP8266 VCC
                              → ESP8266 RST（上拉）
                              → ESP8266 IO0（上拉，正常运行模式）
                              → ESP8266 EN/CH_PD（上拉）
          GND               → ESP8266 GND
                            → STM32 GND（共地）
```

**关键经验**：
- ESP8266 的 VCC、RST、IO0、EN 都应接到同一个稳定的 3.3V 电源。
- STM32 的 PA2/PA3 与 ESP8266 的 RX/TX 交叉连接，**务必共地**。
- 遇到 ESP8266 异常先检查供电，99% 的稳定性问题是电源引起的。

---

### 三、ESP8266 通信协议切换（MQTT → HTTP Server）

#### 问题 4：ESP8266 不支持 MQTT AT 指令

**现象**：发送 `AT+MQTTUSERCFG` 或 `AT+MQTTCONN` 等 MQTT 指令时返回 `ERROR`。

**原因**：ESP8266 基础 AT 固件（出厂固件）只支持标准 AT 指令集（WiFi、TCP/IP），MQTT AT 指令需要刷写专门的 MQTT AT 固件。

**方案决策**：

| 方案 | 优点 | 缺点 |
|---|---|---|
| 刷 MQTT 固件 | MQTT 协议标准，物联网行业常用 | 需要刷固件工具和固件文件，有变砖风险 |
| **HTTP Server 模式** | 无需刷固件，基础 AT 指令即可 | 浏览器需在同 WiFi 下主动访问 |

**最终采用 HTTP Server 模式**：ESP8266 开启 TCP Server（`AT+CIPSERVER=1,80`），手机浏览器访问 ESP8266 的 IP 即可看到实时数据仪表盘。方案架构更简单，满足当前演示需求。

#### 问题 5：AT+CIPSERVER=1,80 返回 ERROR

**现象**：发送 `AT+CIPSERVER=1,80` 开启 TCP Server 时返回 ERROR。

**原因**：ESP8266 默认为单连接模式（`AT+CIPMUX=0`），开启 Server 必须先切换到多连接模式。

**解决方案**：
```
AT+CIPMUX=1        → 先切换到多连接模式
AT+CIPSERVER=1,80   → 再开启 TCP Server
```

#### 问题 6：HTML 页面卡片显示不完整（5 张而非 8 张）

**现象**：手机浏览器只显示前 5 张卡片（本地、远程、Modbus、云端、采样），成功、失败、上传三张卡片缺失。

**原因**：原 HTML 每张卡片都使用完整内联样式（`style="background:...;border-radius:...;padding:...;text-align:...;border:..."`），8 张卡片总 HTML 约 2000 字节，超出 ESP8266 AT+CIPSEND 单次发送的 TCP 缓冲区限制，导致尾部截断。

**解决方案对比**：

| 方案 | HTML 大小 | 实现难度 |
|---|---|---|
| 旧：全部内联样式 | ~2000 字节 | 简单但被截断 |
| 新：CSS 类 + 少量内联 | ~1150 字节 | **采用此方案** |

**最终实现**：用 `<style>` 标签定义 CSS 类（`.g` 网格布局、`.c` 卡片容器、`.l` 标签文字、`.v` 数值文字、`.m` 状态文字），每张卡片从 ~180 字节压缩到 ~70 字节，8 张卡片完整显示。

**核心代码片段**（来自 `cloud_service.c` 的 `cloud_build_response`）：
```c
snprintf(buf, buf_size,
    "HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=utf-8\r\n"
    "Connection: close\r\n\r\n"
    "<!DOCTYPE html><html><head>..."
    "<style>"
    "*{margin:0;padding:0}"
    "body{background:#0f172a;...}"
    ".g{display:grid;grid-template-columns:1fr 1fr;...}"
    ".c{background:#1e293b;...}"
    ".l{font-size:9px;color:#94a3b8}"
    ".v{font-size:20px}.m{font-size:14px}"
    "</style></head><body>"
    // 8 张卡片，每张约 70 字节
    "<div class=c><div class=l>本地</div><div class=v style=color:#38bdf8>%u</div></div>"
    ...
    "</body></html>",
    ...);
```

#### 问题 7：编译错误 "too many arguments in function call"

**现象**：Keil 编译 `cloud_service.c` 报错 `#140: too many arguments in function call`，定位在 `cloud_send_http_response(link_id, header, body)`。

**原因**：代码重构时，`cloud_send_http_response` 函数签名已改为只接受 `(int link_id, const char *response)` 两个参数，但调用处残留旧代码传入了三个参数（`link_id`、`header`、`body` 分开传递）。

**解决方案**：在 `cloud_service_publish_once()` 中改为先用 `cloud_build_response()` 一次性构建完整 HTTP 响应（头+正文），再将拼接好的 `response` 字符串传给 `cloud_send_http_response()`。

---

### 四、其他编译与运行问题

#### 问题 8：FreeRTOS 堆内存不足

**现象**：任务创建失败，部分功能不运行。

**原因**：本项目 5 个任务 + Modbus/Cloud 服务缓冲区，原 Heap 大小 10240 字节不够。

**解决方案**：在 [FreeRTOSConfig.h](Core/Inc/FreeRTOSConfig.h) 中将 `configTOTAL_HEAP_SIZE` 从 `10240` 增大到 `15360`。

---

### 五、调试方法论总结

1. **分层隔离**：先验证单模块（USB-TTL 单独测 ESP8266），再联调。
2. **最小系统**：先只接 USB 确认 RTOS 任务框架正常，再逐个加入 ADC、RS485、ESP8266。
3. **日志驱动**：每个模块都有充分的状态日志输出，定位问题不靠猜测。
4. **硬件问题优先排查**：遇到通信问题先检查供电、共地、接线，再怀疑代码。
5. **旁路测试**：跳过可疑硬件（如绕过 MAX485 用 TTL 直连），缩小问题范围。

## 常见问题

### 没有日志

- 确认打开的是板载 DAP 虚拟 COM 口，不是普通 USB_DEVICE 口。
- 串口参数必须是 `115200 8N1`。
- 当前日志走 `USART1/PA9/PA10`。
- 不要把 ESP8266 接到 PA9/PA10。

### Modbus 一直超时

1. 检查 `PB13` 是否接 MAX485 的 `DE` 和 `/RE`（两者必须短接）。
2. 检查 `PB10 → DI`、`PB11 → RO` 是否接反。
3. A/B 线对调测试一次。
4. GND 共地是否连接。
5. 从站地址是否为 `1`、寄存器地址从 `0` 开始、波特率为 `115200 8N1`。
6. USB-RS485 模块模式开关是否在 RS485 位置。

### ESP8266 没反应 / 反复重启

1. **99% 是供电问题**：检查 3.3V 电源是否能提供 ≥500mA 电流。
2. 检查 `PA2 → ESP RX`、`PA3 → ESP TX`（交叉连接）。
3. 检查 ESP8266 的 RST、IO0、EN 是否都拉高到 3.3V。
4. 检查 STM32 与 ESP8266 是否共地。
5. 先用 USB-TTL 直接发 `AT\r\n` 确认模块本身正常。

### 手机浏览器看不到完整 8 张卡片

- 确认 `cloud_service.c` 中的 `cloud_build_response` 已更新为 CSS 类版本。
- 确认编译下载后 STM32 已重启。
- 确认手机与 ESP8266 连接同一 WiFi。
- 确认访问的是 ESP8266 IP 的 80 端口（如 `http://192.168.43.112`）。

## 项目简历描述

**完整版（云传输 + Modbus）：**

基于 STM32G431RBT6 和 FreeRTOS 设计轻量化工业数据采集与云传输终端，实现本地模拟量采集（ADC）、RS485/Modbus RTU 主站轮询、ESP8266 HTTP Server 实时数据仪表盘。项目采用 5 任务多任务架构划分采样、通信、日志和云传输模块，设计统一终端状态数据结构管理全局变量，支持 Modbus 超时重试、连续失败离线/恢复上线自愈、ESP8266 供电稳定性优化、HTML/CSS 数据可视化等。从硬件接线、协议调试到云端展示，完整经历了一个嵌入式 IoT 项目的全流程开发。

**核心亮点（面试可讲）：**

1. **协议栈实现**：手写 Modbus RTU 03/06 功能码帧构建与 CRC16 校验，不依赖第三方库。
2. **异常处理机制**：Modbus 连续失败离线检测 + 自动恢复上线，ESP8266 连接失败自动重连。
3. **硬件调试能力**：独立定位 ESP8266 供电不足导致的反复重启问题，设计 AMS1117 稳压方案。
4. **架构权衡决策**：在 ESP8266 不支持 MQTT AT 指令时，评估刷固件 vs HTTP Server 两种方案，选择更稳定的 HTTP Server 方案。
5. **全栈开发**：从 STM32 底层 BSP 驱动 → FreeRTOS 任务调度 → 应用层协议 → 前端 HTML/CSS 仪表盘。
