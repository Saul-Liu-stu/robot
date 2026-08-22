/*
 * leg_control.h
 * 四足机器人 — 单腿控制 (3舵机 + 1电机)
 *
 * 每条腿关节:
 *   joint 0 = 外展 (abduction, Servo 1~4)
 *   joint 1 = 大腿 (thigh,     Servo 5~8)
 *   joint 2 = 小腿 (calf,      Servo 9~12)
 */

#ifndef INC_LEG_CONTROL_H_
#define INC_LEG_CONTROL_H_

#include "main.h"
#include "leg_config.h"

/* 腿编号 */
#define LEG_1  0
#define LEG_2  1
#define LEG_3  2
#define LEG_4  3

/* 关节编号 */
#define JOINT_ABD    0   /* 外展 */
#define JOINT_THIGH  1   /* 大腿 */
#define JOINT_CALF   2   /* 小腿 */

/* 初始化: 全部进入站立姿态 */
void Leg_Init(void);

/* 设置单关节: leg(0~3), joint(0~2), angle(0~270) */
void Leg_SetJoint(uint8_t leg, uint8_t joint, uint16_t angle);

/* 设置单腿全部关节 */
void Leg_SetPose(uint8_t leg, uint16_t abd, uint16_t thigh, uint16_t calf);

/* 设置轮子: speed(0~100), dir(1=前进 0=后退) */
void Leg_SetWheel(uint8_t leg, uint8_t speed, uint8_t dir);

/* 停止单腿轮子 */
void Leg_StopWheel(uint8_t leg);

/* 单腿站立 */
void Leg_Stand(uint8_t leg);

/* 四足站立 + 轮停 */
void Leg_AllStand(void);

/* 四轮驱动 */
void Leg_RaceForward(uint8_t speed);
void Leg_RaceBackward(uint8_t speed);

/* 全停 */
void Leg_AllStop(void);

#endif /* INC_LEG_CONTROL_H_ */
