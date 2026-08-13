/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "dcmi.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "camera.h"
#include "motor_control.h"
#include "new_servo.h"
#include "leg_control.h"
#include "encoder.h"
#include "bluetooth_control.h"
#include "imu.h"
#include "gait.h"
#include "ik2d.h"
#include <stdio.h>
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

/* USER CODE BEGIN PV */
uint8_t bt_rx_byte;           /* 蓝牙接收字节 */
uint8_t imu_rx_byte;          /* IMU 接收字节 */
volatile uint8_t g_cmd_stand_flag = 0;
volatile uint8_t g_cmd_step_flag  = 0;
volatile uint8_t g_cmd_trot_flag  = 0;
static uint8_t  g_first_step = 1;        /* 站姿后第一次A不推进 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
static void cmd_stand(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 弱实现：无调试器时防止printf调用空指针引发HardFault；调试器连接时semihosting覆盖此实现
__attribute__((weak)) int __io_putchar(int ch)
{
  (void)ch;
  return ch;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI6_Init();
  MX_DCMI_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM12_Init();
  MX_TIM1_Init();
  MX_TIM16_Init();
  MX_TIM17_Init();
  MX_TIM5_Init();
  MX_USART3_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* ====== 舵机校准：先强制拉低引脚，再开 PWM ====== */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_14, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);
  NewServo_StartAll();

  HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* 延时命令: 主循环执行 HAL_Delay (不能放 ISR 里, SysTick 优先级低会死锁) */
    if (g_cmd_stand_flag) {
        g_cmd_stand_flag = 0;
        bluetooth_send("STAND...\r\n");
        cmd_stand();
        Gait_DebugReset();     /* 单步插值起点重新同步站姿 */
        bluetooth_send("OK:STAND\r\n");
    }
    if (g_cmd_trot_flag) {
        g_cmd_trot_flag = 0;
        if (g_first_step) {            /* 从站姿出发: 先插值到动作1, 再连续跑 */
            g_first_step = 0;
            Gait_SetType(GAIT_TROT);
            Gait_DebugStep(0);         /* 500ms 平滑到 phase0 姿态 */
            Gait_SetType(GAIT_TROT);   /* 解除暂停, 从 phase0 连续走 */
        } else {
            Gait_SetType(GAIT_TROT);   /* 从调试暂停恢复, 原地续跑 */
        }
        bluetooth_send("TROT\r\n");
    }
    if (g_cmd_step_flag) {
        g_cmd_step_flag = 0;
        Gait_SetType(GAIT_TROT);
        if (g_first_step) {          /* 第一次 A: 站姿→动作1(phase 0), 不推进 */
            g_first_step = 0;
            Gait_DebugStep(0);
        } else {
            Gait_DebugStep(90);      /* 之后每次推进 90° */
        }
        { static char m[20]; snprintf(m, sizeof(m), "PH:%d\r\n", Gait_GetPhase(0)->phase);
          bluetooth_send(m); }
    }

    /* 步态更新 */
    Gait_Update();

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

static uint8_t  g_calib_servo   = 1;     /* 当前校准舵机号 1~12 */
static uint16_t g_pending_angle = 0;
static uint8_t  g_pending_srv   = 0;
static uint8_t  g_pending       = 0;
static char     g_line_buf[12];
static uint8_t  g_line_idx = 0;

static int s_atoi(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (int)(*s++ - '0');
    return v;
}

/* G 命令: 8 舵机错开 200ms, IK 算站姿角 (脚在髋正下方贴地) */
static void cmd_stand(void) {
    for (int leg = 0; leg < 4; leg++) {
        uint16_t th, ca;
        IK2D_Solve((uint8_t)leg, 0.0f, HIP_H_MM, &th, &ca);
        Leg_SetJoint((uint8_t)leg, JOINT_THIGH, th);
        Leg_SetJoint((uint8_t)leg, JOINT_CALF,  ca);
        HAL_Delay(200);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3) {
        HAL_UART_Receive_IT(huart, &bt_rx_byte, 1);
        return;
    }
    char c = (char)bt_rx_byte;

    if (c == '\r' || c == '\n') {
        if (g_line_idx > 0) {
            g_line_buf[g_line_idx] = '\0';
            char f = g_line_buf[0];
            static char msg[32];

            if (f == 'G' || f == 'g') {
                g_pending = 0;         /* 清校准残留 */
                g_first_step = 1;      /* 回到站姿, 重置A单步序列 */
                g_cmd_stand_flag = 1;
            }
            else if (f == 'T' || f == 't') {
                g_cmd_trot_flag = 1;
            }
            else if (f == 'A' || f == 'a') {
                g_cmd_step_flag = 1;
            }
            else if (f == 'Y' || f == 'y') {
                if (g_pending && g_pending_srv >= 1 && g_pending_srv <= 12) {
                    NewServo_SetAngle(g_pending_srv, g_pending_angle);
                    g_calib_servo = g_pending_srv;
                    snprintf(msg, sizeof(msg), "OK:S%d:%d\r\n", g_calib_servo, g_pending_angle);
                    bluetooth_send(msg);
                    g_pending = 0;
                }
                g_line_idx = 0; HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1); return;
            }
            else if (f == 'N' || f == 'n') {
                g_pending = 0;
                bluetooth_send("CANCEL\r\n");
            }
            else if (f >= '0' && f <= '9') {
                char *col = NULL;
                for (int i = 0; g_line_buf[i]; i++)
                    if (g_line_buf[i] == ':') { col = &g_line_buf[i]; break; }

                if (col) {
                    *col = '\0';
                    int s = s_atoi(g_line_buf), a = s_atoi(col + 1);
                    if (s >= 1 && s <= 12 && a >= 0 && a <= 270) {
                        g_pending_srv = (uint8_t)s; g_pending_angle = (uint16_t)a; g_pending = 1;
                        snprintf(msg, sizeof(msg), "SET:S%d:%d?\r\n", s, a); bluetooth_send(msg);
                    }
                } else {
                    int v = s_atoi(g_line_buf);
                    if (v >= 1 && v <= 12) {
                        g_calib_servo = (uint8_t)v; g_pending = 0;
                        snprintf(msg, sizeof(msg), "SW:S%d\r\n", v); bluetooth_send(msg);
                    } else if (v >= 0 && v <= 270) {
                        g_pending_srv = g_calib_servo; g_pending_angle = (uint16_t)v; g_pending = 1;
                        snprintf(msg, sizeof(msg), "SET:S%d:%d?\r\n", g_calib_servo, v); bluetooth_send(msg);
                    }
                }
            }
            g_line_idx = 0;
        }
    } else if ((c >= '0' && c <= '9') || c == ':') {
        if (g_line_idx < sizeof(g_line_buf) - 1) g_line_buf[g_line_idx++] = c;
    } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
        if (g_line_idx == 0) g_line_buf[g_line_idx++] = c;
    }
    HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1);
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x24010000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
