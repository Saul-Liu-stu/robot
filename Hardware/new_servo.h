/*
 * new_servo.h
 * 0-270° 舵机驱动（12舵机版）
 */

#ifndef INC_NEW_SERVO_H_
#define INC_NEW_SERVO_H_

#include "main.h"

/* 舵机编号 */
#define NEW_SERVO_1   1   /* PA0  TIM2_CH1  左前腿髋关节 */
#define NEW_SERVO_2   2   /* PA7  TIM3_CH2  左前腿膝关节 */
#define NEW_SERVO_3   3   /* PD15 TIM4_CH4  右前腿髋关节 */
#define NEW_SERVO_4   4   /* PD14 TIM4_CH3  右前腿膝关节 */
#define NEW_SERVO_5   5   /* PB0  TIM3_CH3  左后腿髋关节 */
#define NEW_SERVO_6   6   /* PB1  TIM3_CH4  左后腿膝关节 */
#define NEW_SERVO_7   7   /* PD13 TIM4_CH2  右后腿髋关节 */
#define NEW_SERVO_8   8   /* PD12 TIM4_CH1  右后腿膝关节 */
#define NEW_SERVO_9   9   /* PA1  TIM2_CH2 */
#define NEW_SERVO_10 10   /* PA2  TIM2_CH3 */
#define NEW_SERVO_11 11   /* PA3  TIM2_CH4 */
#define NEW_SERVO_12 12   /* PB14 TIM12_CH1 */

/* 角度范围 */
#define NEW_SERVO_ANGLE_MIN    0
#define NEW_SERVO_ANGLE_MAX  270

void NewServo_SetAngle(uint16_t servo_n, uint16_t angle);
void NewServo_BatchControl(const uint16_t *angles);
void NewServo_StartAll(void);   /* 启动全部 12 路舵机 PWM */
int  NewServo_Map(int x, int in_min, int in_max, int out_min, int out_max);

#endif /* INC_NEW_SERVO_H_ */
