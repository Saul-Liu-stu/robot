/*
 * new_servo.c
 * 0-270° 12舵机驱动
 *
 * PWM 参数（50Hz 舵机）:
 *   TIM2/3/4/12: PSC=23999, Period=199
 *   时钟=240MHz → 计数器=10kHz, CCR单位=0.1ms
 *
 * 角度 → CCR: (angle × 2) / 27 + 5
 *   0°   → 5  (0.5ms)    135° → 15 (1.5ms)
 *   270° → 25 (2.5ms)
 */

#include "new_servo.h"

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim12;

void NewServo_SetAngle(uint16_t servo_n, uint16_t angle)
{
    if (angle > NEW_SERVO_ANGLE_MAX)
        angle = NEW_SERVO_ANGLE_MAX;

    uint16_t pulse = (uint16_t)(((uint32_t)angle * 2u) / 27u) + 5u;

    switch (servo_n)
    {
        case 1:  /* PA0  TIM2_CH1 */
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse); break;
        case 2:  /* PA7  TIM3_CH2 */
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse); break;
        case 3:  /* PD15 TIM4_CH4 */
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, pulse); break;
        case 4:  /* PD14 TIM4_CH3 */
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, pulse); break;
        case 5:  /* PB0  TIM3_CH3 */
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pulse); break;
        case 6:  /* PB1  TIM3_CH4 */
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, pulse); break;
        case 7:  /* PD13 TIM4_CH2 */
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pulse); break;
        case 8:  /* PD12 TIM4_CH1 */
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pulse); break;
        case 9:  /* PA1  TIM2_CH2 */
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse); break;
        case 10: /* PA2  TIM2_CH3 */
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse); break;
        case 11: /* PA3  TIM2_CH4 */
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, pulse); break;
        case 12: /* PB14 TIM12_CH1 */
            __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, pulse); break;
        default: break;
    }
}

void NewServo_BatchControl(const uint16_t *angles)
{
    uint16_t ccr[12];

    for (int i = 0; i < 12; i++)
    {
        uint16_t a = angles[i];
        if (a > NEW_SERVO_ANGLE_MAX) a = NEW_SERVO_ANGLE_MAX;
        ccr[i] = (uint16_t)(((uint32_t)a * 2u) / 27u) + 5u;
    }

    __HAL_TIM_SET_COMPARE(&htim2,  TIM_CHANNEL_1, ccr[0]);  /*  1 PA0 */
    __HAL_TIM_SET_COMPARE(&htim3,  TIM_CHANNEL_2, ccr[1]);  /*  2 PA7 */
    __HAL_TIM_SET_COMPARE(&htim4,  TIM_CHANNEL_4, ccr[2]);  /*  3 PD15 */
    __HAL_TIM_SET_COMPARE(&htim4,  TIM_CHANNEL_3, ccr[3]);  /*  4 PD14 */
    __HAL_TIM_SET_COMPARE(&htim3,  TIM_CHANNEL_3, ccr[4]);  /*  5 PB0 */
    __HAL_TIM_SET_COMPARE(&htim3,  TIM_CHANNEL_4, ccr[5]);  /*  6 PB1 */
    __HAL_TIM_SET_COMPARE(&htim4,  TIM_CHANNEL_2, ccr[6]);  /*  7 PD13 */
    __HAL_TIM_SET_COMPARE(&htim4,  TIM_CHANNEL_1, ccr[7]);  /*  8 PD12 */
    __HAL_TIM_SET_COMPARE(&htim2,  TIM_CHANNEL_2, ccr[8]);  /*  9 PA1 */
    __HAL_TIM_SET_COMPARE(&htim2,  TIM_CHANNEL_3, ccr[9]);  /* 10 PA2 */
    __HAL_TIM_SET_COMPARE(&htim2,  TIM_CHANNEL_4, ccr[10]); /* 11 PA3 */
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, ccr[11]); /* 12 PB14 */
}

int NewServo_Map(int x, int in_min, int in_max, int out_min, int out_max)
{
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/* 启动全部 12 路舵机 PWM：TIM2/3/4/12 所有通道 */
void NewServo_StartAll(void)
{
    HAL_TIM_PWM_Start(&htim2,  TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2,  TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2,  TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2,  TIM_CHANNEL_4);
    HAL_TIM_PWM_Start(&htim3,  TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3,  TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3,  TIM_CHANNEL_4);
    HAL_TIM_PWM_Start(&htim4,  TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4,  TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim4,  TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim4,  TIM_CHANNEL_4);
    HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
}
