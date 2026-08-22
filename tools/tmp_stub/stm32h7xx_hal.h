/* 语法检查用最小 HAL 桩头 (仅 gcc -fsyntax-only, 不参与编译) */
#ifndef STM32H7XX_HAL_H
#define STM32H7XX_HAL_H
#include <stdint.h>
#define __IO volatile
#define __weak __attribute__((weak))
typedef uint32_t HAL_StatusTypeDef;
typedef uint32_t GPIO_PinState;
typedef enum { HAL_UART_STATE_RESET = 0, HAL_UART_STATE_READY,
               HAL_UART_STATE_BUSY_TX, HAL_UART_STATE_BUSY_RX,
               HAL_UART_STATE_BUSY_RX_TX, HAL_UART_STATE_TIMEOUT,
               HAL_UART_STATE_ERROR } HAL_UART_StateTypeDef;
extern __IO uint32_t uwTick;
typedef struct { void *Instance; HAL_UART_StateTypeDef gState; uint32_t RxState; int dummy; } UART_HandleTypeDef;
typedef struct { uint32_t CR1, ISR, MODER, AFR; } USART_TypeDef;
typedef struct { int OscillatorType; int HSIState; int HSICalibrationValue; int HSEState;
                 struct { int PLLState; int PLLSource; int PLLM; int PLLN; int PLLP; int PLLQ; int PLLR;
                          int PLLRGE; int PLLVCOSEL; int PLLFRACN; } PLL;
                 int APB1CLKDivider; int APB2CLKDivider; int SYSCLKSource; int AHBCLKDivider; } RCC_OscInitTypeDef;
typedef struct { int ClockType; int SYSCLKSource; int AHBCLKDivider; int APB1CLKDivider; int APB2CLKDivider;
                 int APB3CLKDivider; int APB4CLKDivider; int SYSCLKDivider; } RCC_ClkInitTypeDef;
#define PWR_LDO_SUPPLY 0
#define PWR_REGULATOR_VOLTAGE_SCALE0 0
#define PWR_FLAG_VOSRDY 0
#define RCC_OSCILLATORTYPE_HSI 0
#define RCC_OSCILLATORTYPE_HSE 0
#define RCC_OSCILLATORTYPE_PLL 0
#define RCC_HSI_DIV1 0
#define RCC_HSICALIBRATION_DEFAULT 0
#define RCC_PLL_ON 0
#define RCC_PLLSOURCE_HSI 0
#define RCC_HSE_NOT_SUPPLIED 0
#define RCC_SYSCLKSOURCE_PLLCLK 0
#define RCC_CLOCKTYPE_SYSCLK 0
#define RCC_CLOCKTYPE_HCLK 0
#define RCC_CLOCKTYPE_PCLK1 0
#define RCC_CLOCKTYPE_PCLK2 0
#define RCC_HCLK_DIV1 0
#define RCC_HCLK_DIV2 0
#define RCC_APB1_DIV1 0
#define RCC_APB1_DIV2 0
#define RCC_APB2_DIV1 0
#define RCC_APB2_DIV2 0
#define RCC_APB3_DIV2 0
#define RCC_APB4_DIV2 0
#define RCC_SYSCLK_DIV1 0
#define FLASH_LATENCY_4 0
#define RCC_PLL1VCIRANGE_3 0
#define RCC_PLL1VCOWIDE 0
#define RCC_CLOCKTYPE_D3PCLK1 0
#define RCC_CLOCKTYPE_D1PCLK1 0
typedef struct { void *Instance; int dummy; } TIM_HandleTypeDef;
typedef struct { void *Instance; int dummy; } I2C_HandleTypeDef;
typedef struct { void *Instance; int dummy; } SPI_HandleTypeDef;
typedef struct { void *Instance; int dummy; } ADC_HandleTypeDef;
typedef struct { void *Instance; int dummy; } DMA_HandleTypeDef;
typedef struct { void *Instance; int dummy; } DCMI_HandleTypeDef;
typedef struct { int dummy; } UART_InitTypeDef;
typedef struct { int dummy; } I2C_InitTypeDef;
typedef struct { int dummy; } TIM_OC_InitTypeDef;
#define HAL_OK 0
#define HAL_MAX_DELAY 0xFFFFFFFFu
#define __IO volatile
#define __weak __attribute__((weak))
#define DCMI ((void*)0)
typedef struct { int Enable; int Number; int BaseAddress; int Size; int SubRegionDisable;
                 int TypeExtField; int AccessPermission; int DisableExec; int IsShareable;
                 int IsCacheable; int IsBufferable; } MPU_Region_InitTypeDef;
