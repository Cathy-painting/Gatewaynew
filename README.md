# 基于 CT117E-M4 的工业数据采集网关

本项目基于 `STM32G431RBT6`、FreeRTOS 和 CT117E-M4 开发板，实现一个**工业级数据采集与物联网关终端**，适用于工控/BMS/物联网岗位秋招展示。

> **版本**: v2.3 | **硬件**: CT117E-M4 (STM32G431RBT6) + ESP32 + DHT11 | **系统**: FreeRTOS + HAL

---

## 项目亮点

- **7 任务架构**：watchdogTask / sampleTask / modbusTask / cloudTask / cmdTask / ledTask / logTask，职责清晰分离
- **命令队列异步处理**：cloudTask 收包 → 投递 cmdQueue → cmdTask 异步处理，Flash 写入/Modbus 写等耗时操作不阻塞收包
- **独立看门狗喂狗**：watchdogTask 最高优先级（osPriorityRealtime）独立喂狗，5 秒 IWDG 超时
- **任务心跳监控**：每个任务注册心跳，watchdogTask 监控超时（>5 秒），异常时日志告警
- **栈高水位监控**：logTask 每秒调用 osThreadEnumerate + osThreadGetStackSpace 输出最小剩余栈空间
- **告警立即上报**：sensorTask 检测到告警状态变化时立即推送云端，不等定时上报周期
- **Flash 参数持久化**：告警阈值、从站地址、采样周期等参数断电保存，CRC16 校验，云端可远程修改
- **云端命令系统**：支持 query/config/write/reboot/led 五种命令，双向 JSON 通信带 ACK/NACK
- **告警联动**：4 路告警（过温/欠温/过压/欠压）3 次消抖 + LED 指示 + 事件日志 + 云端上报
- **事件日志**：32 条环形缓冲区，记录告警触发/恢复、Modbus 上下线、云端连接、配置变更
- **Modbus RTU 主站**：03/06/10 功能码，多从站轮询，异常码分类，连续 3 次失败离线、自动恢复上线
- **UART2 互斥锁**：cloudTask 和 cmdTask 共享 UART2 发送，互斥锁保护避免数据交错
- **USART3 互斥锁**：modbusTask 和 cmdTask 共享 USART3/RS485，互斥锁保护避免并发冲突

---

## FreeRTOS 7 任务架构（4 级优先级）

| 优先级 | 任务 | 周期 | 栈 | 功能 |
|--------|------|------|-----|------|
| osPriorityRealtime | `watchdogTask` | 500ms | 2048B | 独立喂狗 + 7 任务心跳异常检测 |
| osPriorityAboveNormal | `sampleTask` | 200ms | 1024B | ADC 采样 + 滤波 + DHT11 + 告警检测 + 立即上报 |
| osPriorityAboveNormal | `modbusTask` | 1000ms | 2048B | Modbus RTU 主站多从站轮询 |
| osPriorityNormal | `cloudTask` | 500ms | 2048B | ESP32 JSON 收包 + 定时上报 + 命令投递到队列 |
| osPriorityNormal | `cmdTask` | 事件驱动 | 3072B | 从 cmdQueue 取命令异步处理（Flash 写入/Modbus 写等） |
| osPriorityBelowNormal | `ledTask` | 1000ms | 1024B | OLED 刷新 + 状态 LED 更新（Modbus/Cloud 离线指示） |
| osPriorityBelowNormal | `logTask` | 1000ms | 1024B | uptime 递增 + 完整状态日志 + 栈高水位 |

### 命令流架构

```
ESP32 → UART2 → cloudTask(收包) → cmdQueue(消息队列，16深度) → cmdTask(处理)
                                              ├── Flash 写入（config 命令）
                                              ├── Modbus 写（write 命令）
                                              ├── 数据查询（query 命令）
                                              ├── LED 控制（led 命令）
                                              └── 软复位（reboot 命令）
```

### 任务心跳与栈监控

