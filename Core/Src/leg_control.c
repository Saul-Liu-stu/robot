/*
 * leg_control.c
 * 四足机器人 — 单腿控制实现（含缓启动）
 *
 * servo_map[leg][joint] → 舵机编号
 *   Leg 1: {1,5,9}    Leg 2: {2,6,10}
 *   Leg 3: {3,7,11}   Leg 4: {4,8,12}
 *
 *   joint 0 = 外展(abd)    — Servo 1~4
 *   joint 1 = 大腿(thigh)  — Servo 5~8
 *   joint 2 = 小腿(calf)   — Servo 9~12
 *
 * motor_map[leg] → 电机编号
 *   Leg 1→A, Leg 2→B, Leg 3→C, Leg 4→D
 */

#include "leg_control.h"
#include "new_servo.h"
#include "motor_control.h"

static const uint8_t servo_map[4][3] = {
    {  1,  5,  9 },   /* Leg 1 */
    {  2,  6, 10 },   /* Leg 2 */
    {  3,  7, 11 },   /* Leg 3 */
    {  4,  8, 12 },   /* Leg 4 */
};

static const uint8_t motor_map[4] = {
    MOTOR_A, MOTOR_B, MOTOR_C, MOTOR_D
};

/* ================ 公开函数 ================ */

/**
 * @brief  缓启动初始化
 * @note   上电时12个舵机先进入安全放松姿态，
 *         再分15步(每步100ms)渐进到站立姿态，
 *         避免瞬间大电流冲击导致复位。
 */
void Leg_Init(void)
{
    /* 第1步：放松姿态——关节回中，减少扭矩突变 */
    const uint16_t relaxed[4][3] = {
        { ABD_ANGLE_STAND,  90,  90 },  /* Leg 1 */
        { ABD_ANGLE_STAND,  90,  90 },  /* Leg 2 */
        { ABD_ANGLE_STAND,  90,  90 },  /* Leg 3 */
        { ABD_ANGLE_STAND,  90,  90 },  /* Leg 4 */
    };

    for (int leg = 0; leg < 4; leg++) {
        Leg_SetPose(leg,
            relaxed[leg][JOINT_ABD],
            relaxed[leg][JOINT_THIGH],
            relaxed[leg][JOINT_CALF]);
    }
    HAL_Delay(300);  /* 等待舵机到达放松姿态 */

    /* 第2步：15步渐进到站立姿态 (1.5秒) */
    for (int step = 1; step <= 15; step++) {
        for (int leg = 0; leg < 4; leg++) {
            uint16_t abd   = relaxed[leg][0] + (STAND_POSE[leg][0] - relaxed[leg][0]) * step / 15;
            uint16_t thigh = relaxed[leg][1] + (STAND_POSE[leg][1] - relaxed[leg][1]) * step / 15;
            uint16_t calf  = relaxed[leg][2] + (STAND_POSE[leg][2] - relaxed[leg][2]) * step / 15;
            Leg_SetPose(leg, abd, thigh, calf);
        }
        HAL_Delay(100);
    }
}

void Leg_SetJoint(uint8_t leg, uint8_t joint, uint16_t angle)
{
    if (leg > LEG_4 || joint > JOINT_CALF) return;
    NewServo_SetAngle(servo_map[leg][joint], angle);
}

void Leg_SetPose(uint8_t leg, uint16_t abd, uint16_t thigh, uint16_t calf)
{
    Leg_SetJoint(leg, JOINT_ABD,   abd);
    Leg_SetJoint(leg, JOINT_THIGH, thigh);
    Leg_SetJoint(leg, JOINT_CALF,  calf);
}

void Leg_SetWheel(uint8_t leg, uint8_t speed, uint8_t dir)
{
    if (leg > LEG_4) return;
    if (speed > WHEEL_SPEED_MAX) speed = WHEEL_SPEED_MAX;
    Motor_Set(motor_map[leg], speed, dir);
}

void Leg_StopWheel(uint8_t leg)
{
    if (leg > LEG_4) return;
    Motor_Stop(motor_map[leg]);
}

void Leg_Stand(uint8_t leg)
{
    if (leg > LEG_4) return;
    Leg_SetPose(leg,
        STAND_POSE[leg][JOINT_ABD],
        STAND_POSE[leg][JOINT_THIGH],
        STAND_POSE[leg][JOINT_CALF]);
}

void Leg_AllStand(void)
{
    for (uint8_t i = LEG_1; i <= LEG_4; i++) {
        Leg_Stand(i);
        Leg_StopWheel(i);
    }
}

void Leg_RaceForward(uint8_t speed)
{
    Leg_AllStand();
    for (uint8_t i = LEG_1; i <= LEG_4; i++) {
        Leg_SetWheel(i, speed, 1);
    }
}

void Leg_RaceBackward(uint8_t speed)
{
    Leg_AllStand();
    for (uint8_t i = LEG_1; i <= LEG_4; i++) {
        Leg_SetWheel(i, speed, 0);
    }
}

void Leg_AllStop(void)
{
    for (uint8_t i = LEG_1; i <= LEG_4; i++) {
        Leg_StopWheel(i);
    }
    Motor_Standby();
}
