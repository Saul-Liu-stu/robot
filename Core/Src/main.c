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
#include "iwdg.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motor_control.h"
#include "new_servo.h"
#include "leg_control.h"
#include "encoder.h"
#include "bluetooth_control.h"
#include "imu.h"
#include "leg_ik.h"
#include "walk_gait.h"
#include "attitude_control.h"
#include "drive_ctrl.h"
#include "gimbal.h"
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* 控制模式: 上电默认校准模式(不动), 蓝牙命令切换 */
typedef enum { MODE_CALIB = 0, MODE_STAND, MODE_TROT, MODE_STEP } ctrl_mode_t;

#define CROUCH_X_F   30.0f    /* 正常分支低趴: 前腿足端前伸 (膝轴离地86mm) */
#define CROUCH_X_FR 114.5f    /* 反折分支低趴: 前足+129.5, 大腿≈水平(q2=85)+小腿垂直(q3=-85) */
#define CROUCH_X_R  144.5f    /* 正常分支低趴后伸: 后足15-144.5=-129.5, 与前足对称 (L 视觉姿态) */
#define CROUCH_X_RR 135.0f    /* 反折分支低趴后伸 (支撑中心在身下, 前后对称) */

#define FLIP_H         240.0f /* 翻膝机动高度: 此高度下前膝全程≥110mm */
#define FLIP_SEG_MS    900u   /* 翻膝单段时长 (伸/收各900ms) */
#define FLIP_SETTLE_MS 800u   /* 到位后等待平滑收敛再翻膝 */

#define PARK_H          72.0f /* 收纳姿态站高: 贴地趴 (肚皮离地~32mm, 断电无砸落) */
#define PARK_X_F       282.0f /* 贴地趴前足前伸量 (15+282=297, 腿伸直) */
#define PARK_X_R       312.0f /* 贴地趴后足后伸量 (15-312=-297) */

#define RISE_CM_X       15.0f /* 站起阶段重心后移量: 足端前移=身体后坐, 卸载前腿 */
#define RISE_CM_Y       10.0f /* 站起阶段重心左移量: 卸载右腿(FR堵转), 载荷给强壮的RL */

#define SIT_Z_F        310.0f /* 坐姿前腿足端z: 腿竖直伸直 (q2≈q3≈0, 直腿点) */
#define SIT_Z_R        240.0f /* 坐姿后腿足端z: 反折深蹲 */
#define SIT_X_R         95.0f /* 坐姿后足后伸量 (15-95=-80, 膝后折72°) */

/* 前后腿膝分支: 0=正常(狗姿态) 1=反折(膝盖往前顶) */
static uint8_t  g_front_rev = 0;
static uint8_t  g_rear_rev  = 0;
static uint8_t  g_park      = 0;   /* 收纳姿态 (C 命令): 72mm 贴地趴 */
static uint8_t  g_sit       = 0;   /* 坐姿 (O 命令): 前腿竖直伸直 + 后腿反折深蹲 */
static uint8_t  g_rise      = 0;   /* 规划式站起 (U 命令): 1=阶段A收腿+顶升 */
static uint32_t g_rise_t0   = 0;
static float    g_rise_h0   = 0;   /* 站起阶段A起始高度 */
static uint8_t  g_booted    = 1;   /* 刚上电标志: U 站起在上电后可用 (假定在趴姿) */

/* 控制模式与行走时刻 (PTD 函数也要用, 声明放这里) */
static ctrl_mode_t g_mode    = MODE_CALIB;
static uint32_t    g_walk_t0 = 0; /* 行走起始时刻 (相位基准) */

/* 横移走状态: F 命令开启 (0=前后trot, 1=横向螃蟹步), g_lat_dir 为方向 */
static uint8_t  g_gait_lat = 0;
static int      g_lat_dir  = 1;

/* 自稳标定: Z 命令采样中标志 (完成时发 CAL:OK) */
static uint8_t  g_calib_pending = 0;

/* 站立自稳: L:1/L:0 开关, 10Hz 更新四腿z补偿 (仅H/K高站姿, Z标定后可用) */
static float    g_lv_dz[4] = {0, 0, 0, 0};
static uint32_t g_lv_last  = 0;

/* IMU 上报: I 命令开关, 10Hz 固定频率 */
static uint8_t  g_imu_stream = 0;
static uint32_t g_imu_last   = 0;
static uint8_t  g_imu_diag   = 0;  /* I 按下后主循环发一次诊断 */

/* 编码器转速上报: J 命令开关, 10Hz */
static uint8_t  g_enc_stream = 0;
static uint32_t g_enc_last   = 0;

/* 翻膝机动状态 (姿态切换专用: 指定腿组的足端伸到直腿点再收回, 中点翻转膝分支) */
static uint8_t  g_flip_on    = 0;   /* 翻膝进行中 */
static uint32_t g_flip_t0    = 0;   /* 翻膝起始时刻 */
static uint32_t g_flip_start = 0;   /* 翻膝定时启动 (0=无计划) */
static uint8_t  g_flip_mask  = 0;   /* 翻膝腿组: bit0=前腿 bit1=后腿 */
static float    g_flip_x0f   = 0;   /* 前腿起点/终点x */
static float    g_flip_x0r   = 0;   /* 后腿起点/终点x */
static float    g_flip_xm    = 0;   /* 直腿点x幅度 (两分支在此连续) */
static uint8_t  g_flip_revf  = 0;   /* 翻膝后前腿分支 */
static uint8_t  g_flip_revr  = 0;   /* 翻膝后后腿分支 */
static uint8_t  g_flip_post  = 0;   /* 翻膝完成后: 1=趴LOW 2=升HIGH 3=开W 4=开F 5=趴PARK 7=坐姿 8=降到指定高度 */
static float    g_flip_post_h = 0;   /* post=8 的目标站高 (H:高度命令) */

/* 站起阶段重心挪移系数: h<100 无几何空间(腿近伸直), 100~160 渐变, ≥160 全量 */
static float RiseCmK(void)
{
    float k = (g_walk_params.stand_h - 100.0f) / 60.0f;
    if (k < 0.0f) return 0.0f;
    if (k > 1.0f) return 1.0f;
    return k;
}