- 每个任务在 `app_tasks.c` 中定义静态 `task_heartbeat_t` 结构体
- 任务循环首尾调用 `task_heartbeat_beat()` 更新时间戳
- `watchdogTask` 每 500ms 遍历所有心跳，>5 秒未更新则日志告警
- `logTask` 每秒调用 `osThreadEnumerate` + `osThreadGetStackSpace` 输出最小剩余栈空间

---

## ADC 数据采集（软件轮询模式）

- **CH0 (PB15)**: 板载 R37 电位器 → 软件触发轮询采样 → 滑动平均(窗口16)+中值滤波 → 电压换算（**实际使用**）
- **CH1 (PB12)**: R38 电位器 → 代码预留，硬件未接（读回浮空值，不影响功能）
- **CH2 (PA0)**: NTC 热敏电阻 → 代码预留，硬件未接，软件自动检测为 NC（无效温度）
- 使用 `HAL_ADC_PollForConversion` 单通道轮流采样，每次切换通道重新配置
- 12 位分辨率，3.3V 参考电压，采样时间 47.5 ADC 时钟周期
- 滤波算法：16 点滑动窗口 + 中值滤波（偏差 >30% 时用中值替代）

## DHT11 温湿度传感器

- **PA1** 单总线协议，开漏输出 + DWT 微秒级时序
- 使用 `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` 暂停任务调度，防止读取过程中被抢占破坏时序
- MCU 发 18ms 低电平启动信号 → DHT11 应答 80μs 低 + 80μs 高 → 40bit 数据
- 校验和校验（湿度高8+湿度低8+温度高8+温度低8 == 校验字节）
- 最小采样间隔 1500ms，过快读取会失败
- **必须外接 10kΩ 上拉电阻到 3.3V**，否则湿度一直显示 NC

## NTC 温度采集（预留，当前未接）

- B=3950K, R0=10kΩ@25°C, PA0 ← NTC + 10kΩ 分压电阻中点
- B 值公式: `T_K = 1 / (1/T0 + ln(R_ntc/R0)/B)`
- 自动检测传感器是否在线（ADC 值接近 0 或 4095 判定为未连接）

## 告警系统（4 路 + 3 次消抖 + LED 联动 + 立即上报）

| 告警 | 默认阈值 | 检测源 | LED 指示 |
|------|---------|--------|----------|
| 过温 | >60.0°C | CH2 (NTC，未接) | LED1 亮 |
| 欠温 | <-5.0°C | CH2 (NTC，未接) | LED2 亮 |
| 过压 | >3.2V | CH0 (PB15) | LED3 亮 |
| 欠压 | <0.3V | CH0 (PB15) | LED4 亮 |

> 过温/欠温依赖 CH2(NTC)，NTC 未接时不触发（读到无效温度 -273.15℃，被 -50℃ 下限排除）；过压/欠压用 CH0(R37) 可直接演示。

- 阈值可通过云端 config 命令修改并**断电保存到 Flash**
- 连续 3 次越限才触发告警，防止误报
- 告警触发/恢复时**立即写入事件日志**并**立即上报云端**（不等定时周期）
- 告警回调在临界区外执行，避免死锁

## LED 状态指示（8 路，74HC573 锁存器）

| LED | 功能 |
|-----|------|
| LED1-4 | 告警指示（过温/欠温/过压/欠压），由 sensorTask 的告警回调更新 |
| LED5 | Modbus 从机离线（红灯），由 ledTask 维护 |
| LED6 | 云端离线（红灯），由 ledTask 维护 |
| LED7-8 | 保留，可通过云端命令控制 |

- LED 与 LCD 共享 GPIOC 数据总线（PC8-PC15），使用 FreeRTOS 互斥锁 `g_lcd_led_bus_mutex` 串行访问
- PD2 为 74HC573 锁存使能（LE），高电平直通、低电平锁存

## Modbus RTU 主站