#define MPU_REGION_ENABLE 0
#define MPU_REGION_NUMBER0 0
#define MPU_REGION_NUMBER1 0
#define MPU_REGION_SIZE_512KB 0
#define MPU_TEX_LEVEL0 0
#define MPU_REGION_FULL_ACCESS 0
#define MPU_INSTRUCTION_ACCESS_DISABLE 0
#define MPU_INSTRUCTION_ACCESS_ENABLE 0
#define MPU_ACCESS_SHAREABLE 0
#define MPU_ACCESS_NOT_SHAREABLE 0
#define MPU_ACCESS_CACHEABLE 0
#define MPU_ACCESS_BUFFERABLE 0
#define MPU_ACCESS_NOT_CACHEABLE 0
#define MPU_ACCESS_NOT_BUFFERABLE 0
#define MPU_SUBREGION_DISABLE 0
#define MPU_PRIVILEGED_DEFAULT 0
typedef struct { int dummy; } GPIO_InitTypeDef;
typedef struct { int dummy; } DMA_InitTypeDef;
typedef struct { uint32_t MODER, IDR, AFR[2]; int dummy; } GPIO_TypeDef;
#define TIM_CHANNEL_1 0u
#define TIM_CHANNEL_2 0u
#define TIM_CHANNEL_3 0u
#define TIM_CHANNEL_4 0u
#define SPI6 ((void*)0)
#define GPIO_PIN_0  ((uint32_t)1u<<0)
#define GPIO_PIN_1  ((uint32_t)1u<<1)
#define GPIO_PIN_2  ((uint32_t)1u<<2)
#define GPIO_PIN_3  ((uint32_t)1u<<3)
#define GPIO_PIN_4  ((uint32_t)1u<<4)
#define GPIO_PIN_5  ((uint32_t)1u<<5)
#define GPIO_PIN_6  ((uint32_t)1u<<6)
#define GPIO_PIN_7  ((uint32_t)1u<<7)
#define GPIO_PIN_8  ((uint32_t)1u<<8)
#define GPIO_PIN_9  ((uint32_t)1u<<9)
#define GPIO_PIN_10 ((uint32_t)1u<<10)
#define GPIO_PIN_11 ((uint32_t)1u<<11)
#define GPIO_PIN_12 ((uint32_t)1u<<12)
#define GPIO_PIN_13 ((uint32_t)1u<<13)
#define GPIO_PIN_14 ((uint32_t)1u<<14)
#define GPIO_PIN_15 ((uint32_t)1u<<15)
#define GPIO_PIN_RESET 0u
#define GPIO_PIN_SET 0u
#define GPIOA ((GPIO_TypeDef*)0)
#define GPIOB ((GPIO_TypeDef*)0)
#define GPIOC ((GPIO_TypeDef*)0)
#define GPIOD ((GPIO_TypeDef*)0)
#define GPIOE ((GPIO_TypeDef*)0)
#define GPIOF ((GPIO_TypeDef*)0)
#define GPIOG ((GPIO_TypeDef*)0)
#define GPIOH ((GPIO_TypeDef*)0)
#define TIM1 ((void*)0)
#define TIM2 ((void*)0)
#define TIM3 ((void*)0)
#define TIM4 ((void*)0)
#define TIM5 ((void*)0)
#define TIM6 ((void*)0)
#define TIM8 ((void*)0)
#define TIM12 ((void*)0)
#define USART2 ((USART_TypeDef*)0)
#define USART3 ((USART_TypeDef*)0)
#define UART3 ((void*)0)
#define UART4 ((void*)0)
#define SPI1 ((void*)0)
#define I2C1 ((void*)0)
#define ADC1 ((void*)0)
#define DMA1_Stream1_IRQn 0
#define DMA2_Stream7_IRQn 1
#endif
