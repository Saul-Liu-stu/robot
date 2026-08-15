/*
 * walk_gait.h
 * trot 足端轨迹发生器 (相位归一化版)
 *
 * 每条腿相位 phi ∈ [0,1) 随时间推进:
 *   支撑相 (phi < duty):     足端贴地从 +S/2 匀速移到 -S/2 (推身体前进)
 *   摆动相 (phi >= duty):    足端沿半椭圆弧线抬腿回到 +S/2
 *   trot 相位: FL=RR=0, FR=RL=0.5 (对角腿同步)
 *
 * 输出腿系足端目标 (mm, FRD), 喂给 LegIK_SolveServo()。
 * 参数在线可调: 改 g_walk_params, 无需重新烧录。
 */
#ifndef INC_WALK_GAIT_H_
#define INC_WALK_GAIT_H_

#include <stdint.h>

typedef struct { float x, y, z; } walk_vec3_t;

typedef struct {
    float step_len;   /* 步幅 S (mm), 前进速度 ≈ step_len/period */
    float step_h;     /* 抬腿高度 H (mm) */
    float period;     /* 步态周期 T (s) */
    float duty;       /* 支撑相比例 beta ∈ (0,1) */
    float stand_h;    /* 站立足端 z (mm, 地面) */
} walk_params_t;

/* 步态参数 (默认值见 walk_gait.c), 运行时可改 */
extern walk_params_t g_walk_params;

void WalkGait_Init(void);

/*
 * 某腿某时刻的足端目标 (腿系 FRD, mm)
 * @param leg       0=FL 1=FR 2=RL 3=RR
 * @param t         行走时间 (s)
 * @param d_signed  该腿带符号髋偏置 (左负右正)
 * @param out       输出足端目标
 */
void WalkGait_FootTarget(uint8_t leg, float t, float d_signed, walk_vec3_t *out);

#endif /* INC_WALK_GAIT_H_ */