- 物理层：USART3 (PB10/PB11) + RS485 (MAX485/SP3485, PB13 控制 DE/RE)
- 功能码：**03**（读保持寄存器）+ **06**（写单个）+ **10**（写多个，已实现帧构建/解析）
- 多从站轮询：`slave_list[]` 可配置，轮询完当前从站自动切换到下一个
- 3.5 字符帧间隔：根据波特率（115200bps）动态计算 ≈ 2ms，用 osDelay(1) 轮询检查接收缓冲区长度稳定后认为一帧结束
- RS485 收发切换：发送前 DE 拉高 → DWT 延时 50μs → HAL_UART_Transmit → DWT 延时 50μs → DE 拉低
- 异常码分类：ILLEGAL_FUNC/ADDR/VAL/SLAVE_FAIL/ACK/BUSY
- 连续 3 次失败标记离线，离线时清零 remote_value（避免显示旧数据），恢复后自动上线
- 上下线事件写入事件日志
- USART3 互斥锁保护 modbusTask 轮询和 cmdTask 写寄存器不冲突

## Flash 参数持久化

- 使用 STM32G431 内部 Flash 末页（Page 63, 地址 0x0801F800）
- 参数结构体 8 字节对齐，支持双字（64 位）写入
- 三重保护：魔数 0xA5F0 校验 + CRC16 校验 + 默认值兜底
- 擦除前临时提升当前任务优先级到 osPriorityHigh，防止被抢占后在关中断状态进入不可预期的操作
- 保存内容：告警阈值（4 个 float）、Modbus 从站地址表、采样周期、云端上报周期、设备 ID
- 云端发送 `{"t":"config",...}` 即可修改并保存（cmdTask 异步处理 Flash 写入）

## 云端命令系统

### JSON 协议格式（换行 `\n` 分隔，带 CRC16 校验）

| 命令 | JSON 格式（ESP32→STM32） | 响应（STM32→ESP32） | 处理方式 |
|------|-----------|------|----------|
| 数据上报 | — | `{"t":"d","c0":...,"crc":...}\n` | cloudTask 定时发送 |
| 告警上报 | — | `{"t":"alarm","al":...,"uptime":...}\n` | sensorTask 触发立即发送 |
| 查询 | `{"t":"query"}\n` | `{"t":"query",...,"th_tempH":...,"crc":...}\n` | cmdTask 异步处理 |
| 配置 | `{"t":"config","temp_high":70.0}\n` | `{"t":"ack","cmd":"config","status":"ok","msg":"saved"}\n` | cmdTask 异步处理（Flash 写入） |
| 写寄存器 | `{"t":"write","reg":0,"val":100}\n` | `{"t":"ack","cmd":"write","status":"ok","msg":"ok"}\n` | cmdTask 异步处理（Modbus 写） |
| 软复位 | `{"t":"reboot"}\n` | `{"t":"ack","cmd":"reboot","status":"ok","msg":"rebooting"}\n` | cmdTask 异步处理 |
| LED 控制 | `{"t":"c","dev":"led","id":1,"act":"on"}\n` | `{"t":"ack","cmd":"led","status":"ok"}\n` | cmdTask 异步处理 |

- CRC16 校验：STM32 对「不含 `"crc"` 字段的完整 JSON（含结尾 `}`）」计算 CRC16-Modbus，然后插入 `,"crc":N` 字段
- ESP32 收到数据后还原 JSON 并验证 CRC，校验失败丢弃
- Cloud 在线判定：UART2 收到 ESP32 任何数据（包括 ping）即标记在线，超过 10 秒无收包标记离线
- ESP32 每 2 秒发送 ping 心跳，收到 STM32 数据也回 ping

## OLED 显示（10 行）

| 行 | 内容 | 颜色 |
|----|------|------|
| Line0 | 标题（Gateway v2.0 G431） | 绿色 |
| Line1 | 平台信息（CT117E-M4 FreeRTOS） | 白色 |
| Line2 | ADC CH0 原始值 + 电压（PB15/R37） | 黄色 |
| Line3 | DHT11 温度 + 湿度（或 NC） | 白色 |
| Line4 | 空行（预留） | — |
| Line5 | Modbus 状态（ON/OFF + 远程寄存器值 R/R1） | 绿/红 |
| Line6 | Modbus 统计（OK/Fail 计数） | 白色 |
| Line7 | Cloud 状态（ON/OFF + 上传次数） | 绿/红 |
| Line8 | 告警状态（NORMAL / 告警码） | 绿/红 |
| Line9 | 系统运行时间（HH:MM:SS） | 绿色 |