/* 站姿足端 x: 前移修正 + 低趴外伸 + 坡度修正 (站高越低, 前后腿越往外伸) */
static float StandFootX(uint8_t leg)
{
    if (g_park) {
        /* 贴地趴: 前腿反折前伸直 x+297 / 后腿正常后伸直 x-297 (膝高~67mm) */
        if (leg < 2)
            return g_walk_params.foot_x_shift + PARK_X_F;
        return g_walk_params.foot_x_shift - PARK_X_R;
    }
    if (g_rise == 1) {
        /* 站起阶段A: 先顶升后收腿 (低力矩路径)
         * 反折前腿/正常后腿在低站高时只能近伸直前伸/后伸 (大腿 q2=±90 限制):
         * 足端收拢量随站高上升逐步放开, 沿 大腿(q2) + 小腿(q3) 双边界内收。
         * 2026-08-21 修复: 原轨迹只限小腿, 低处要求 q2>90 → IK钳位 →
         * 前腿被指令压进地面 (FR 堵转)。 */
        float h = g_walk_params.stand_h;
        /* 大腿 q2=±90 边界: 低处足端必须保持外伸, h≥180 后放开 (前后腿对称)
         * +2mm 安全余量: 目标恰好压在边界上时 IK 数值会微超 ±90 被钳 */
        float xq2 = (h < 180.0f) ? (130.0f + sqrtf(32400.0f - h * h) + 2.0f) : 0.0f;
        float bf = sqrtf(53361.0f - h * h);   /* FL小腿边界 (q3=-85) */
        if (bf < 0.0f) bf = 0.0f;
        float xf = 15.0f + CROUCH_X_FR;       /* =129.5, 前顶低趴前足 (h≤191恒k=1) */
        if (bf > xf)  xf = bf;
        if (xq2 > xf) xf = xq2;
        float br = sqrtf(43594.0f - h * h);   /* RL小腿边界 (q3=+97) */
        if (br < 0.0f) br = 0.0f;
        float xr = CROUCH_X_R - 15.0f;        /* =129.5, 前顶低趴后足幅度 (终点对齐L) */
        if (br > xr)  xr = br;
        if (xq2 > xr) xr = xq2;
        /* 重心后移: 足端整体前移 = 身体后坐, 卸载前腿 (FR 堵转补偿) */
        float cmx = RISE_CM_X * RiseCmK();
        if (leg < 2)
            return g_walk_params.foot_x_shift + xf + cmx;
        return g_walk_params.foot_x_shift - xr + cmx;
    }
    if (g_sit) {
        /* 坐姿: 前腿竖直伸直(x=0, z=310) + 后腿反折深蹲(x=-80, z=240) */
        if (leg < 2)
            return g_walk_params.foot_x_shift - 15.0f;
        return g_walk_params.foot_x_shift - SIT_X_R;
    }
    float k = (STAND_H_HIGH - g_walk_params.stand_h) / (STAND_H_HIGH - STAND_H_LOW);
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    if (leg < 2)
        return g_walk_params.foot_x_shift +
               (g_front_rev ? CROUCH_X_FR * k : CROUCH_X_F * k);
    return g_walk_params.foot_x_shift -
           (g_rear_rev ? CROUCH_X_RR * k : CROUCH_X_R * k);
}

/* 直腿点x: 足端在高度h下伸到腿完全伸直的位置 (两分支在此连续) */
static float FlipStraightX(float h)
{
    float dmax = (LEG_L1 + LEG_L2) * 0.9999f;
    float d2 = dmax * dmax - h * h;
    return (d2 > 0.0f) ? sqrtf(d2) : 0.0f;
}

/* 开始翻膝机动 (前腿往+伸, 后腿往-伸) */
static void FlipBegin(void)
{
    g_flip_x0f = StandFootX(0) + g_walk_params.foot_x_corr[0];
    g_flip_x0r = StandFootX(2) + g_walk_params.foot_x_corr[2];
    g_flip_xm  = FlipStraightX(g_walk_params.stand_h);
    g_flip_t0  = uwTick;
    g_flip_on  = 1;
    bluetooth_send("FLIP\r\n");
}

/* 启动 trot: 恢复行走参数 + 停电机 + 相位归零, 可选重心横移模式
 * wheel_pct: 轮助走占空比 (T 前进行走=10%, E 转圈=0); back: 1=倒着走(负步幅+轮反向) */
