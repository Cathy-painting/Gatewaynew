基于 STM32G431RBT6 与 FreeRTOS 的轻量化工业数据采集与云传输终端
项目简介
本项目是专为大三软件工程专业学生设计的嵌入式 / 物联网入门实战项目，目标是在 1.5 个月内完成一个可演示、可写进简历、能清晰讲解的完整工业级原型。项目基于 STM32G431RBT6 微控制器和 FreeRTOS 实时操作系统，实现了本地模拟量采集、RS485/Modbus RTU 工业总线通信、终端状态统一管理、异常处理以及MQTT 云数据上传核心功能。
项目严格遵循 "先跑通再优化、先假数据后真硬件" 的开发原则，避开非科班学生容易踩的硬件坑，重点突出软件架构设计和工程化能力，是嵌入式、物联网、工业软件方向暑假实习的高性价比项目。
技术栈
表格
类别	技术 / 工具
核心硬件	STM32G431RBT6 开发板、MAX485 模块、ESP8266-01S (AT 固件)
辅助硬件	USB 转 TTL 模块、USB 转 RS485 模块、10k 电位器 (可选)、ST-Link 下载器
开发工具	STM32CubeMX 6.10+、Keil MDK-ARM 5.38+、ST-Link 驱动
调试工具	XCOM/SSCOM 串口助手、MQTTX、Modbus Slave
核心技术	STM32 HAL 库 (GPIO/USART/ADC)、FreeRTOS (任务 / 消息队列)、Modbus RTU 协议、CRC16 校验、ESP8266 AT 指令、JSON 数据封装
硬件清单与引脚分配
必备硬件清单
表格
序号	元器件名称	数量	备注
1	STM32G431RBT6 开发板	1	核心控制板，推荐带板载 DAP 调试器版本
2	ST-Link V2 下载器	1	开发板无板载调试器时必备
3	USB 转 TTL 模块	1	备用串口调试
4	MAX485 电平转换模块	1	RS485 总线通信
5	USB 转 RS485 模块	1	电脑端 Modbus 从站模拟
6	ESP8266-01S 模块	1	必须刷支持 MQTT 的 AT 固件
7	杜邦线 (公对公 / 公对母)	若干	模块连接
8	10k 电位器	1	ADC 真实采样测试 (可选)
核心引脚分配 (固定)
表格
功能	STM32 引脚	连接对象
系统运行指示灯	PB9	板载 LD9
调试日志串口	PA2(USART2_TX)、PA3(USART2_RX)	板载 DAP 虚拟串口
RS485 通信	PB10(USART3_TX)、PB11(USART3_RX)、PB13(DE)	MAX485 模块 DI/RO/DE
ESP8266 通信	PA9(USART1_TX)、PA10(USART1_RX)	ESP8266 RX/TX
ADC 采样 (可选)	PA1	电位器中间引脚
SWD 下载调试	PA13(SWDIO)、PA14(SWCLK)	ST-Link 下载器
软件环境搭建
安装 STM32CubeMX：从 ST 官网下载最新版本，安装后在 "Manage Embedded Software Packages" 中下载STM32CubeG4固件包
安装 Keil MDK-ARM：安装 5.38 及以上版本，完成激活
安装 ST-Link 驱动：开发板无板载调试器时需单独安装
安装调试工具：
串口助手：推荐 XCOM 或 MobaXterm
MQTT 客户端：MQTTX (官网免费下载)
Modbus 模拟工具：Modbus Slave
项目架构
软件分层设计 (模块化)
plaintext
Project_Workspace/02_Keil工程/
├── App/                # 应用层：FreeRTOS任务实现
│   ├── freertos.c      # 任务入口与初始化
│   └── tasks/          # 各任务独立实现
├── Bsp/                # 板级支持包：硬件驱动封装
│   ├── bsp_led.c/h     # LED驱动
│   ├── bsp_uart.c/h    # 串口驱动
│   ├── bsp_adc.c/h     # ADC驱动(可选)
│   └── bsp_rs485.c/h   # RS485驱动
├── Common/             # 通用工具模块
│   ├── log.c/h         # 结构化日志系统
│   └── crc16.c/h       # Modbus CRC16校验算法
├── Protocol/           # 通信协议层
│   └── modbus_master.c/h # Modbus RTU主站协议实现
├── Service/            # 业务服务层
│   ├── terminal_service.c/h # 终端状态统一管理
│   └── esp8266_at.c/h  # ESP8266 AT指令封装
└── Core/               # STM32CubeMX自动生成代码
FreeRTOS 任务划分
表格
任务名称	优先级	栈大小	运行周期	核心功能
ledTask	osPriorityLow	128	500ms	系统运行状态指示，LED 周期闪烁
sampleTask	osPriorityNormal	256	1000ms	本地数据采集 (假数据 / ADC)
modbusTask	osPriorityNormal	512	1000ms	Modbus RTU 主站轮询、响应解析
logTask	osPriorityLow	512	2000ms	系统状态统一打印、日志输出
cloudTask	osPriorityBelowNormal	768	5000ms	ESP8266 控制、MQTT 数据上传
核心功能实现
系统运行监控：LED 500ms 周期闪烁，直观判断系统是否正常运行
本地数据采集：
默认软件模拟 0-4095 递增采样值，不依赖硬件
可选 PA1 引脚 ADC 真实采样，支持电位器调节
Modbus RTU 工业通信：
主站模式，支持 03 功能码 (读保持寄存器) 和 06 功能码 (写单寄存器)
完整 CRC16 校验，确保数据传输正确性
终端状态管理：
统一数据结构管理本地值、远程值、在线状态、统计计数
支持通信超时、连续失败离线、恢复上线自动检测
结构化日志系统：通过 USART2 输出分级日志，格式统一便于调试
云数据传输：
ESP8266 通过 AT 指令连接 2.4GHz Wi-Fi
连接公共 MQTT Broker (broker.emqx.io)
终端状态以 JSON 格式定时上传
基础异常处理：Modbus 超时重试、CRC 错误统计、ESP8266 断线重连
快速开始
步骤 1：硬件连接
按照 "核心引脚分配" 表连接所有模块，特别注意：
ESP8266 必须使用 3.3V 供电，禁止接 5V
RS485 总线 A 接 A、B 接 B，共地必须连接
ADC 输入电压范围 0-3.3V，禁止超过
步骤 2：工程配置与编译
克隆本项目到本地，打开01_CubeMX工程/Gateway_Minimal.ioc
确认以下配置：
SYS→Debug：Serial Wire
RCC→High Speed Clock：Crystal/Ceramic Resonator(24MHz)
所有外设引脚与 "核心引脚分配" 一致
点击 "GENERATE CODE" 生成 Keil 工程
打开02_Keil工程/Gateway_Minimal.uvprojx
点击 "Build" 编译工程，确保 0 错误
步骤 3：程序下载与运行
连接 ST-Link 下载器到开发板和电脑
点击 Keil 工具栏 "Download" 按钮下载程序
按下开发板复位键，程序开始运行
步骤 4：功能测试
基础功能测试
观察板载 LD9 是否 500ms 闪烁
打开串口助手，选择板载 DAP 对应的 COM 口，波特率 115200
确认串口每秒输出结构化日志，包含本地采样值和系统状态
Modbus 通信测试
打开 Modbus Slave 软件，新建连接，设置从站地址 1，波特率 9600 8N1
配置寄存器 0 地址值为 123，寄存器 1 地址值为 456
连接 USB 转 RS485 模块到电脑，A 接 MAX485 的 A，B 接 MAX485 的 B
观察串口日志，确认显示[MODBUS] rx ok remote=123
断开 RS485 线，3 秒后日志显示[MODBUS] timeout fail=3和online=0
重新连接 RS485 线，日志显示online=1，恢复正常读取
MQTT 云传输测试
打开 MQTTX，新建连接：
名称：gateway_test
主机：broker.emqx.io
端口：1883
用户名 / 密码：留空
点击连接，订阅主题gateway/stm32/data
确认每 5 秒收到一条 JSON 格式的终端数据
转动电位器 (ADC 模式)，观察 JSON 中local字段变化
目录结构
plaintext
Project_Workspace/
├── 00_资料/          # 芯片手册、模块 datasheet、参考资料
├── 01_CubeMX工程/    # STM32CubeMX配置文件(.ioc)
├── 02_Keil工程/      # Keil MDK工程源码
├── 03_笔记/          # 每日开发笔记(按DayXX_主题.txt命名)
├── 04_截图/          # CubeMX配置、Keil工程、串口日志、MQTT截图
├── 05_串口日志/      # 调试过程中保存的串口日志文件
├── 06_演示视频/      # 30秒短视频、2分钟完整演示视频
└── 07_简历材料/      # 项目描述、面试讲解稿、常见问题答案
演示效果
串口日志示例
plaintext
[BOOT] system start
[RTOS] tasks created successfully
[SAMPLE] local=10
[MODBUS] tx: 01 03 00 00 00 02 C4 0B
[MODBUS] rx ok, reg0=123, reg1=456
[STATE] local=10 remote=123 online=1 sample=1 mb_ok=1 mb_fail=0 upload=0
[SAMPLE] local=20
[MODBUS] tx: 01 03 00 00 00 02 C4 0B
[MODBUS] rx ok, reg0=123, reg1=456
[STATE] local=20 remote=123 online=1 sample=2 mb_ok=2 mb_fail=0 upload=0
[CLOUD] MQTT publish success, topic=gateway/stm32/data
MQTT 上传数据格式
json
{
  "local": 1234,
  "remote": 456,
  "online": 1,
  "sample_count": 120,
  "mb_ok": 115,
  "mb_fail": 5,
  "upload_count": 24
}
常见问题与解决方案
表格
问题现象	可能原因	解决方案
Keil 下载失败	SYS 未配置 Serial Wire；ST-Link 驱动未安装	重新配置 CubeMX 的 SYS 选项；重装 ST-Link 驱动；按住复位键下载
串口无输出	TX/RX 接反；波特率错误；COM 口选择错误	交叉 TX/RX；确认波特率 115200；在设备管理器查看正确 COM 口
开 FreeRTOS 后系统卡死	Timebase Source 未改为 TIM6/TIM7	CubeMX 中 SYS→Timebase Source 选择 TIM6
Modbus 通信无响应	A/B 线接反；DE 引脚控制错误；波特率不匹配	交换 A/B 线；检查 RS485_DE 引脚电平；统一主从站波特率
ESP8266 无响应	供电不足；TX/RX 接反；固件不支持 MQTT	使用独立 3.3V 电源；交叉 TX/RX；刷写支持 MQTT 的 AT 固件
ESP8266 连不上 Wi-Fi	不支持 5GHz Wi-Fi；名称含中文；密码错误	连接 2.4GHz Wi-Fi；修改 Wi-Fi 名称为英文；确认密码正确
开发进度与优先级
1.5 个月标准开发计划
表格
周次	核心任务	验收标准
第 1 周	环境搭建、最小工程、FreeRTOS 双任务、消息队列、工程分层	LED 闪烁 + 串口打印；两个任务独立运行；Queue 能传递数据
第 2 周	本地采样 (假数据)、USART3 配置、RS485 物理层调试	本地值周期变化；USART3 能收发数据；RS485 环回测试通过
第 3 周	CRC16 校验、Modbus 03/06 功能、modbusTask	能发出正确 Modbus 帧；能解析从站响应；支持写寄存器
第 4 周	终端状态整合、日志系统、异常处理、本地版验收	串口输出完整状态；断线显示 offline；恢复自动上线
第 5 周	ESP8266 AT 指令、Wi-Fi 连接、MQTT 通信、JSON 上传	ESP8266 返回 OK；连接 Wi-Fi 成功；MQTTX 收到数据
第 6 周	完整联调、稳定性优化、演示材料、简历准备	所有模块同时运行无冲突；录制演示视频；完成简历描述
优先级说明 (时间紧张时)
✅ 必须完成：最小工程、FreeRTOS 多任务、本地采样、Modbus 03、状态管理、异常处理
⚠️ 尽量完成：ADC 真实采样、Modbus 06、日志系统
⭐ 加分项：ESP8266 Wi-Fi、MQTT 云上传
最低可交付版本
若时间不足，完成以下核心功能即可满足实习简历要求：
基于 STM32G431RBT6 和 FreeRTOS 的轻量化工业数据采集终端，实现本地数据采样、RS485/Modbus RTU 主站轮询和串口日志监控。采用多任务架构划分模块，封装基础 BSP 接口，支持 Modbus CRC 校验、通信超时、连续失败离线和恢复上线机制。
许可证
本项目仅供个人学习交流使用，禁止用于商业用途。
致谢
感谢所有开源社区贡献者提供的参考资料和工具。
