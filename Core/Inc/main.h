/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_dma.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_SAMPLE_PERIOD_US 10000
#define Onboard_LED_Pin LL_GPIO_PIN_13
#define Onboard_LED_GPIO_Port GPIOC
#define CS_MUX_B_Pin LL_GPIO_PIN_0
#define CS_MUX_B_GPIO_Port GPIOA
#define USART2_DE_Pin LL_GPIO_PIN_1
#define USART2_DE_GPIO_Port GPIOA
#define IN_D_Pin LL_GPIO_PIN_4
#define IN_D_GPIO_Port GPIOA
#define CS_MUX_A_Pin LL_GPIO_PIN_2
#define CS_MUX_A_GPIO_Port GPIOB
#define IO_ST_Pin LL_GPIO_PIN_12
#define IO_ST_GPIO_Port GPIOB
#define OUT_SH_Pin LL_GPIO_PIN_13
#define OUT_SH_GPIO_Port GPIOB
#define IN_SH_Pin LL_GPIO_PIN_14
#define IN_SH_GPIO_Port GPIOB
#define DISP_SH_Pin LL_GPIO_PIN_15
#define DISP_SH_GPIO_Port GPIOB
#define DISP_ST_Pin LL_GPIO_PIN_8
#define DISP_ST_GPIO_Port GPIOA
#define SR_D_Pin LL_GPIO_PIN_15
#define SR_D_GPIO_Port GPIOA
#define CS_MUX_C_Pin LL_GPIO_PIN_4
#define CS_MUX_C_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
