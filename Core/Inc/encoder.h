/*
 * encoder.h
 * GMR 500PPR 编码器 — EXTI 双沿计数
 *
 * 接线: ENC1_A=PE7(EXTI双沿), ENC1_B=PE8(方向判断)
 * 四倍频: 500PPR × 4 = 2000 脉冲/圈
 */
#ifndef INC_ENCODER_H_
#define INC_ENCODER_H_

#include "main.h"

/* 编码器ID */
#define ENCODER_1  0
#define ENCODER_2  1
#define ENCODER_3  2
#define ENCODER_4  3

/* 校准参数 */
#define ENC_PPR        500   /* 编码器每圈脉冲数（确认：手转一圈ENC增量÷2） */
#define GEAR_RATIO     28    /* 减速比 1:28，编码器在电机轴上 */

/* 初始化: 复位计数 */
void Encoder_Init(void);

/* 读取累计脉冲数（有符号，正转+ 反转-） */
int32_t Encoder_GetCount(uint8_t id);

/* 读取转速 (RPM) */
float Encoder_GetRPM(uint8_t id);

/* 清零指定编码器 */
void Encoder_Reset(uint8_t id);

/* EXTI回调（stm32h7xx_it.c自动调用） */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#endif
