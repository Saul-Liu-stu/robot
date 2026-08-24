/*
 * leg_config.h
 * 四足机器人 — 全部可调参数集中管理
 *
 * 腿部映射:
 *   Leg 1: Servo[1,5,9]  + Motor A
 *   Leg 2: Servo[2,6,10] + Motor B
 *   Leg 3: Servo[3,7,11] + Motor C
 *   Leg 4: Servo[4,8,12] + Motor D
 *
 *   伺服分组:
 *     Servo 1~4   = 外展关节 (abduction) — 腿向身体两侧展开/收拢
 *     Servo 5~8   = 大腿关节 (thigh)     — 髋部俯仰, 前后摆动
 *     Servo 9~12  = 小腿关节 (calf)      — 膝部弯曲
 */

#ifndef INC_LEG_CONFIG_H_
#define INC_LEG_CONFIG_H_

#include <stdint.h>

/* ==================== 外展关节 (Servo 1~4) ==================== */
#define ABD_ANGLE_MIN       0
#define ABD_ANGLE_MAX     270
#define ABD_ANGLE_STAND   135    /* 站立位(腿竖直向下) */

/* ==================== 大腿关节 (Servo 5~8) ==================== */
#define THIGH_ANGLE_MIN     0
#define THIGH_ANGLE_MAX   270
#define THIGH_ANGLE_STAND 150    /* 站立位(大腿前倾) */

/* ==================== 小腿关节 (Servo 9~12) ==================== */
#define CALF_ANGLE_MIN       0
#define CALF_ANGLE_MAX     270
#define CALF_ANGLE_STAND  120    /* 站立位(膝微曲) */

/* ==================== 电机速度 ==================== */
#define WHEEL_SPEED_MAX   100
#define WHEEL_SPEED_STOP    0

/* ==================== 站立姿态 [leg][joint] ==================== */
/* 仅外展列在用 (上电锁定); 大腿/小腿标定已迁入 leg_ik.c 的 g_leg_calib */
static const uint16_t STAND_POSE[4][3] = {
    /* {外展, 大腿, 小腿} */
    {  95, 240, 153 },  /* Leg 1 左前 */
    {  95,  35,  74 },  /* Leg 2 右前 */
    {  50, 135, 220 },  /* Leg 3 左后 */
    { 150, 200,  30 },  /* Leg 4 右后 */
};

/* ==================== 步态参数 ==================== */
/* 已迁入 walk_gait.c 的 g_walk_params (步幅/抬腿/周期/支撑比/站高), 运行时可调 */

/* ==================== 轮式模式参数 ==================== */
#define RACE_SPEED_FAST     80
#define RACE_SPEED_NORMAL   50
#define RACE_SPEED_SLOW     30
#define RACE_SPEED_TURN     40

/* ==================== 通用宏 ==================== */
#ifndef CLAMP
#define CLAMP(x, lo, hi)  ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

#endif /* INC_LEG_CONFIG_H_ */
