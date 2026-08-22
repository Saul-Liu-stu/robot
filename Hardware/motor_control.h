/*
 * motor_control.h
 * 4路 TB6612 直流电机驱动 (MG513)
 *
 * 引脚:
 *   A: PA8(TIM1_CH1) + PE0(AIN1) + PE1(AIN2)
 *   B: PA9(TIM1_CH2) + PE2(BIN1) + PE3(BIN2)
 *   C: PB8(TIM16_CH1) + PB5(CIN1) + PB6(CIN2)
 *   D: PB9(TIM17_CH1) + PB7(DIN1) + PB12(DIN2)
 *   STBY: PA10 (低=全停, 高=使能)
 *
 * PWM: 10kHz, ARR=99, speed 0~100 直接对应 CCR
 */

#ifndef INC_MOTOR_CONTROL_H_
#define INC_MOTOR_CONTROL_H_

#include "main.h"

/* 电机编号 */
#define MOTOR_A  0
#define MOTOR_B  1
#define MOTOR_C  2
#define MOTOR_D  3

/* 速度范围 */
#define MOTOR_SPEED_MAX  100

/* 初始化: 启动PWM + 方向复位 + STBY使能 (由main.c调用) */
void Motor_Init(void);

/* 单电机: speed 0~100, dir: 1=正转 0=反转 */
void Motor_Set(uint8_t motor, uint8_t speed, uint8_t dir);

/* 停止单个电机 */
void Motor_Stop(uint8_t motor);

/* 全部停止 + 待机 */
void Motor_Standby(void);

/* 唤醒 (STBY=1) */
void Motor_Wakeup(void);

/* ====== 旧接口兼容 ====== */
void Motor_A_Forward(uint8_t speed);
void Motor_A_Backward(uint8_t speed);
void Motor_A_Stop(void);
void Motor_B_Forward(uint8_t speed);
void Motor_B_Backward(uint8_t speed);
void Motor_B_Stop(void);
void Motor_Stop_All(void);

#endif