- 显示死区防抖：ADC ±2、电压 ±0.02V、温度 ±0.2°C、湿度 ±1%
- 只刷新内容变化的行，减少闪烁
- 初始化时屏幕依次闪过红→蓝→绿→黑，确认 LCD 控制器正常

## 事件日志

- 环形缓冲区 32 条
- 记录类型：告警触发/恢复、Modbus 上下线、云端连接/断开、系统启动、配置保存、用户命令
- 每条事件带时间戳（uptime 秒数）

## 串口架构

| 串口 | 引脚 | 用途 | 接收方式 |
|------|------|------|----------|
| USART1 | PA9/PA10 | 调试日志（DAP 虚拟 COM） | 单字节中断 + 线性 buffer |
| USART2 | PA2/PA3 | ESP32 JSON 双向通信 | 单字节中断 + 环形缓冲区（256B） |
| USART3 | PB10/PB11 | RS485/Modbus | 单字节中断 + 环形缓冲区（256B） |

- 全部 115200 8N1
- USART2 RX (PA3) 配置上拉，空闲时保持高电平抗干扰
- USART3 RX (PB11) 配置上拉

## 看门狗

- IWDG 独立看门狗：LSI 时钟（约 32kHz），Prescaler=256，Reload=625
- 超时时间 = (256 × 625) / 32000 ≈ 5 秒
- 由 `watchdogTask`（osPriorityRealtime，最高优先级）每 500ms 独立喂狗
- IWDG 初始化延后到长初始化（LCD/业务 init）之后，避免初始化超时导致白屏死循环复位
- SysTick 中断中不再喂狗

## 临界区保护

| 保护手段 | 用于 | 实现细节 |
|----------|------|----------|
| `__get_PRIMASK()` / `__disable_irq()` / `__set_PRIMASK()` | terminal_data 全局数据读写 | 保存/恢复 PRIMASK，支持嵌套锁 |
| FreeRTOS 互斥锁 `g_lcd_led_bus_mutex` | LCD/LED 共享 GPIOC 总线 | 100ms 超时，防止死锁 |
| FreeRTOS 互斥锁 `g_uart2_mutex` | UART2 发送（cloudTask/cmdTask 共享） | 100ms 超时 |
| FreeRTOS 互斥锁 `g_uart3_mutex` | USART3/RS485 发送（modbusTask/cmdTask 共享） | 200ms 超时 |
| `__DMB()` 内存屏障 | 环形缓冲区 head/tail 读写 | 防止 ARM Cortex-M4 指令重排 |
| `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` | DHT11 读取（微秒级时序） | 暂停任务调度 |

---

## 系统架构

```
┌─────────────┐   USB/DAP    ┌──────────┐
│   PC/Keil   │◄────────────►│  STM32   │◄── OLED(TFT LCD) + 板载 LED + 电位器
│ 串口助手    │   115200     │  主站    │◄── DHT11 (PA1, +10kΩ上拉)
└─────────────┘              └──┬───┬───┘
                                │   │
              USART3+RS485      │   │ USART2 JSON (CRC16)
                                │   │
                         ┌──────▼┐ ┌▼──────────┐     WiFi
                         │Modbus │ │  ESP32    │◄────────► 手机浏览器
                         │从站/PC│ │ WebSocket │            控制面板
                         └───────┘ └────┬──────┘
                                        │ GPIO
                                   面包板 4 LED
```

---

## 引脚分配

