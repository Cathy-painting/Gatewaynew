基于 STM32G431RBT6 与 FreeRTOS 的轻量化工业数据采集与云传输终端
<div align="center">
</div>
📖 项目简介
项目基于 STM32G431RBT6 微控制器和 FreeRTOS 实时操作系统，实现了本地模拟量采集、RS485/Modbus RTU 工业总线通信、终端状态统一管理、异常处理以及MQTT 云数据上传核心功能。严格遵循 "先跑通再优化、先假数据后真硬件" 的开发原则，避开非科班学生容易踩的硬件坑，重点突出软件架构设计和工程化能力，是嵌入式、物联网、工业软件方向暑假实习的高性价比项目。
✨ 项目亮点
🎯 实习导向：覆盖 90% 以上嵌入式 / 物联网实习岗位核心技能点，远超普通 LED + 串口 demo
🛡️ 避坑设计：先假数据后真硬件，先跑通后优化，确保 1.5 个月内必出成果
🏗️ 工程化架构：模块化分层设计，代码可维护性强，体现专业软工素养
🎬 可演示性强：可视化运行指示灯、结构化串口日志、MQTT 客户端实时数据展示
⚠️ 异常处理完善：支持 Modbus 超时重试、CRC 校验、连续失败离线、恢复上线自动检测
🚀 赛道差异化：纯软工学生少有的工业物联网项目，简历竞争力远超普通 Web/APP 项目
📋 目录
技术栈与开发环境
硬件清单与引脚分配
项目架构
快速开始
演示效果
目录结构
常见问题
开发进度计划
实习简历参考
许可证
致谢
🛠️ 技术栈与开发环境
表格
类别	技术 / 工具	版本要求
核心硬件	STM32G431RBT6 开发板、MAX485 模块、ESP8266-01S	ESP8266 需刷支持 MQTT 的 AT 固件
辅助硬件	USB 转 TTL 模块、USB 转 RS485 模块、10k 电位器 (可选)、ST-Link V2	-
开发工具	STM32CubeMX、Keil MDK-ARM、ST-Link 驱动	CubeMX 6.10+、Keil 5.38+
调试工具	XCOM/SSCOM 串口助手、MQTTX、Modbus Slave	最新稳定版即可
核心技术	STM32 HAL 库 (GPIO/USART/ADC)、FreeRTOS (任务 / 消息队列)、Modbus RTU 协议、CRC16 校验、ESP8266 AT 指令、JSON 数据封装	-
📦 硬件清单与引脚分配
必备硬件清单
表格
序号	元器件名称	数量	备注
1	STM32G431RBT6 开发板	1	推荐带板载 DAP 调试器版本
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
🏗️ 项目架构
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
🚀 快速开始
步骤 1：硬件连接
按照 "核心引脚分配" 表连接所有模块，特别注意：
ESP8266 必须使用 3.3V 供电，禁止接 5V
RS485 总线 A 接 A、B 接 B，共地必须连接
ADC 输入电压范围 0-3.3V，禁止超过
步骤 2：工程配置与编译
克隆本项目到本地
bash
运行
git clone https://github.com/你的用户名/STM32-Industrial-Data-Gateway.git
打开01_CubeMX工程/Gateway_Minimal.ioc
确认以下核心配置：
SYS → Debug：Serial Wire
RCC → High Speed Clock：Crystal/Ceramic Resonator(24MHz)
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
确认串口每秒输出结构化日志
Modbus 通信测试
打开 Modbus Slave，新建连接：从站地址 1，波特率 9600 8N1
配置寄存器 0 值为 123，寄存器 1 值为 456
连接 USB 转 RS485 模块，观察串口日志是否显示读取成功
断开 RS485 线，3 秒后日志显示离线；重新连接自动恢复
MQTT 云传输测试
打开 MQTTX，连接broker.emqx.io:1883
订阅主题gateway/stm32/data
确认每 5 秒收到一条 JSON 格式的终端数据
🎬 演示效果
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
演示截图
此处替换为你的实际截图
开发板实物图
串口日志截图
MQTTX 数据接收截图
Modbus Slave 配置截图
演示视频
此处替换为你的视频链接（推荐上传至 B 站或 GitHub Releases）
30 秒快速演示：点击观看
2 分钟完整演示：点击观看
📂 目录结构
plaintext
Project_Workspace/
├── 00_资料/          # 芯片手册、模块datasheet、参考资料
├── 01_CubeMX工程/    # STM32CubeMX配置文件(.ioc)
├── 02_Keil工程/      # Keil MDK工程源码
├── 03_笔记/          # 每日开发笔记(按DayXX_主题.txt命名)
├── 04_截图/          # 开发过程截图
├── 05_串口日志/      # 调试过程中保存的串口日志
├── 06_演示视频/      # 项目演示视频
└── 07_简历材料/      # 项目描述、面试讲解稿、常见问题答案
❓ 常见问题
<details>
<summary>Keil下载失败怎么办？</summary>
<ul>
<li>检查CubeMX中SYS是否配置为Serial Wire</li>
<li>确认ST-Link驱动已正确安装</li>
<li>尝试按住开发板复位键后再点击下载</li>
<li>检查ST-Link与开发板的SWDIO/SWCLK接线是否正确</li>
</ul>
</details>
<details>
<summary>串口无输出怎么办？</summary>
<ul>
<li>检查TX/RX是否接反（交叉连接）</li>
<li>确认串口助手波特率为115200，8位数据位，1位停止位，无校验</li>
<li>在设备管理器中查看正确的COM口号</li>
<li>确认板载DAP虚拟串口已被正确识别</li>
</ul>
</details>
<details>
<summary>开FreeRTOS后系统卡死怎么办？</summary>
<ul>
<li>检查CubeMX中SYS→Timebase Source是否改为TIM6/TIM7（不能用SysTick）</li>
<li>确认任务函数中包含osDelay()，否则会占用全部CPU资源</li>
<li>适当增大任务栈大小，避免栈溢出</li>
</ul>
</details>
<details>
<summary>Modbus通信无响应怎么办？</summary>
<ul>
<li>检查RS485 A/B线是否接反，尝试交换A/B线</li>
<li>确认RS485_DE引脚电平控制正确（发送时高电平，接收时低电平）</li>
<li>统一主从站的波特率、数据位、停止位、校验位</li>
<li>确认从站地址与程序中配置的一致</li>
</ul>
</details>
<details>
<summary>ESP8266无响应怎么办？</summary>
<ul>
<li>使用独立3.3V电源供电，避免开发板供电不足</li>
<li>检查TX/RX是否交叉连接</li>
<li>尝试不同波特率（常见：115200、9600、74880）</li>
<li>刷写支持MQTT指令的官方AT固件</li>
</ul>
</details>
📅 开发进度计划
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
📝 实习简历参考
项目名称
基于 STM32G431RBT6 与 FreeRTOS 的轻量化工业数据采集与云传输终端
项目描述
基于 STM32G431RBT6 和 FreeRTOS 设计轻量化工业数据采集与云传输终端，实现本地模拟量采集、RS485/Modbus RTU 主站轮询、ESP8266 联网和 MQTT 数据上传。项目采用多任务架构划分采样、通信、日志和云传输模块，并设计统一数据结构管理终端状态，支持通信超时、重试、离线检测和恢复上线日志输出。
项目职责与亮点
使用 STM32CubeMX 完成 GPIO、USART、ADC、FreeRTOS 等外设配置
基于 FreeRTOS 实现 5 个独立任务，完成系统功能解耦和调度
封装 LED、UART、RS485 等 BSP 模块，采用分层架构提升代码可维护性
实现 Modbus RTU 主站 03/06 功能，完成 CRC16 校验和响应解析
设计终端状态管理模块，整合本地与远程数据，支持异常状态检测
通过 ESP8266 AT 指令实现 Wi-Fi 连接和 MQTT JSON 数据上传
📄 许可证
本项目仅供个人学习交流使用，禁止用于商业用途。
🙏 致谢
感谢所有开源社区贡献者提供的参考资料和工具。
<div align="center">
如果这个项目对你有帮助，欢迎点个 Star ⭐ 支持一下！
</div>
