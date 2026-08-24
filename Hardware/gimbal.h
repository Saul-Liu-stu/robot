/*
 * gimbal.h
 * 云台双舵机控制 (TIM13 CH1: PF8 pan / TIM14 CH1: PF9 tilt)
 */
#ifndef INC_GIMBAL_H_
#define INC_GIMBAL_H_

#include <stdint.h>

/* 云台通道: 0=水平 pan (PF8/TIM13_CH1), 1=俯仰 tilt (PF9/TIM14_CH1)
 * 180° 舵机: pan 0~180 (90正前), tilt 75~180 (120正前), 超出限幅 */
#define GIMBAL_PAN   0
#define GIMBAL_TILT  1

void     Gimbal_Init(void);
void     Gimbal_Set(uint8_t ch, uint16_t angle_deg);   /* 超出限幅 */
uint16_t Gimbal_Get(uint8_t ch);

#endif /* INC_GIMBAL_H_ */
