/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
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
#include "log.h"
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
/* Definitions for ledTask */
osThreadId_t ledTaskHandle;
const osThreadAttr_t ledTask_attributes = {
  .name = "ledTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* Definitions for logTask */
osThreadId_t logTaskHandle;
const osThreadAttr_t logTask_attributes = {
  .name = "logTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};
/* Definitions for senderTask */
osThreadId_t senderTaskHandle;
const osThreadAttr_t senderTask_attributes = {
  .name = "senderTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};
/* Definitions for receiverTask */
osThreadId_t receiverTaskHandle;
const osThreadAttr_t receiverTask_attributes = {
  .name = "receiverTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};
/* Definitions for sampleTask */
osThreadId_t sampleTaskHandle;
const osThreadAttr_t sampleTask_attributes = {
  .name = "sampleTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};
/* Definitions for sampleQueue */
osMessageQueueId_t sampleQueueHandle;
const osMessageQueueAttr_t sampleQueue_attributes = {
  .name = "sampleQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartLedTask(void *argument);
void StartModbusTask(void *argument);
void StartLogTask(void *argument);
void StartCloudTask(void *argument);
void StartSenderTask(void *argument);
void StartReceiverTask(void *argument);
void StartSampleTask(void *argument);
void MX_FREERTOS_Init(void);
/* USER CODE END FunctionPrototypes */

void StartLedTask(void *argument);
void StartLogTask(void *argument);
void StartSenderTask(void *argument);
void StartReceiverTask(void *argument);
void StartSampleTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of sampleQueue */
  sampleQueueHandle = osMessageQueueNew (8, 4, &sampleQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of ledTask */
  ledTaskHandle = osThreadNew(StartLedTask, NULL, &ledTask_attributes);

  /* creation of logTask */
  logTaskHandle = osThreadNew(StartLogTask, NULL, &logTask_attributes);

  /* creation of senderTask */
  senderTaskHandle = osThreadNew(StartSenderTask, NULL, &senderTask_attributes);

  /* creation of receiverTask */
  receiverTaskHandle = osThreadNew(StartReceiverTask, NULL, &receiverTask_attributes);

  /* creation of sampleTask */
  sampleTaskHandle = osThreadNew(StartSampleTask, NULL, &sampleTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add modbusTask and cloudTask */
  const osThreadAttr_t modbusTask_attributes = {
    .name = "modbusTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 512 * 4
  };
  osThreadNew(StartModbusTask, NULL, &modbusTask_attributes);

  const osThreadAttr_t cloudTask_attributes = {
    .name = "cloudTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 768 * 4
  };
  osThreadId_t cloud_handle = osThreadNew(StartCloudTask, NULL, &cloudTask_attributes);
  if (cloud_handle == NULL) {
    log_info("[ERROR] cloudTask create failed!\r\n");
  } else {
    log_info("[BOOT] cloudTask created OK\r\n");
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartLedTask */
/**
  * @brief  Function implementing the ledTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartLedTask */
void StartLedTask(void *argument)
{
  /* USER CODE BEGIN StartLedTask */
  for(;;)
  {
    app_led_task();
  }
  /* USER CODE END StartLedTask */
}

/* USER CODE BEGIN Header_StartLogTask */
/**
  * @brief Function implementing the logTask thread.
  * @param argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartLogTask */
void StartLogTask(void *argument)
{
  /* USER CODE BEGIN StartLogTask */
  for(;;)
  {
    app_log_task();
  }
  /* USER CODE END StartLogTask */
}

/* USER CODE BEGIN Header_StartSenderTask */
/**
  * @brief Function implementing the senderTask thread.
  * @param argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSenderTask */
void StartSenderTask(void *argument)
{
  /* USER CODE BEGIN StartSenderTask */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartSenderTask */
}

/* USER CODE BEGIN Header_StartReceiverTask */
/**
  * @brief Function implementing the receiverTask thread.
  * @param argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartReceiverTask */
void StartReceiverTask(void *argument)
{
  /* USER CODE BEGIN StartReceiverTask */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartReceiverTask */
}

/* USER CODE BEGIN Header_StartSampleTask */
/**
  * @brief  Function implementing the sampleTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSampleTask */
void StartSampleTask(void *argument)
{
  /* USER CODE BEGIN StartSampleTask */
  for(;;)
  {
    app_sample_task();
  }
  /* USER CODE END StartSampleTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void StartModbusTask(void *argument)
{
  for(;;)
  {
    app_modbus_task();
  }
}

void StartCloudTask(void *argument)
{
  log_info("[CLOUD] task started\r\n");
  for(;;)
  {
    app_cloud_task();
  }
}
/* USER CODE END Application */

