/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

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
#define BIN1_Pin GPIO_PIN_2
#define BIN1_GPIO_Port GPIOE
#define BIN2_Pin GPIO_PIN_3
#define BIN2_GPIO_Port GPIOE
#define PWDN_Pin GPIO_PIN_13
#define PWDN_GPIO_Port GPIOF
#define SCCB_SCL_Pin GPIO_PIN_14
#define SCCB_SCL_GPIO_Port GPIOF
#define SCCB_SDA_Pin GPIO_PIN_15
#define SCCB_SDA_GPIO_Port GPIOF
#define ENC1_A_Pin GPIO_PIN_7
#define ENC1_A_GPIO_Port GPIOE
#define ENC1_A_EXTI_IRQn EXTI9_5_IRQn
#define ENC1_B_Pin GPIO_PIN_8
#define ENC1_B_GPIO_Port GPIOE
#define ENC2_A_Pin GPIO_PIN_9
#define ENC2_A_GPIO_Port GPIOE
#define ENC2_A_EXTI_IRQn EXTI9_5_IRQn
#define ENC2_B_Pin GPIO_PIN_10
#define ENC2_B_GPIO_Port GPIOE
#define ENC3_A_Pin GPIO_PIN_11
#define ENC3_A_GPIO_Port GPIOE
#define ENC3_B_Pin GPIO_PIN_12
#define ENC3_B_GPIO_Port GPIOE
#define ENC4_A_Pin GPIO_PIN_13
#define ENC4_A_GPIO_Port GPIOE
#define ENC4_B_Pin GPIO_PIN_14
#define ENC4_B_GPIO_Port GPIOE
#define DIN2_Pin GPIO_PIN_12
#define DIN2_GPIO_Port GPIOB
#define STBY_Pin GPIO_PIN_10
#define STBY_GPIO_Port GPIOA
#define LCD_BL_Pin GPIO_PIN_12
#define LCD_BL_GPIO_Port GPIOG
#define LCD_DC_Pin GPIO_PIN_15
#define LCD_DC_GPIO_Port GPIOG
#define CIN1_Pin GPIO_PIN_5
#define CIN1_GPIO_Port GPIOB
#define CIN2_Pin GPIO_PIN_6
#define CIN2_GPIO_Port GPIOB
#define DIN1_Pin GPIO_PIN_7
#define DIN1_GPIO_Port GPIOB
#define AIN1_Pin GPIO_PIN_0
#define AIN1_GPIO_Port GPIOE
#define AIN2_Pin GPIO_PIN_1
#define AIN2_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
