/*
 * motor_control.c
 * 4路 TB6612 + MG513 直流电机驱动
 *
 * TB6612 真值表 (单路):
 *   IN1=1 IN2=0  →  正转
 *   IN1=0 IN2=1  →  反转
 *   IN1=0 IN2=0  →  惯性停止
 *   IN1=1 IN2=1  →  刹车
 *   STBY=0       →  全部停止
 */

#include "motor_control.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim16;
extern TIM_HandleTypeDef htim17;

/* 每个电机的引脚/定时器信息 */
typedef struct {
    GPIO_TypeDef *in1_port; uint16_t in1_pin;
    GPIO_TypeDef *in2_port; uint16_t in2_pin;
    TIM_HandleTypeDef *htim;
    uint32_t channel;
} MotorPin_t;

static const MotorPin_t motor_pins[4] = {
    /* A */ { AIN1_GPIO_Port, AIN1_Pin, AIN2_GPIO_Port, AIN2_Pin, &htim1,  TIM_CHANNEL_1 },
    /* B */ { BIN1_GPIO_Port, BIN1_Pin, BIN2_GPIO_Port, BIN2_Pin, &htim1,  TIM_CHANNEL_2 },
    /* C */ { CIN1_GPIO_Port, CIN1_Pin, CIN2_GPIO_Port, CIN2_Pin, &htim16, TIM_CHANNEL_1 },
    /* D */ { DIN1_GPIO_Port, DIN1_Pin, DIN2_GPIO_Port, DIN2_Pin, &htim17, TIM_CHANNEL_1 },
};

/* ==================== 公开函数 ==================== */

void Motor_Init(void)
{
    HAL_TIM_PWM_Start(&htim1,  TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1,  TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
    __HAL_TIM_MOE_ENABLE(&htim1);
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_SET);
}

void Motor_Set(uint8_t motor, uint8_t speed, uint8_t dir)
{
    if (motor > MOTOR_D) return;
    if (speed > MOTOR_SPEED_MAX) speed = MOTOR_SPEED_MAX;

    /* A/B 电机模块接线方向相反, 反转 dir */
    if (motor == MOTOR_A || motor == MOTOR_B)
        dir = !dir;

    const MotorPin_t *m = &motor_pins[motor];

    if (dir) {
        HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_SET);
    }
    __HAL_TIM_SET_COMPARE(m->htim, m->channel, speed);
}

void Motor_Stop(uint8_t motor)
{
    if (motor > MOTOR_D) return;

    const MotorPin_t *m = &motor_pins[motor];
    HAL_GPIO_WritePin(m->in1_port, m->in1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(m->in2_port, m->in2_pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(m->htim, m->channel, 0);
}

void Motor_Standby(void)
{
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_RESET);
}

void Motor_Wakeup(void)
{
    HAL_GPIO_WritePin(STBY_GPIO_Port, STBY_Pin, GPIO_PIN_SET);
}

/* ==================== 旧接口兼容 ==================== */

void Motor_A_Forward(uint8_t speed)  { Motor_Set(MOTOR_A, speed, 1); }
void Motor_A_Backward(uint8_t speed) { Motor_Set(MOTOR_A, speed, 0); }
void Motor_A_Stop(void)              { Motor_Stop(MOTOR_A); }

void Motor_B_Forward(uint8_t speed)  { Motor_Set(MOTOR_B, speed, 1); }
void Motor_B_Backward(uint8_t speed) { Motor_Set(MOTOR_B, speed, 0); }
void Motor_B_Stop(void)              { Motor_Stop(MOTOR_B); }

void Motor_Stop_All(void)
{
    Motor_Stop(MOTOR_A);
    Motor_Stop(MOTOR_B);
    Motor_Stop(MOTOR_C);
    Motor_Stop(MOTOR_D);
}