### STM32 引脚

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| LD1-LD8 数据 | `PC8-PC15` | GPIO 输出 | 锁存器 74HC573，与 LCD 共享总线 |
| LD1-LD8 锁存 | `PD2` | GPIO 输出 | LE 使能（高直通，低锁存） |
| LCD 控制 | `PB5/PB8/PB9` | GPIO 输出 | NWR/RS/CS |
| LCD 读使能 | `PA8` | GPIO 输出 | NRD |
| 按键 | `PB0/PB1/PB2` | GPIO 输入 | 上拉，板载按键 |
| 调试日志 | `PA9/PA10` | USART1 | 115200，板载 DAP 虚拟串口 |
| ESP32 | `PA2(TX)/PA3(RX)` | USART2 | 115200，JSON 协议，RX 上拉 |
| RS485 | `PB10(TX)/PB11(RX)` | USART3 | Modbus RTU，RX 上拉 |
| RS485 方向 | `PB13` | GPIO | MAX485 DE/RE 短接 |
| ADC CH0 | `PB15` | ADC2_IN15 | 板载 R37 电位器（实际使用） |
| ADC CH1 | `PB12` | ADC2_IN12 | R38 电位器（代码预留，未接） |
| ADC CH2 | `PA0` | ADC2_IN1 | NTC（代码预留，未接） |
| DHT11 | `PA1` | GPIO | 单总线，开漏输出，需 10kΩ 上拉 |
| SWD | `PA13/PA14` | SWDIO/SWCLK | 板载 DAP |

### ESP32 引脚

| ESP32 引脚 | 连接 | 说明 |
|------------|------|------|
| GPIO17 (TX2) | STM32 **PA3** (RX) | 交叉连接 |
| GPIO16 (RX2) | STM32 **PA2** (TX) | 交叉连接 |
| GND | STM32 GND | **必须共地** |
| GPIO4 | 面包板 LED 红（串 220Ω 到 GND） | |
| GPIO5 | 面包板 LED 绿（串 220Ω 到 GND） | |
| GPIO18 | 面包板 LED 蓝（串 220Ω 到 GND） | |
| GPIO19 | 面包板 LED 黄（串 220Ω 到 GND） | |

---

## 软件结构

```text
code/
  App/
    app_tasks.c/h              FreeRTOS 7 任务入口 + 心跳管理 + 告警 LED 联动
  Bsp/
    bsp_led.c/h                LD1-LD8 锁存 LED 驱动 + 总线互斥锁
    bsp_uart.c/h               USART1/2/3 中断接收 + 环形缓冲区
    bsp_rs485.c/h              MAX485 方向控制（DWT 精确延时）
    bsp_adc.c/h                ADC2 软件轮询采样
    bsp_lcd.c/h                TFT LCD 显示（10 行 + 死区防抖）
    bsp_dht11.c/h              DHT11 单总线驱动（PA1，含 DWT 微秒延时）
  Common/
    crc16.c/h                  Modbus CRC16 校验
    log.c/h                    USART1 日志输出
    filter.c/h                 滑动平均+中值滤波（窗口16）
    ring.c/h                   无锁环形缓冲区（SPSC, 256B, __DMB 内存屏障）
  Protocol/
    modbus_master.c/h          03/06/10 帧构建与解析 + 异常码
  Service/
    sample_service.c/h         本地 ADC 采样 + 滤波 + NTC + DHT11 + 告警
    modbus_service.c/h         Modbus 多从站轮询引擎 + 事件日志 + UART3 互斥锁
    cloud_service.c/h          ESP32 JSON 双向通信 + 命令队列 + 告警上报 + UART2 互斥锁
    terminal_service.c/h       终端状态统一管理（嵌套锁安全 + 告警回调）
    terminal_data.h            终端数据结构体 + 告警位图 + uptime
    flash_config.c/h           Flash 参数持久化（CRC16 校验 + 双字写入 + 优先级提升）
    event_log.c/h              事件日志环形缓冲区（32 条）
    ntc_service.c/h            NTC B值温度补偿
    dht11_service.c/h          DHT11 服务层（1500ms 最小间隔控制）
esp32_gateway/
    esp32_gateway.ino          ESP32 固件（WiFi+WebSocket+HTTP+数据缓存+CRC校验）
Core/
  Inc/FreeRTOSConfig.h        FreeRTOS 配置（堆 24000B，heap_4，trace facility 启用）
  Src/main.c                  主函数 + 复位原因日志
  Src/app_freertos.c          7 任务创建 + cmdQueue + UART2 互斥锁 + LCD/LED 总线互斥锁
  Src/iwdg.c                  IWDG 初始化（Prescaler=256, Reload=625）
  Src/stm32g4xx_it.c          中断服务函数（3 路 UART IRQ）
```

