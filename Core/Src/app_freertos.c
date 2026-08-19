/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @attention
  * Copyright (c) 2026  Gateway Project
  * FreeRTOS 任务创建：4 级优先级设计（高→低：Modbus/Sample > Cloud > LED/Log）
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_tasks.h"
#include "cloud_service.h"
#include "log.h"
#include "bsp_led.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for watchdogTask */
osThreadId_t watchdogTaskHandle;
const osThreadAttr_t watchdogTask_attributes = {
  .name = "watchdogTask",
  .priority = (osPriority_t) osPriorityRealtime,
  .stack_size = 512 * 4
};
/* Definitions for modbusTask */
osThreadId_t modbusTaskHandle;
const osThreadAttr_t modbusTask_attributes = {
  .name = "modbusTask",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 512 * 4
};
/* Definitions for sampleTask */
osThreadId_t sampleTaskHandle;
const osThreadAttr_t sampleTask_attributes = {
  .name = "sampleTask",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 256 * 4
};
/* Definitions for cloudTask */
osThreadId_t cloudTaskHandle;
const osThreadAttr_t cloudTask_attributes = {
  .name = "cloudTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for cmdTask */
osThreadId_t cmdTaskHandle;
const osThreadAttr_t cmdTask_attributes = {
  .name = "cmdTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 768 * 4
};
/* Definitions for ledTask */
osThreadId_t ledTaskHandle;
const osThreadAttr_t ledTask_attributes = {
  .name = "ledTask",
  .priority = (osPriority_t) osPriorityBelowNormal,
  .stack_size = 256 * 4
};
/* Definitions for logTask */
osThreadId_t logTaskHandle;
const osThreadAttr_t logTask_attributes = {
  .name = "logTask",
  .priority = (osPriority_t) osPriorityBelowNormal,
  .stack_size = 256 * 4
};
/* Definitions for sampleQueue */
osMessageQueueId_t sampleQueueHandle;
const osMessageQueueAttr_t sampleQueue_attributes = {
  .name = "sampleQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void MX_FREERTOS_Init(void);
/* USER CODE END FunctionPrototypes */

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationIdleHook(void);

/* USER CODE BEGIN 2 */
void vApplicationIdleHook( void )
{
   /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
}
/* USER CODE END 2 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* LCD 与 LED 共享 GPIOC 数据总线，创建互斥锁防止刷新时相互抢占 */
  g_lcd_led_bus_mutex = osMutexNew(NULL);
  if (g_lcd_led_bus_mutex == NULL) {
      log_info("[ERROR] LCD/LED bus mutex create failed\r\n");
  }

  /* UART2 互斥锁：cloudTask 和 cmdTask 共享 UART2 发送 */
  g_uart2_mutex = osMutexNew(NULL);
  if (g_uart2_mutex == NULL) {
      log_info("[ERROR] UART2 mutex create failed\r\n");
  }
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */

  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */

  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of sampleQueue */
  sampleQueueHandle = osMessageQueueNew (8, 4, &sampleQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* 命令队列：cloudTask 投递，cmdTask 处理 */
  g_cmd_queue = osMessageQueueNew(16, sizeof(cloud_cmd_t), NULL);
  if (g_cmd_queue == NULL) {
      log_info("[ERROR] cmdQueue create failed\r\n");
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of watchdogTask */
  watchdogTaskHandle = osThreadNew(app_watchdog_task, NULL, &watchdogTask_attributes);

  /* creation of modbusTask */
  modbusTaskHandle = osThreadNew(app_modbus_task, NULL, &modbusTask_attributes);

  /* creation of sampleTask */
  sampleTaskHandle = osThreadNew(app_sensor_task, NULL, &sampleTask_attributes);

  /* creation of cloudTask */
  cloudTaskHandle = osThreadNew(app_cloud_task, NULL, &cloudTask_attributes);
  if (cloudTaskHandle == NULL) {
    log_info("[ERROR] cloudTask create failed! Check heap size.\r\n");
  }

  /* creation of cmdTask */
  cmdTaskHandle = osThreadNew(app_cmd_task, NULL, &cmdTask_attributes);

  /* creation of ledTask */
  ledTaskHandle = osThreadNew(app_led_task, NULL, &ledTask_attributes);

  /* creation of logTask */
  logTaskHandle = osThreadNew(app_log_task, NULL, &logTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* ================================================================
     7 任务架构（优先级从高到低）：
       osPriorityRealtime:     watchdogTask (500ms, 独立喂狗+心跳检测)
       osPriorityAboveNormal:  modbusTask   (1000ms, Modbus RTU 主站轮询)
                               sampleTask   (200ms, 传感器采集+滤波+告警)
       osPriorityNormal:       cloudTask    (500ms, 云端数据上报+命令接收)
                               cmdTask      (事件驱动, 云端命令处理)
       osPriorityBelowNormal:  ledTask      (1000ms, OLED显示+状态LED)
                               logTask      (1000ms, 串口日志+运行时间)
     ================================================================ */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  /* USER CODE END RTOS_EVENTS */

}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

