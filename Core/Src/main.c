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
#include "leg_ik.h"
#include "walk_gait.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* 控制模式: 上电默认校准模式(不动), 蓝牙命令切换 */
typedef enum { MODE_CALIB = 0, MODE_STAND, MODE_TROT, MODE_STEP } ctrl_mode_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CTRL_PERIOD_MS  5u    /* 控制节拍 5ms = 200Hz */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t bt_rx_byte;           /* 蓝牙接收字节 */
uint8_t imu_rx_byte;          /* IMU 接收字节 */
static ctrl_mode_t g_mode = MODE_CALIB;
static float       g_cmd_deg[12];   /* 当前舵机指令角 (平滑用) */
static uint32_t    g_ctrl_last = 0; /* 控制节拍计时 */
static uint32_t    g_walk_t0   = 0; /* 行走起始时刻 (相位基准) */
static float       g_step_t    = 0; /* 单步调试: 冻结的行走时刻 */
static int         g_step_cnt  = 0; /* 单步计数 0~3 -> PH:0/90/180/270 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

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

  Motor_Init();   /* 电机 PWM 启动, CCR=0 不转, STBY 拉高待命 */

  /* 外展 4 舵机错开 100ms 上电锁定站姿角, 防乱动 */
  for (int leg = 0; leg < 4; leg++) {
      Leg_SetJoint((uint8_t)leg, JOINT_ABD, STAND_POSE[leg][JOINT_ABD]);
      HAL_Delay(100);
  }

  /* 指令角初始化: 外展=锁定值, 大腿/小腿=IK站姿解 (首次G写入即站姿, 不跳变) */
  for (int leg = 0; leg < 4; leg++) {
      float d_leg = (leg == 0 || leg == 2) ? -LEG_HIP_D : LEG_HIP_D;
      uint16_t deg[3];
      LegIK_SolveServo((uint8_t)leg, g_walk_params.foot_x_shift, d_leg, g_walk_params.stand_h, deg);
      g_cmd_deg[leg]     = (float)STAND_POSE[leg][JOINT_ABD];
      g_cmd_deg[leg + 4] = (float)deg[1];   /* 大腿 */
      g_cmd_deg[leg + 8] = (float)deg[2];   /* 小腿 */
  }

  HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1);
  bluetooth_send("READY\r\n");   /* 上电自检: TX/DMA 通不通 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* ====== 5ms 控制节拍: IK解算 + 指数平滑 + 批量输出 ====== */
    if ((uint32_t)(uwTick - g_ctrl_last) >= CTRL_PERIOD_MS) {
        g_ctrl_last = uwTick;

        if (g_mode == MODE_STAND || g_mode == MODE_TROT || g_mode == MODE_STEP) {
            float rate = (g_mode == MODE_STAND) ? 0.12f : 0.20f;
            uint16_t angles[12];

            for (uint8_t leg = 0; leg < 4; leg++) {
                float d_leg = (leg == 0 || leg == 2) ? -LEG_HIP_D : LEG_HIP_D;
                uint16_t deg[3];

                if (g_mode == MODE_STAND) {
                    /* 站立: 足端 = 髋正下方 + 前移修正 (foot_x_shift, ±d, 站高) */
                    LegIK_SolveServo(leg, g_walk_params.foot_x_shift, d_leg, g_walk_params.stand_h, deg);
                } else {
                    /* 行走/单步: 足端轨迹 -> IK */
                    float t = (g_mode == MODE_TROT)
                            ? (float)(uwTick - g_walk_t0) / 1000.0f
                            : g_step_t;
                    walk_vec3_t ft;
                    WalkGait_FootTarget(leg, t, d_leg, &ft);
                    LegIK_SolveServo(leg, ft.x, ft.y, ft.z, deg);
                }

                const uint8_t idx[3] = { leg, leg + 4, leg + 8 };  /* 舵机{i+1,i+5,i+9} */
                for (int j = 0; j < 3; j++) {
                    g_cmd_deg[idx[j]] += ((float)deg[j] - g_cmd_deg[idx[j]]) * rate;
                    angles[idx[j]] = (uint16_t)(g_cmd_deg[idx[j]] + 0.5f);
                }
            }
            NewServo_BatchControl(angles);
        }
        /* MODE_CALIB: 控制循环不碰舵机 */
    }

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

static char     g_line_buf[12];
static uint8_t  g_line_idx = 0;

/* 解析指针处连续数字, 返回数值并推进指针 */
static int s_parse_num(const char **p)
{
    int v = 0;
    while (**p >= '0' && **p <= '9') { v = v * 10 + (int)(**p - '0'); (*p)++; }
    return v;
}

