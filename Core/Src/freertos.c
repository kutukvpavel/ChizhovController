/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
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
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
uint32_t defaultTaskBuffer[ 1024 ];
osStaticThreadDef_t defaultTaskControlBlock;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .cb_mem = &defaultTaskControlBlock,
  .cb_size = sizeof(defaultTaskControlBlock),
  .stack_mem = &defaultTaskBuffer[0],
  .stack_size = sizeof(defaultTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for adcTask */
osThreadId_t adcTaskHandle;
uint32_t adcTaskBuffer[ 256 ];
osStaticThreadDef_t adcTaskControlBlock;
const osThreadAttr_t adcTask_attributes = {
  .name = "adcTask",
  .cb_mem = &adcTaskControlBlock,
  .cb_size = sizeof(adcTaskControlBlock),
  .stack_mem = &adcTaskBuffer[0],
  .stack_size = sizeof(adcTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for thermocoupleTas */
osThreadId_t thermocoupleTasHandle;
uint32_t thermocoupleTasBuffer[ 256 ];
osStaticThreadDef_t thermocoupleTasControlBlock;
const osThreadAttr_t thermocoupleTas_attributes = {
  .name = "thermocoupleTas",
  .cb_mem = &thermocoupleTasControlBlock,
  .cb_size = sizeof(thermocoupleTasControlBlock),
  .stack_mem = &thermocoupleTasBuffer[0],
  .stack_size = sizeof(thermocoupleTasBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for displayTask */
osThreadId_t displayTaskHandle;
uint32_t displayTaskBuffer[ 1024 ];
osStaticThreadDef_t displayTaskControlBlock;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .cb_mem = &displayTaskControlBlock,
  .cb_size = sizeof(displayTaskControlBlock),
  .stack_mem = &displayTaskBuffer[0],
  .stack_size = sizeof(displayTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for encoderTask */
osThreadId_t encoderTaskHandle;
uint32_t encoderTaskBuffer[ 512 ];
osStaticThreadDef_t encoderTaskControlBlock;
const osThreadAttr_t encoderTask_attributes = {
  .name = "encoderTask",
  .cb_mem = &encoderTaskControlBlock,
  .cb_size = sizeof(encoderTaskControlBlock),
  .stack_mem = &encoderTaskBuffer[0],
  .stack_size = sizeof(encoderTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ioTask */
osThreadId_t ioTaskHandle;
uint32_t ioTaskBuffer[ 256 ];
osStaticThreadDef_t ioTaskControlBlock;
const osThreadAttr_t ioTask_attributes = {
  .name = "ioTask",
  .cb_mem = &ioTaskControlBlock,
  .cb_size = sizeof(ioTaskControlBlock),
  .stack_mem = &ioTaskBuffer[0],
  .stack_size = sizeof(ioTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for debugMenuTask */
osThreadId_t debugMenuTaskHandle;
uint32_t debugMenuTaskBuffer[ 1024 ];
osStaticThreadDef_t debugMenuTaskControlBlock;
const osThreadAttr_t debugMenuTask_attributes = {
  .name = "debugMenuTask",
  .cb_mem = &debugMenuTaskControlBlock,
  .cb_size = sizeof(debugMenuTaskControlBlock),
  .stack_mem = &debugMenuTaskBuffer[0],
  .stack_size = sizeof(debugMenuTaskBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for usbTask */
osThreadId_t usbTaskHandle;
uint32_t usbTaskBuffer[ 256 ];
osStaticThreadDef_t usbTaskControlBlock;
const osThreadAttr_t usbTask_attributes = {
  .name = "usbTask",
  .cb_mem = &usbTaskControlBlock,
  .cb_size = sizeof(usbTaskControlBlock),
  .stack_mem = &usbTaskBuffer[0],
  .stack_size = sizeof(usbTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for rs485_Task */
osThreadId_t rs485_TaskHandle;
uint32_t rs485_TaskBuffer[ 256 ];
osStaticThreadDef_t rs485_TaskControlBlock;
const osThreadAttr_t rs485_Task_attributes = {
  .name = "rs485_Task",
  .cb_mem = &rs485_TaskControlBlock,
  .cb_size = sizeof(rs485_TaskControlBlock),
  .stack_mem = &rs485_TaskBuffer[0],
  .stack_size = sizeof(rs485_TaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ethernetTask */
osThreadId_t ethernetTaskHandle;
uint32_t ethernetTaskBuffer[ 256 ];
osStaticThreadDef_t ethernetTaskControlBlock;
const osThreadAttr_t ethernetTask_attributes = {
  .name = "ethernetTask",
  .cb_mem = &ethernetTaskControlBlock,
  .cb_size = sizeof(ethernetTaskControlBlock),
  .stack_mem = &ethernetTaskBuffer[0],
  .stack_size = sizeof(ethernetTaskBuffer),
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartMainTask(void *argument);
extern void StartAdcTask(void *argument);
extern void StartThermocoupleTask(void *argument);
extern void StartDisplayTask(void *argument);
extern void StartEncoderTask(void *argument);
extern void StartIoTask(void *argument);
extern void StartDebugMenuTask(void *argument);
extern void StartUsbTask(void *argument);
extern void StartRs485Task(void *argument);
extern void StartEthernetTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  osThreadNew(StartMainTask, NULL, &defaultTask_attributes);

  osThreadNew(StartAdcTask, NULL, &adcTask_attributes);
  osThreadNew(StartThermocoupleTask, NULL, &thermocoupleTas_attributes);
  osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);
  osThreadNew(StartEncoderTask, NULL, &encoderTask_attributes);
  osThreadNew(StartIoTask, NULL, &ioTask_attributes);
  osThreadNew(StartDebugMenuTask, NULL, &debugMenuTask_attributes);
  osThreadNew(StartUsbTask, NULL, &usbTask_attributes);
  osThreadNew(StartRs485Task, NULL, &rs485_Task_attributes);
  osThreadNew(StartEthernetTask, NULL, &ethernetTask_attributes);

  /* USER CODE END Init */
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/**
  * @}
  */

/**
  * @}
  */
}

/* USER CODE BEGIN Header_StartMainTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartMainTask */
__weak void StartMainTask(void *argument)
{
  /* USER CODE BEGIN StartMainTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartMainTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