static void TrotStart(uint8_t shift_mode, uint8_t wheel_pct, uint8_t back, const char *reply)
{
    g_flip_on = 0; g_flip_start = 0;
    g_walk_params.step_len = back ? -60.0f : 60.0f;   /* 负步幅=倒走 (支撑相反向推) */
    g_walk_params.step_h   = 25.0f;
    DriveCtrl_Reset();           /* 接管电机前先复位摇杆驱动 */
    g_booted = 0;
    g_sit = 0;
    g_shift_mode = shift_mode;
    g_gait_lat = 0;              /* 前后 trot */
    g_walk_t0 = uwTick;          /* 相位从零开始 */
    g_mode = MODE_TROT;
    for (int i = 0; i < 4; i++) {
        if (wheel_pct > 0)
            Motor_Set((uint8_t)i, wheel_pct, back ? 0 : 1);
        else
            Motor_Stop((uint8_t)i);
    }
    bluetooth_send(reply);
}
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
static float       g_cmd_deg[12];   /* 当前舵机指令角 (平滑用) */
static uint32_t    g_ctrl_last = 0; /* 控制节拍计时 */
static float       g_step_t    = 0; /* 单步调试: 冻结的行走时刻 */
static int         g_step_cnt  = 0; /* 单步计数 0~3 -> PH:0/90/180/270 */
static uint8_t     g_rise_slow    = 0;             /* 站高上升标记: 低趴升站用慢速平滑 */
static float       g_stand_h_prev = STAND_H_HIGH; /* 上一目标站高 (判升/降方向) */
static ctrl_mode_t g_mode_prev    = MODE_CALIB;   /* 上一拍模式 (退出行走时停轮用) */
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
  MX_UART4_Init();
  MX_TIM13_Init();
  MX_TIM14_Init();
  MX_IWDG1_Init();
  /* USER CODE BEGIN 2 */

  /* ====== 舵机校准：先强制拉低引脚，再开 PWM ====== */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_14, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);
  NewServo_StartAll();

  Motor_Init();   /* 电机 PWM 启动, CCR=0 不转, STBY 拉高待命 */

  Gimbal_Init();  /* 云台舵机 (TIM8 CH3/CH4), 默认居中 135/135 */

  /* 外展 4 舵机错开 100ms 上电锁定站姿角, 防乱动 */
  for (int leg = 0; leg < 4; leg++) {
      Leg_SetJoint((uint8_t)leg, JOINT_ABD, STAND_POSE[leg][JOINT_ABD]);
      HAL_Delay(100);
  }

  /* 指令角初始化: 外展=锁定值, 大腿/小腿=IK站姿解 (首次G写入即站姿, 不跳变) */
  LegIK_SetFrontReversed(0);   /* 上电默认狗姿态 (前后腿正常膝分支) */
  LegIK_SetRearReversed(0);
  for (int leg = 0; leg < 4; leg++) {
      float d_leg = (leg == 0 || leg == 2) ? -LEG_HIP_D : LEG_HIP_D;
      uint16_t deg[3];
      LegIK_SolveServo((uint8_t)leg, StandFootX((uint8_t)leg), d_leg, g_walk_params.stand_h, deg);
      g_cmd_deg[leg]     = (float)STAND_POSE[leg][JOINT_ABD];
      g_cmd_deg[leg + 4] = (float)deg[1];   /* 大腿 */
      g_cmd_deg[leg + 8] = (float)deg[2];   /* 小腿 */
  }

  HAL_UART_Receive_IT(&huart2, &imu_rx_byte, 1);   /* IMU 数据流 (PD6, 115200bps) */
  HAL_UART_Receive_IT(&huart3, &bt_rx_byte, 1);
  Encoder_Init();   /* 编码器计数清零 */
  bluetooth_send("READY\r\n");   /* 上电自检: TX/DMA 通不通 */

  /* 环回自测: PD5(TX) 发 5 字节, 短接 PD5-PD6 后 raw 应 >=5 */
  {
      static const uint8_t loop_test[] = {0x55, 0x51, 0x00, 0x00, 0xA6};
      HAL_UART_Transmit(&huart2, (uint8_t *)loop_test, 5, 100);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    HAL_IWDG_Refresh(&hiwdg1);   /* 喂狗: 主循环死掉/Error_Handler 超2秒 → 自动复位, 蓝牙失效免断电 */

    /* ====== 编码器转速上报: J 命令开启后 10Hz, 帧尾 520 ====== */
    if (g_enc_stream && (uint32_t)(uwTick - g_enc_last) >= 100u) {
        g_enc_last = uwTick;
        static char ebuf[40];
        snprintf(ebuf, sizeof(ebuf), "E,%d,%d,%d,%d,520\r\n",
                 (int)DriveCtrl_GetRPM(0), (int)DriveCtrl_GetRPM(1),
                 (int)DriveCtrl_GetRPM(2), (int)DriveCtrl_GetRPM(3));
        bluetooth_send(ebuf);
    }

    /* ====== IMU 姿态上报: I 命令开启后 10Hz, 帧尾 520 (参考 adc_10 风格) ====== */
    if (g_imu_stream && (uint32_t)(uwTick - g_imu_last) >= 100u) {
        g_imu_last = uwTick;
        const IMU_Angle *a = IMU_GetAngle();
        /* 定点×10 (0.1°分辨率), 全整数传输, 避免浮点打印 */
        int16_t rx = (int16_t)(a->roll  * 10.0f + (a->roll  >= 0 ? 0.5f : -0.5f));
        int16_t px = (int16_t)(a->pitch * 10.0f + (a->pitch >= 0 ? 0.5f : -0.5f));
        int16_t yx = (int16_t)(a->yaw   * 10.0f + (a->yaw   >= 0 ? 0.5f : -0.5f));
        static char ibuf[40];   /* static: DMA 发送期间缓冲区必须有效 */
        snprintf(ibuf, sizeof(ibuf), "R,%d,P,%d,Y,%d,520\r\n", (int)rx, (int)px, (int)yx);
        bluetooth_send(ibuf);
    }

    /* ====== USART2 接收链看门狗: 每1s检查, 断链自动重装 (双保险) ====== */
    {
        static uint32_t rx_wd_last = 0;
        if ((uint32_t)(uwTick - rx_wd_last) >= 1000u) {
            rx_wd_last = uwTick;
            if (huart2.RxState != HAL_UART_STATE_BUSY_RX) {
                huart2.RxState = HAL_UART_STATE_READY;
                HAL_UART_Receive_IT(&huart2, &imu_rx_byte, 1);
            }
        }
    }

    /* ====== IMU 诊断: 按 I 后报告 USART2 寄存器与收发计数 (主循环发, 避开ISR DMA死锁) ====== */
    if (g_imu_diag) {
        g_imu_diag = 0;
        static char dbg[128];
        snprintf(dbg, sizeof(dbg),
            "DBG2:CR1=%08lX ISR=%08lX MODER=%08lX AFR=%08lX gSt=%d RxSt=%d raw=%lu frames=%lu\r\n",
            (unsigned long)USART2->CR1,
            (unsigned long)USART2->ISR,
            (unsigned long)GPIOD->MODER,
            (unsigned long)GPIOD->AFR[0],
            (int)huart2.gState, (int)huart2.RxState,
            (unsigned long)IMU_GetRawCount(),
            (unsigned long)IMU_GetFrameCount());
        bluetooth_send(dbg);
    }

    /* ====== IMU 诊断: I 开启但 5s 无有效帧 → 报告收发计数 ====== */
    if (g_imu_stream && IMU_Timeout(5000)) {
        static uint32_t diag_last = 0;
        if ((uint32_t)(uwTick - diag_last) >= 2000u) {  /* 每2s一条, 不刷屏 */
            diag_last = uwTick;
            static char dbuf[64];
            snprintf(dbuf, sizeof(dbuf),
                     "IMU:NO DATA raw=%lu frames=%lu\r\n",
                     (unsigned long)IMU_GetRawCount(),
                     (unsigned long)IMU_GetFrameCount());
            bluetooth_send(dbuf);
        }
    }

    /* ====== 5ms 控制节拍: IK解算 + 指数平滑 + 批量输出 ====== */
    if ((uint32_t)(uwTick - g_ctrl_last) >= CTRL_PERIOD_MS) {
        g_ctrl_last = uwTick;

        /* 翻膝定时启动 (命令设的延迟到点后开始) */
        if (g_flip_start && (int32_t)(uwTick - g_flip_start) >= 0) {
            g_flip_start = 0;
            FlipBegin();
        }

        /* 摇杆驱动: 占空比斜坡 + 电机差速 + 倾斜状态机 (每5ms) */
        DriveCtrl_Update();

        /* 退出行走模式→站立时停轮 (T 轮助走10% 随行走结束停止; 急停用 S) */
        if (g_mode == MODE_STAND && g_mode_prev == MODE_TROT) {
            for (int i = 0; i < 4; i++)
                Motor_Stop((uint8_t)i);
        }
        g_mode_prev = g_mode;

        /* 规划式站起: 单段顶升+收腿 → 前顶低趴 191 (3.5s 缓升)
         * 2026-08-21 改: 趴姿(前反折+后正常)与前顶低趴同膝分支,
         * 免翻膝、免回高站姿, 直接站到 L 视觉姿态 (之后 G/K 可再升) */
        if (g_rise == 1) {
            float t = (float)(uwTick - g_rise_t0) / 1000.0f;
            float h = g_rise_h0 + (STAND_H_L_FRONT - g_rise_h0) * (t / 3.5f);
            if (h > STAND_H_L_FRONT) h = STAND_H_L_FRONT;
            g_walk_params.stand_h = h;
            if (t >= 3.5f) {
                g_rise = 0;
                bluetooth_send("LOW\r\n");   /* 站起完成: 前顶低趴 191 */
            }
        }

        /* IMU 行为层: 姿态ID跟随当前配置, Z标定每拍采样, 自稳10Hz更新 */
        {
            const IMU_Angle *ia = IMU_GetAngle();
            float pitch = ia->pitch;
            AttCtrl_SetPose(g_front_rev, g_rear_rev,
                            g_walk_params.stand_h >= STAND_H_HIGH);
            AttCtrl_CalibTick(pitch, ia->roll);
            if (g_calib_pending && !AttCtrl_CalibBusy()) {
                g_calib_pending = 0;
                bluetooth_send("CAL:OK\r\n");  /* 标定完成: 自稳零偏已存 */
            }
            /* 站立自稳: 10Hz 更新四腿z补偿 (站立静止 或 V模式1无倾斜驱动 时喂活, 否则清零) */
            if ((uint32_t)(uwTick - g_lv_last) >= 100u) {
                g_lv_last = uwTick;
                uint8_t lv_act = (g_mode == MODE_STAND) && !g_park &&
                                 !g_rise && !g_flip_on && !g_sit &&
                                 (AttCtrl_LevelEnabled() || DriveCtrl_NoTilt());
                AttCtrl_LevelUpdate(pitch, ia->roll, g_walk_params.stand_h, lv_act, g_lv_dz);
            }
        }

        if (g_mode == MODE_STAND || g_mode == MODE_TROT || g_mode == MODE_STEP) {
            /* 目标站高变化: 上升(低趴站起, 大轮负载重)标记慢速, 下降保持原手感 */
            if (g_walk_params.stand_h != g_stand_h_prev) {
                g_rise_slow = (g_walk_params.stand_h > g_stand_h_prev) ? 1 : 0;
                g_stand_h_prev = g_walk_params.stand_h;
            }
            /* 平滑速率: 收纳/站起最慢; 低趴升站慢速(0.04≈1.5s); 翻膝用原速(时序对齐) */
            float rate = (g_park || g_rise) ? 0.008f
                       : ((g_mode == MODE_STAND)
                          ? ((g_rise_slow && !g_flip_on) ? 0.04f : 0.12f)
                          : 0.20f);
            uint16_t angles[12];

            for (uint8_t leg = 0; leg < 4; leg++) {
                float d_leg = (leg == 0 || leg == 2) ? -LEG_HIP_D : LEG_HIP_D;
                uint16_t deg[3];

                if (g_mode == MODE_STAND) {
                    float x_leg;
                    uint8_t leg_bit = (leg < 2) ? 1 : 2;
                    if (g_flip_on && (g_flip_mask & leg_bit)) {
                        /* 翻膝机动: 该腿组足端伸到直腿点再收回, 中点翻转膝分支 */
                        float x0  = (leg < 2) ? g_flip_x0f : g_flip_x0r;
                        float sgn = (leg < 2) ? 1.0f : -1.0f;   /* 前腿往+伸, 后腿往-伸 */
                        uint8_t rev_new = (leg < 2) ? g_flip_revf : g_flip_revr;
                        uint8_t rev_old = (leg < 2) ? g_front_rev : g_rear_rev;
                        float seg = (float)FLIP_SEG_MS / 1000.0f;
                        float t   = (float)(uwTick - g_flip_t0) / 1000.0f;
                        if (t >= 2.0f * seg) {
                            /* 完成 (由第一条翻膝腿触发一次) */
                            g_flip_on = 0;
                            if (g_flip_mask & 1) {
                                g_front_rev = g_flip_revf;
                                LegIK_SetFrontReversed(g_flip_revf);
                            }
                            if (g_flip_mask & 2) {
                                g_rear_rev = g_flip_revr;
                                LegIK_SetRearReversed(g_flip_revr);
                            }
                            if (g_flip_post == 1)      g_walk_params.stand_h = STAND_H_L_FRONT;
                            else if (g_flip_post == 2) g_walk_params.stand_h = STAND_H_HIGH;
                            else if (g_flip_post == 3) {
                                for (int i = 0; i < 4; i++)
                                    Motor_Set((uint8_t)i, 30, 1);
                                g_walk_t0 = uwTick;
                                g_mode = MODE_TROT;
                            }
                            else if (g_flip_post == 4) {
                                g_walk_t0 = uwTick;      /* 翻膝完成后启动横移走 */
                                g_mode = MODE_TROT;
                            }
                            else if (g_flip_post == 5) {
                                g_walk_params.stand_h = PARK_H;  /* 翻完慢速趴到贴地 */
                                g_park = 1;
                            }
                            else if (g_flip_post == 7) {
                                g_sit = 1;   /* 翻完进入坐姿 */
                            }
                            else if (g_flip_post == 8) {
                                g_walk_params.stand_h = g_flip_post_h;  /* 翻完降到 H: 指定高度 */
                            }
                            x_leg = StandFootX(leg);
                        } else if (t < seg) {
                            float u = t / seg;          /* 第一段: 旧分支伸腿 */
                            if (leg < 2) LegIK_SetFrontReversed(rev_old);
                            else         LegIK_SetRearReversed(rev_old);
                            x_leg = x0 + (sgn * g_flip_xm - x0) * u;
                        } else {
                            float u = (t - seg) / seg;  /* 第二段: 新分支收腿 */
                            if (leg < 2) LegIK_SetFrontReversed(rev_new);
                            else         LegIK_SetRearReversed(rev_new);
                            x_leg = sgn * g_flip_xm + (x0 - sgn * g_flip_xm) * u;
                        }
                    } else {
                        /* 站立: 足端 = 前移修正 + 低趴外伸 + 单腿零位修正 (StandFootX, ±d, 站高) */
                        x_leg = StandFootX(leg) + g_walk_params.foot_x_corr[leg];
                    }
                    /* 倾斜平衡: 足端 z 差动
                     * pitch>0=低头: 前腿z小(前低) 后腿z大(后高)
                     * roll>0=右倾: 右腿z小(右低) 左腿z大(左高) ← 向弯内压 */
                    {
                        float tp, tr;
                        DriveCtrl_GetTilt(&tp, &tr);
                        float z_leg = g_walk_params.stand_h;
                        if (g_sit) {
                            z_leg = (leg < 2) ? SIT_Z_F : SIT_Z_R;  /* 坐姿固定z (不叠加倾斜) */
                        } else {
                            if (leg < 2)          z_leg -= tp;   /* 前腿 */
                            else                  z_leg += tp;   /* 后腿 */
                            if (leg == 0 || leg == 2) z_leg += tr;  /* 左腿: 右倾时左高 */
                            else                      z_leg -= tr;  /* 右腿: 右倾时右低 */
                        }
                        /* 站立自稳: 四腿z补偿 (过渡/翻膝/收纳中不叠加, 防IK钳位) */
                        if (!g_park && !g_rise && !g_flip_on && !g_sit)
                            z_leg += g_lv_dz[leg];
                        /* 站起阶段: 足端右移 = 身体左移, 卸载右腿 (载荷给 RL) */
                        float y_leg = d_leg + ((g_rise) ? RISE_CM_Y * RiseCmK() : 0.0f);
                        LegIK_SolveServo(leg, x_leg, y_leg, z_leg, deg);
                    }
                } else {
                    /* 行走/单步: 足端轨迹 -> IK */
                    float t = (g_mode == MODE_TROT)
                            ? (float)(uwTick - g_walk_t0) / 1000.0f
                            : g_step_t;
                    walk_vec3_t ft;
                    if (g_gait_lat)   /* 横移走: 扫步在 y 方向 (螃蟹步, 抬腿+横扫) */
                        WalkGait_FootTargetLat(leg, t, d_leg, g_lat_dir, &ft);
                    else
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
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

/* UART 错误自愈: 上电时序噪声(模块复位输出低电平)会产生 FE/ORE,
 * HAL 错误路径会停掉接收链, 必须在这里清错误+重装接收 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        huart->RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(huart, &imu_rx_byte, 1);
    }
    else if (huart->Instance == USART3) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        huart->RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(huart, &bt_rx_byte, 1);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        /* IMU 字节流: 喂解析器 (0x55 帧协议) */
        IMU_ParseByte(imu_rx_byte);
        HAL_UART_Receive_IT(&huart2, &imu_rx_byte, 1);
        return;
    }
    char c = (char)bt_rx_byte;

    if (c == '\r' || c == '\n') {
        if (g_line_idx > 0) {
            g_line_buf[g_line_idx] = '\0';
            char f = g_line_buf[0];
            static char msg[32];

            if (f == 'G' || f == 'g') {
                if (g_line_buf[1] == ':') {
                    /* G:水平:俯仰 → 云台舵机 (0~270°, 俯仰缺省135; 与 G 站姿区分: 带冒号) */
                    const char *p = g_line_buf + 2;
                    int pan = s_parse_num(&p);
                    int tilt = 135;
                    if (*p == ':') { p++; tilt = s_parse_num(&p); }
                    Gimbal_Set(GIMBAL_PAN,  (uint16_t)pan);
                    Gimbal_Set(GIMBAL_TILT, (uint16_t)tilt);
                    snprintf(msg, sizeof(msg), "GM:%d,%d\r\n",
                             (int)Gimbal_Get(GIMBAL_PAN), (int)Gimbal_Get(GIMBAL_TILT));
                    bluetooth_send(msg);
                } else {
                    /* 站立 = 狗高站姿 280 (前后正常膝, 若反折先翻膝再升), 收纳姿态直接站起 */
                    g_flip_on = 0; g_flip_start = 0;
                    g_park = 0; g_sit = 0;
                    g_booted = 0;
                    g_flip_mask = 0;
                    if (g_front_rev) g_flip_mask |= 1;
                    if (g_rear_rev)  g_flip_mask |= 2;
                    g_flip_revf = 0; g_flip_revr = 0;
                    if (g_flip_mask) {
                        if (g_walk_params.stand_h < FLIP_H)
                            g_walk_params.stand_h = FLIP_H;   /* 先到翻膝高度 */
                        g_flip_post = 2;                      /* 翻完升 280 */
                        g_flip_start = uwTick + FLIP_SETTLE_MS;
                    } else {
                        g_walk_params.stand_h = STAND_H_HIGH;
                    }
                    g_mode = MODE_STAND;
                    bluetooth_send("STAND...\r\nOK:STAND\r\n");
                }
            }
            else if (f == 'T' || f == 't') {
                /* 连续 Trot: T 前进 / T:-1 倒着走 (负步幅+轮反向助走)
                 * 注意: s_parse_num 不处理负号, 方向直接看符号字符 */
                int back = (g_line_buf[1] == ':' && g_line_buf[2] == '-') ? 1 : 0;
                TrotStart(SHIFT_OFF, 10, (uint8_t)back,
                          back ? "TROTB\r\n" : "TROT\r\n");
            }
            else if (f == 'E' || f == 'e') {
                /* 转圈动作: trot + 重心横移 (定死15mm); E/E:1 顺时针, E:0 逆时针 */
                const char *p = g_line_buf + 1;
                int sign = 1;
                if (*p == ':') {
                    p++;
                    if (*p == '0') sign = -1;
                }
                g_shift_sign = sign;
                snprintf(msg, sizeof(msg), "SWAY:%d\r\n", sign);
                TrotStart(SHIFT_IK, 0, 0, msg);   /* 原地转圈: 不助走 */
            }
            else if (f == 'F' || f == 'f') {
                /* 横移走 (螃蟹步): 四腿外展横向扫步, F 或 F:1 向右, F:0 向左
                 * 强制狗姿态 280 (低趴反折膝抬腿会钳FL小腿), 抬腿50mm防拖地 */
                const char *p = g_line_buf + 1;
                int dir = 1;
                if (*p == ':') {
                    p++;
                    if (*p == '0') dir = -1;
                }
                g_flip_on = 0; g_flip_start = 0;
                DriveCtrl_Reset();
                g_booted = 0;
                g_shift_mode = SHIFT_OFF;
                g_gait_lat = 1;
                g_lat_dir  = dir;
                g_walk_params.stand_h  = STAND_H_HIGH;   /* 强制狗姿态 280 */
                g_walk_params.step_len = 30.0f;          /* 横移步宽 30mm */
                g_walk_params.step_h   = 40.0f;          /* 横移抬腿 40mm (原地抬腿结合版) */
                for (int i = 0; i < 4; i++)
                    Motor_Stop((uint8_t)i);
                if (g_front_rev || g_rear_rev) {
                    /* 反折膝抬腿会钳位: 先翻膝回正常再开走 */
                    g_flip_mask = (g_front_rev ? 1 : 0) | (g_rear_rev ? 2 : 0);
                    g_flip_revf = 0;
                    g_flip_revr = 0;
                    g_flip_post = 4;
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    g_walk_t0 = uwTick;
                    g_mode = MODE_TROT;
                }
                snprintf(msg, sizeof(msg), "SIDE:%d\r\n", dir);
                bluetooth_send(msg);
            }
            else if (f == 'W' || f == 'w') {
                /* 越障模式: 四轮30%前进 + 高站姿对角交替抬腿 (抬起→前伸→落下→回收) */
                g_flip_on = 0; g_flip_start = 0;
                DriveCtrl_Reset();
                g_booted = 0;
                g_shift_mode = SHIFT_OFF;              /* 无重心横移 */
                g_gait_lat = 0;
                g_walk_params.stand_h = STAND_H_HIGH;  /* 高站姿: 抬腿明显 */
                g_walk_params.step_h   = 70.0f;        /* 抬腿 70mm (RL小腿余量8.9°, 极限88mm) */
                g_walk_params.step_len = 50.0f;        /* 摆动相前伸50mm, 支撑相回收推身体前进 */
                if (g_front_rev || g_rear_rev) {
                    /* 反折分支抬70mm会钳小腿: 先翻膝回正常再开走 */
                    g_flip_mask = (g_front_rev ? 1 : 0) | (g_rear_rev ? 2 : 0);
                    g_flip_revf = 0;
                    g_flip_revr = 0;
                    g_flip_post = 3;
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    for (int i = 0; i < 4; i++)
                        Motor_Set((uint8_t)i, 30, 1);
                    g_walk_t0 = uwTick;
                    g_mode = MODE_TROT;
                }
                bluetooth_send("CLIMB\r\n");
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
            else if ((f == 'H' || f == 'h') && g_line_buf[1] == ':') {
                /* H:高度 正常膝低站姿 (爬陡坡用: 站得越低自稳可补偿坡度越大)
                 * 缺省230, 范围200~280; 单独 H 仍是280高站姿 */
                const char *p = g_line_buf + 2;
                int h = s_parse_num(&p);
                if (h < 200 || h > 280) h = 230;
                g_flip_on = 0; g_flip_start = 0;
                g_park = 0; g_sit = 0;
                g_booted = 0;
                g_flip_mask = 0;
                if (g_front_rev) g_flip_mask |= 1;
                if (g_rear_rev)  g_flip_mask |= 2;
                g_flip_revf = 0; g_flip_revr = 0;
                if (g_flip_mask) {
                    g_walk_params.stand_h = FLIP_H;   /* 翻膝固定在240做 (膝离地有保证) */
                    g_flip_post_h = (float)h;
                    g_flip_post = 8;                  /* 翻完降到指定高度 */
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    g_walk_params.stand_h = (float)h;
                }
                g_mode = MODE_STAND;
                snprintf(msg, sizeof(msg), "HIGH:%d\r\n", h);
                bluetooth_send(msg);
            }
            else if (f == 'H' || f == 'h') {
                /* 狗姿态高站姿 280: 前后腿都正常分支 */
                g_flip_on = 0; g_flip_start = 0;
                g_park = 0; g_sit = 0;
                g_booted = 0;
                g_flip_mask = 0;
                if (g_front_rev) g_flip_mask |= 1;
                if (g_rear_rev)  g_flip_mask |= 2;
                g_flip_revf = 0; g_flip_revr = 0;
                if (g_flip_mask) {
                    if (g_walk_params.stand_h < FLIP_H)
                        g_walk_params.stand_h = FLIP_H;   /* 先到翻膝高度 */
                    g_flip_post = 2;                      /* 翻完升 280 */
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    g_walk_params.stand_h = STAND_H_HIGH;
                }
                g_mode = MODE_STAND;
                bluetooth_send("HIGH\r\n");
            }
            else if (f == 'K' || f == 'k') {
                /* 膝盖往前顶高站姿 280: 前腿反折, 后腿正常 */
                g_flip_on = 0; g_flip_start = 0;
                g_park = 0; g_sit = 0;
                g_walk_params.stand_h = STAND_H_HIGH;
                g_flip_mask = 0;
                if (!g_front_rev) g_flip_mask |= 1;
                if (g_rear_rev)   g_flip_mask |= 2;
                g_flip_revf = 1; g_flip_revr = 0;
                if (g_flip_mask) {
                    g_flip_post = 0;                      /* 翻完保持 280 */
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                }
                g_mode = MODE_STAND;
                bluetooth_send("KNEE\r\n");
            }
            else if ((f == 'L' || f == 'l') && g_line_buf[1] == ':') {
                /* L:1/L:0 站立自稳开关 (H/K高站姿, 先Z标定; 单独L仍是低趴姿态) */
                uint8_t r = AttCtrl_LevelToggle();
                if (r == 2)      bluetooth_send("LV:NOCAL\r\n");
                else if (r == 0) bluetooth_send("LV:ON\r\n");
                else             bluetooth_send("LV:OFF\r\n");
            }
            else if (f == 'L' || f == 'l') {
                /* 低趴 240: 前腿反折, 后腿正常 */
                g_flip_on = 0; g_flip_start = 0;
                g_park = 0; g_sit = 0;
                g_booted = 0;
                g_flip_mask = 0;
                if (!g_front_rev) g_flip_mask |= 1;
                if (g_rear_rev)   g_flip_mask |= 2;
                g_flip_revf = 1; g_flip_revr = 0;
                if (g_flip_mask) {
                    g_walk_params.stand_h = FLIP_H;       /* 先到翻膝高度 */
                    g_flip_post = 1;                      /* 翻完趴到 LOW */
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    g_walk_params.stand_h = STAND_H_L_FRONT;  /* 已是目标分支: 直接趴 */
                }
                g_mode = MODE_STAND;
                bluetooth_send("LOW\r\n");
            }
            else if ((f == 'M' || f == 'm') &&
                     !(g_line_buf[1] >= '0' && g_line_buf[1] <= '9')) {
                /* 后腿膝盖往前突低趴 240: 前腿狗腿(正常), 后腿反折, 重心集中
                 * 注意: 第二字节是数字时放行给后面的 M0:50:1 电机命令 */
                g_flip_on = 0; g_flip_start = 0;
                g_park = 0; g_sit = 0;
                g_booted = 0;
                g_flip_mask = 0;
                if (g_front_rev) g_flip_mask |= 1;        /* 前腿翻回正常 */
                if (!g_rear_rev) g_flip_mask |= 2;        /* 后腿翻成反折 */
                g_flip_revf = 0; g_flip_revr = 1;
                if (g_flip_mask) {
                    g_walk_params.stand_h = FLIP_H;
                    g_flip_post = 1;
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    g_walk_params.stand_h = STAND_H_LOW;
                }
                g_mode = MODE_STAND;
                bluetooth_send("REAR\r\n");
            }
            else if (f == 'P' || f == 'p') {
                /* 后腿膝盖往前突高站姿 280: 前腿狗腿(正常), 后腿反折 */
                g_flip_on = 0; g_flip_start = 0;
                g_park = 0; g_sit = 0;
                g_walk_params.stand_h = STAND_H_HIGH;
                g_flip_mask = 0;
                if (g_front_rev) g_flip_mask |= 1;
                if (!g_rear_rev) g_flip_mask |= 2;
                g_flip_revf = 0; g_flip_revr = 1;
                if (g_flip_mask) {
                    g_flip_post = 0;                      /* 翻完保持 280 */
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                }
                g_mode = MODE_STAND;
                bluetooth_send("REARH\r\n");
            }
            else if (f == 'C' || f == 'c') {
                /* 收纳: 72mm 贴地趴 (前腿反折前伸直x+297/后腿正常后伸直x-297)
                 * 慢速缓降约2~3s, 断电后肚皮只落~32mm, U 规划式站起 */
                g_flip_on = 0; g_flip_start = 0;
                DriveCtrl_Reset();
                g_rise = 0;
                g_booted = 0;
                g_flip_mask = 0;
                if (!g_front_rev) g_flip_mask |= 1;   /* 前腿翻成反折 */
                if (g_rear_rev)   g_flip_mask |= 2;   /* 后腿翻回正常 */
                g_flip_revf = 1; g_flip_revr = 0;
                if (g_flip_mask) {
                    g_walk_params.stand_h = FLIP_H;
                    g_flip_post = 5;                  /* 翻完进贴地趴 */
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    g_park = 1;                       /* 直接慢速趴 */
                    g_walk_params.stand_h = PARK_H;
                }
                g_mode = MODE_STAND;
                bluetooth_send("PARK\r\n");
            }
            else if (f == 'U' || f == 'u') {
                /* 规划式站起: 阶段A 收腿+顶升(3.5s缓升到240) → 阶段B 翻膝升280
                 * 收纳中或刚上电(假定在趴姿)可用 */
                g_flip_on = 0; g_flip_start = 0;
                DriveCtrl_Reset();
                uint8_t need_sync = g_booted;   /* 刚上电: 指令角预置是280站姿, 需先同步到趴姿 */
                if (g_park || g_booted) {
                    g_booted = 0;
                    g_rise_h0 = g_park ? g_walk_params.stand_h : PARK_H;
                    g_park = 0; g_sit = 0;
                    g_rise = 1;
                    g_rise_t0 = uwTick;
                    LegIK_SetFrontReversed(1);        /* 阶段A按反折分支 (与趴姿一致) */
                    if (need_sync) {
                        /* 2026-08-21 修复: 上电后按U, 指令角若不先同步到趴姿,
                         * 大腿会从280站姿角爬向趴姿角 → 前腿乱扫 (FL/FR 动作不对称, FR 堵转) */
                        for (int leg = 0; leg < 4; leg++) {
                            float d_leg = (leg == 0 || leg == 2) ? -LEG_HIP_D : LEG_HIP_D;
                            float xp = (leg < 2)
                                     ? g_walk_params.foot_x_shift + PARK_X_F
                                     : g_walk_params.foot_x_shift - PARK_X_R;
                            uint16_t deg[3];
                            LegIK_SolveServo((uint8_t)leg,
                                             xp + g_walk_params.foot_x_corr[leg],
                                             d_leg, PARK_H, deg);
                            g_cmd_deg[leg]     = (float)STAND_POSE[leg][JOINT_ABD];
                            g_cmd_deg[leg + 4] = (float)deg[1];
                            g_cmd_deg[leg + 8] = (float)deg[2];
                        }
                    }
                    g_walk_params.stand_h = g_rise_h0;
                    g_mode = MODE_STAND;
                    bluetooth_send("RISE\r\n");
                } else {
                    bluetooth_send("NOT PARK\r\n");
                }
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
            else if (f == 'D' || f == 'd') {
                /* 方向按键: D:方向[:速度]  0=前进 1=后退 2=左前 3=右前
                 * 速度 10~50 (占空比%, 默认30), 固定倾斜全程保持, S 停止回平 */
                const char *p = g_line_buf + 1;
                int dir = 0;
                if (*p == ':') { p++; dir = s_parse_num(&p); }
                int spd = 30;
                if (*p == ':') { p++; spd = s_parse_num(&p); }
                DriveCtrl_SetButton((uint8_t)dir, (int8_t)spd);
                g_booted = 0;
                snprintf(msg, sizeof(msg), "DRV:%d:%d\r\n", dir, spd);
                bluetooth_send(msg);
            }
            else if (f == 'V' || f == 'v') {
                /* 摇杆驱动: V:速度:转向[:模式] (-50~+50, 速度负=后退, 转向负=左/正=右)
                 * 模式 缺省/0 = 经典倾斜平衡; 1 = 无倾斜补偿 + 高站姿280 + 自稳保持 (第二套轮盘)
                 * 高频流不回显; 差速+斜坡+倾斜平衡在 drive_ctrl 内处理 */
                const char *p = g_line_buf + 1;
                int sign = 1;
                if (*p == ':') p++;
                if (*p == '-') { sign = -1; p++; }
                int sp = s_parse_num(&p);
                if (sign < 0) sp = -sp;
                sign = 1;
                int st = 0;
                if (*p == ':') {
                    p++;
                    if (*p == '-') { sign = -1; p++; }
                    st = s_parse_num(&p);
                    if (sign < 0) st = -st;
                }
                int mode = 0;
                if (*p == ':') { p++; mode = s_parse_num(&p); }

                if (mode == 1) {
                    /* 无倾斜补偿模式: 要求高站姿 H (自稳仅 H/K 生效)
                     * 反折膝先翻回 H, 翻完前的驱动命令忽略 (高频流会重复触发, 防重入) */
                    if (g_front_rev || g_rear_rev) {
                        if (!g_flip_on && !g_flip_start) {
                            g_park = 0; g_sit = 0; g_rise = 0; g_booted = 0;
                            g_flip_mask = (g_front_rev ? 1 : 0) | (g_rear_rev ? 2 : 0);
                            g_flip_revf = 0; g_flip_revr = 0;
                            if (g_walk_params.stand_h < FLIP_H)
                                g_walk_params.stand_h = FLIP_H;
                            g_flip_post = 2;                 /* 翻完升 280 */
                            g_flip_start = uwTick + FLIP_SETTLE_MS;
                            bluetooth_send("FLIP\r\n");
                        }
                        DriveCtrl_Reset();
                    } else if (!g_park && !g_rise && !g_sit) {
                        /* 已在高站姿: 无倾斜驱动 + 自稳自动跟随 (需先 Z 标定 H) */
                        DriveCtrl_SetNoTilt(1);
                        DriveCtrl_SetCmd((int8_t)sp, (int8_t)st);
                        g_booted = 0;
                    }
                } else {
                    DriveCtrl_SetNoTilt(0);
                    DriveCtrl_SetCmd((int8_t)sp, (int8_t)st);
                    g_booted = 0;
                }
            }
            else if (f == 'J' || f == 'j') {
                /* 编码器转速上报开关: 10Hz E,帧 (帧尾520) */
                g_enc_stream = !g_enc_stream;
                bluetooth_send(g_enc_stream ? "J:ON\r\n" : "J:OFF\r\n");
            }
            else if (f == 'O' || f == 'o') {
                /* 坐姿: 前腿竖直伸直 + 后腿反折深蹲 (狗坐造型, 车身自然抬头约10°) */
                g_flip_on = 0; g_flip_start = 0;
                DriveCtrl_Reset();
                g_park = 0; g_rise = 0; g_booted = 0;
                g_walk_params.stand_h = FLIP_H;   /* 240: 后腿翻膝高度 */
                if (g_rear_rev == 0) {
                    g_flip_mask = 2;               /* 只翻后腿 */
                    g_flip_revf = g_front_rev;     /* 前腿分支不动 */
                    g_flip_revr = 1;
                    g_flip_post = 7;               /* 翻完进入坐姿 */
                    g_flip_start = uwTick + FLIP_SETTLE_MS;
                } else {
                    g_sit = 1;                     /* 后腿已是反折: 直接坐 */
                }
                g_mode = MODE_STAND;
                bluetooth_send("SIT\r\n");
            }
            else if (f == 'I' || f == 'i') {
                /* IMU 上报开关: 10Hz 角度流 (帧尾520) */
                g_imu_stream = !g_imu_stream;
                bluetooth_send(g_imu_stream ? "IMU:ON\r\n" : "IMU:OFF\r\n");
                g_imu_diag = 1;  /* 诊断挪到主循环发 (ISR 里连发两条会卡 DMA) */
            }
            else if (f == 'Z' || f == 'z') {
                /* 自稳标定: 采样2s存当前姿态 pitch/roll 零偏 (放平地保持静止) */
                AttCtrl_CalibStart();
                g_calib_pending = 1;
                bluetooth_send("CALIB...\r\n");
            }
            else if (f == 'R' || f == 'r') {
                /* 一键滚动: 四电机 30% 占空比正转 (A/B方向已在Motor_Set内纠正) */
                DriveCtrl_Reset();   /* 接管电机前先复位摇杆驱动 */
                g_booted = 0;
                for (int i = 0; i < 4; i++)
                    Motor_Set((uint8_t)i, 30, 1);
                bluetooth_send("ROLL\r\n");
            }
            else if (f == 'B' || f == 'b') {
                /* 一键后退: 四电机 30% 占空比反转 */
                DriveCtrl_Reset();
                g_booted = 0;
                for (int i = 0; i < 4; i++)
                    Motor_Set((uint8_t)i, 30, 0);
                bluetooth_send("BACK\r\n");
            }
            else if (f == 'S' || f == 's') {
                /* 一键停: 四电机全停 */
                DriveCtrl_Reset();
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
/* 必做修复2 (每次 Generate 后手动替换):
 * D1/D2 RAM 全部非缓存 — 蓝牙 TX 走 DMA, D-Cache 一致性依赖此配置 */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  HAL_MPU_Disable();

  /* D2 SRAM 区: 非缓存 */
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

  /* 整个 D1 RAM 0x24000000: 非缓存
   * ⚠ 基地址必须按区域大小对齐, 0x24010000 不是512KB对齐会静默失败! */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x24000000;
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