#if 1   /* ==== 舵机校准: 重装舵机臂后重新标定用 (标定完可改回 0) ==== */
static uint8_t  g_calib_servo   = 1;     /* 当前校准舵机号 1~12 */
static uint16_t g_pending_angle = 0;
static uint8_t  g_pending_srv   = 0;
static uint8_t  g_pending       = 0;

static int s_atoi(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (int)(*s++ - '0');
    return v;
}
#endif

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
                g_mode = MODE_STAND;         /* 5ms循环会平滑到站姿 */
                bluetooth_send("STAND...\r\nOK:STAND\r\n");
            }
            else if (f == 'T' || f == 't') {
                g_walk_t0 = uwTick;          /* 相位从零开始 */
                g_mode = MODE_TROT;
                bluetooth_send("TROT\r\n");
            }
            else if (f == 'A' || f == 'a') {
                if (g_mode != MODE_STEP) {   /* 首次按A: 进入单步模式, 相位0 */
                    g_mode = MODE_STEP;
                    g_step_t = 0.0f;
                    g_step_cnt = 0;
                } else {                     /* 之后每按: 相位 +1/4 周期 */
                    g_step_t += g_walk_params.period * 0.25f;
                    g_step_cnt = (g_step_cnt + 1) & 3;
                }
                snprintf(msg, sizeof(msg), "PH:%d\r\n", g_step_cnt * 90);
                bluetooth_send(msg);
            }
            else if (f == 'H' || f == 'h') {
                /* 高站姿 280mm: 抬腿明显 */
                g_walk_params.stand_h = STAND_H_HIGH;
                g_mode = MODE_STAND;
                bluetooth_send("HIGH\r\n");
            }
            else if (f == 'L' || f == 'l') {
                /* 低站姿 240mm: 狗形深蹲 */
                g_walk_params.stand_h = STAND_H_LOW;
                g_mode = MODE_STAND;
                bluetooth_send("LOW\r\n");
            }
            else if (f == 'X' || f == 'x') {
                /* 前倾修正: X:毫米 (如 X:20) 正值=足端前移/身体后坐, 站立行走同时生效 */
                const char *p = g_line_buf + 1;
                int sign = 1;
                if (*p == ':') p++;
                if (*p == '-') { sign = -1; p++; }
                int v = s_parse_num(&p);
                if (v >= 0 && v <= 60) {
                    g_walk_params.foot_x_shift = (float)(sign * v);
                    snprintf(msg, sizeof(msg), "XSH:%d\r\n", sign * v);
                    bluetooth_send(msg);
                }
            }
            else if (f == 'R' || f == 'r') {
                /* 一键滚动: 四电机 30% 占空比正转 (A/B方向已在Motor_Set内纠正) */
                for (int i = 0; i < 4; i++)
                    Motor_Set((uint8_t)i, 30, 1);
                bluetooth_send("ROLL\r\n");
            }
            else if (f == 'S' || f == 's') {
                /* 一键停: 四电机全停 */
                for (int i = 0; i < 4; i++)
                    Motor_Stop((uint8_t)i);
                bluetooth_send("STOP\r\n");
            }
            else if (f == 'M' || f == 'm') {
                /* 电机控制: M电机号:速度[:方向]
                 * 电机号 0~3 = A~D, 速度 0~100, 方向 1=正转 0=反转(缺省1) */
                const char *p = g_line_buf + 1;   /* 跳过 M */
                int m  = s_parse_num(&p);
                if (*p == ':') p++;
                int sp = s_parse_num(&p);
                int dir = 1;
                if (*p == ':') { p++; dir = s_parse_num(&p); }
                if (m >= 0 && m <= 3 && sp >= 0 && sp <= 100) {
                    if (sp == 0)
                        Motor_Stop((uint8_t)m);
                    else
                        Motor_Set((uint8_t)m, (uint8_t)sp, (uint8_t)(dir ? 1 : 0));
                    snprintf(msg, sizeof(msg), "M%c:%d:%d\r\n",
                             'A' + m, sp, dir ? 1 : 0);
                    bluetooth_send(msg);
                }
            }
#if 1   /* ==== 舵机校准命令: 重装舵机臂后重新标定用 (标定完可改回 0) ==== */
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
#endif
            else {
                /* 调试: 回显无法识别的命令 */
                snprintf(msg, sizeof(msg), "UNK:%s\r\n", g_line_buf);
                bluetooth_send(msg);
            }
            g_line_idx = 0;
        }
    } else if ((c >= '0' && c <= '9') || c == ':' || c == '-') {
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
  MPU_InitStruct.BaseAddress = 0x24000000;   /* 必须512KB对齐, 覆盖整个D1 RAM+摄像头缓冲 */
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