---

## 运行日志示例

```
[RESET] CSR=0x1C000000 BOR/POR
[FLASH] config loaded OK
[FLASH] dev_id=GW-G431-001 tempH=60.0 tempL=-5.0 vH=3.20 vL=0.30 slaves=1 sample=1000ms cloud=500ms
[ADC] init OK: poll mode PB15+PB12+PA0
[SAMPLE] service init OK
[DHT11] init OK, PA1 ready
[BOOT] 7-task system start
[CLOUD] task started
[CMD] task started
[CLOUD] ESP32 link UP (UART2 RX)
[SAMPLE] ch0=2048(2050) 1.65V  ch1=1024(1028) 0.83V  ch2(NTC)=512(515) -273.1C NC
[DHT11] humidity=55% temp=26C
[STATE] c0=2050 c1=1028 c2=515 v0=1.65V v1=0.83V temp=26.0C r=100 r1=200 mb=ON cloud=ON s=100 ok=95 fail=5 up=20 al=0x00 hum=55% uptime=120s stack=512
[CLOUD] tick=10 c0=2048 c1=1024 temp=-273.1C mb=ON up=10 alarm=0x00 uptime=60s
```

---

## 编译准备

### Keil 工程需添加的用户文件

- `code/App/app_tasks.c`
- `code/Bsp/bsp_dht11.c`、`bsp_led.c`、`bsp_uart.c`、`bsp_rs485.c`、`bsp_adc.c`、`bsp_lcd.c`
- `code/Common/filter.c`、`ring.c`、`crc16.c`、`log.c`
- `code/Protocol/modbus_master.c`
- `code/Service/sample_service.c`、`modbus_service.c`、`cloud_service.c`、`terminal_service.c`、`flash_config.c`、`event_log.c`、`dht11_service.c`、`ntc_service.c`
- `Core/Src/iwdg.c`
- `Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_iwdg.c`

### ESP32 (Arduino IDE)

- 安装开发板：ESP32 by Espressif Systems
- 安装库：ArduinoJson (Benoit Blanchon)
- 打开 `esp32_gateway/esp32_gateway.ino`，修改 WiFi SSID/密码后上传

---

## 关键设计决策

| 决策 | 原因 |
|------|------|
| ADC 用软件轮询而非 DMA | 轮询模式切换通道更灵活，200ms 采样周期轮询完全够用，避免 DMA 配置复杂性 |
| watchdogTask 独立喂狗 | 之前放在 idle 任务中喂狗导致高优先级任务阻塞时看门狗超时白屏复位 |
| IWDG 延后初始化 | 避免 LCD/业务 init 超时导致启动阶段死循环复位 |
| cloudTask 和 cmdTask 分离 | Flash 写入约 20-25ms 关中断，分离后不影响 cloudTask 继续收包 |
| UART2/UART3 用环形缓冲区而非 DMA | 中断接收 + 环形缓冲区更灵活，配合粘包处理和帧间隔检测 |
| DHT11 读取时关任务调度 | 单总线微秒级时序，任务切换会破坏时序导致读取失败 |
| Flash 擦除前提权 | 擦除期间关中断，提权防止被抢占后进入不可预期的状态 |
| JSON CRC16 校验 | UART 传输无硬件校验，CRC16 保证数据完整性 |
| Cloud 在线判定基于 RX 超时 | 比连接状态更可靠，真正反映通信链路是否活跃 |

---

> **相关文档**: `接线清单.md`（完整接线清单）· `接线与运行步骤.md`（联调手册）· `秋招面试带教文档.md`（面试准备）
