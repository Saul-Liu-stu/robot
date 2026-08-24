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

/* 站姿高度 (mm): 低=四轮驱动模式(低趴240, 腿前后外伸, 不走路), 高=行走模式(抬腿明显) */
#define STAND_H_LOW    240.0f
#define STAND_H_HIGH   280.0f
#define STAND_H_L_FRONT 191.3f /* 前顶低趴(L)专用站高: 大腿≈水平(q2=85)+小腿垂直的视觉姿态 */

typedef struct { float x, y, z; } walk_vec3_t;

typedef struct {
    float step_len;   /* 步幅 S (mm), 前进速度 ≈ step_len/period */
    float step_h;     /* 抬腿高度 H (mm) */
    float period;     /* 步态周期 T (s) */
    float duty;       /* 支撑相比例 beta ∈ (0,1) */
    float stand_h;    /* 站立足端 z (mm, 地面) */
    float foot_x_shift; /* 足端前移修正 (mm): 正值=足端前移/身体后坐, 修正重心偏前导致的身体前倾 */
    float y_shift;    /* 重心横移 (mm): 摆动相期间身体向支撑对角侧横移, 抬腿前就位 */
    float foot_x_corr[4]; /* 单腿足端x零位修正 (mm): RR 落地点偏前 ~30mm (小腿零位偏差补偿) */
} walk_params_t;

/* 步态参数 (默认值见 walk_gait.c), 运行时可改 */
extern walk_params_t g_walk_params;

/* 重心横移模式 (T/E 命令选择, 幅值由 y_shift 决定) */
#define SHIFT_OFF  0   /* 无横移 (纯 trot) */
#define SHIFT_IK   1   /* IK 足端横移 (整条腿重新解算) */
extern uint8_t g_shift_mode;
extern int     g_shift_sign;   /* 横移方向: +1 顺时针旋转, -1 逆时针 (E:0 切换) */

void WalkGait_Init(void);

/* 身体横移量 (mm): 摆动相期间向支撑对角侧横移, 见 walk_gait.c */
float WalkGait_BodyShiftY(float t);

/*
 * 横移走 (螃蟹步) 足端轨迹: 扫步方向从前后(x)换成左右(y), x 固定。
 * 支撑相足端反向扫 (推身体侧移), 摆动相抬腿扫回前方; 对角交替与 trot 相同。
 * @param dir  行走方向: +1 = 身体向 +y (右), -1 = 向 -y (左)
 */
void WalkGait_FootTargetLat(uint8_t leg, float t, float d_signed, int dir,
                            walk_vec3_t *out);

/*
 * 某腿某时刻的足端目标 (腿系 FRD, mm)
 * @param leg       0=FL 1=FR 2=RL 3=RR
 * @param t         行走时间 (s)
 * @param d_signed  该腿带符号髋偏置 (左负右正)
 * @param out       输出足端目标
 */
void WalkGait_FootTarget(uint8_t leg, float t, float d_signed, walk_vec3_t *out);

/* 该腿当前是否摆动相 (与 FootTarget 同口径, W原地抬腿自稳用) */
uint8_t WalkGait_IsSwing(uint8_t leg, float t);

#endif /* INC_WALK_GAIT_H_ */
