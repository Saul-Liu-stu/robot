/*
 * gimbal.c
 * 云台双舵机控制 (TIM13 CH1: PF8 水平 pan, TIM14 CH1: PF9 俯仰 tilt, 50Hz PWM)
 *
 * 180° 数字舵机: 0.5~2.5ms 脉宽对应 0~180°
 * 配置: PSC=23999, ARR=199 → 240MHz/24000/200 = 50Hz, CCR单位=0.1ms
 * 角度 → CCR: angle/9 + 5  (0° → 5=0.5ms, 180° → 25=2.5ms, 分辨率约9°/tick)
 * TIM13/14 为通用定时器, 无需 MOE (2026-08-22 由 TIM8 换到此处)
 */
#include "gimbal.h"
#include "main.h"

extern TIM_HandleTypeDef htim13;
extern TIM_HandleTypeDef htim14;

#define GIMBAL_ANGLE_MAX  180   /* 180° 舵机全行程 */

/* 上电初始角度: 0=保持不动(CCR=0无脉冲, 舵机不使力);
 * 实测好初始角度后填入并置 1, 上电即回中 */
#define GIMBAL_BOOT_MOVE  0

static uint16_t s_angle[2] = { 90, 90 };   /* 初始角度候选 (水平/俯仰), 待实测 */

/* 角度 → CCR (0.5~2.5ms 脉宽) */
static uint16_t AngleToPulse(uint16_t angle)
{
    return (uint16_t)(angle / 9u) + 5u;
}

void Gimbal_Init(void)
{
    HAL_TIM_PWM_Start(&htim13, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1);
#if GIMBAL_BOOT_MOVE
    __HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, AngleToPulse(s_angle[GIMBAL_PAN]));
    __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, AngleToPulse(s_angle[GIMBAL_TILT]));
#else
    __HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, 0);   /* 无脉冲: 上电保持不动 */
    __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, 0);
#endif
}

void Gimbal_Set(uint8_t ch, uint16_t angle_deg)
{
    if (ch > GIMBAL_TILT) return;
    if (angle_deg > GIMBAL_ANGLE_MAX) angle_deg = GIMBAL_ANGLE_MAX;
    s_angle[ch] = angle_deg;
    if (ch == GIMBAL_PAN)
        __HAL_TIM_SET_COMPARE(&htim13, TIM_CHANNEL_1, AngleToPulse(angle_deg));
    else
        __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, AngleToPulse(angle_deg));
}

uint16_t Gimbal_Get(uint8_t ch)
{
    return (ch > GIMBAL_TILT) ? 0 : s_angle[ch];
}
