/*
 * walk_gait.c — trot 足端轨迹发生器
 * 参数用用户实测: 站高 240mm (狗形深蹲), 抬腿 25mm (RR小腿会钳位, 已接受)
 */
#include "walk_gait.h"
#include <math.h>

/* trot 相位偏移: FL 与 RR 同相, FR 与 RL 同相, 两组错半拍 */
static const float PHASE_TROT[4] = { 0.0f, 0.5f, 0.5f, 0.0f };

walk_params_t g_walk_params = {
    .step_len = 60.0f,    /* 步幅 60mm → 前进速度 ≈ 50mm/s */
    .step_h   = 25.0f,    /* 抬腿 25mm */
    .period   = 1.2f,     /* 周期 1.2s (~0.83Hz) */
    .duty     = 0.6f,     /* 支撑相 60% */
    .stand_h  = STAND_H_HIGH,  /* 上电默认狗高站姿 280 (G=狗高, L/M切低趴) */
    .foot_x_shift = 15.0f,  /* 前倾修正 (最终实测值), 蓝牙 X: 命令仍可在线调 */
    .y_shift = 15.0f,       /* 重心横移 15mm (定死, 仅 E 转圈动作用) */
    .foot_x_corr = { 0.0f, 0.0f, 0.0f, -20.0f },  /* RR 足端后移20mm (-30偏后回退, 落地点偏前补偿) */
};

/* 重心横移模式: 默认关 (T 纯 trot), E 命令开启 */
uint8_t g_shift_mode = SHIFT_OFF;
int     g_shift_sign = 1;   /* +1 顺时针旋转, -1 逆时针 */

/*
 * 重心横移: 摆动相期间身体向支撑对角侧横移 y_shift。
 * 全局相位 (FL=0): FL+RR 摆动 [0.6,1.0) → 身体 +Y; FR+RL 摆动 [0.1,0.5) → 身体 -Y。
 * 斜坡在四足着地窗口内完成, 抬腿前重心已就位。返回身体横移量 (mm)。
 */
float WalkGait_BodyShiftY(float t)
{
    float phi = t / g_walk_params.period;
    phi = phi - floorf(phi);
    float Y = g_walk_params.y_shift * (float)g_shift_sign;   /* 符号决定旋转方向 */
    if (phi < 0.1f)  return Y * (1.0f - 20.0f * phi);   /* +Y -> -Y 斜坡 (四足着地) */
    if (phi < 0.5f)  return -Y;
    if (phi < 0.6f)  return Y * (20.0f * phi - 11.0f);  /* -Y -> +Y 斜坡 (四足着地) */
    return Y;
}

void WalkGait_Init(void)
{
    /* 参数已有编译期默认值, 预留在线重置用 */
}

void WalkGait_FootTarget(uint8_t leg, float t, float d_signed, walk_vec3_t *out)
{
    float S    = g_walk_params.step_len;
    float H    = g_walk_params.step_h;
    float beta = g_walk_params.duty;

    float phi = t / g_walk_params.period + PHASE_TROT[leg & 3];
    phi = phi - floorf(phi);                    /* phi ∈ [0,1) */

    if (phi < beta) {
        /* 支撑相: 贴地从前(+S/2)匀速移到后(-S/2) */
        float u = phi / beta;
        out->x = S * 0.5f - S * u;
        out->z = g_walk_params.stand_h;
    } else {
        /* 摆动相: 半椭圆弧线抬腿回到前方 */
        float u = (phi - beta) / (1.0f - beta);
        out->x = -S * 0.5f + S * u;
        out->z = g_walk_params.stand_h - H * sinf(3.14159265f * u);
    }
    out->x += g_walk_params.foot_x_shift + g_walk_params.foot_x_corr[leg];   /* 整体前移修正 + 单腿零位修正 */
    /* 足端 y = 髋偏置 - 身体横移 (足端反向 = 身体正向), 仅 IK 横移模式生效 */
    out->y = d_signed - ((g_shift_mode == SHIFT_IK) ? WalkGait_BodyShiftY(t) : 0.0f);
}

/*
 * 横移走 (螃蟹步): 与 trot 同结构, 扫步从 x 换到 y。
 * step_len 复用为横移步宽, step_h 复用为横移抬腿高度。
 */
void WalkGait_FootTargetLat(uint8_t leg, float t, float d_signed, int dir,
                            walk_vec3_t *out)
{
    float W    = g_walk_params.step_len;
    float H    = g_walk_params.step_h;
    float beta = g_walk_params.duty;

    float phi = t / g_walk_params.period + PHASE_TROT[leg & 3];
    phi = phi - floorf(phi);

    if (phi < beta) {
        /* 支撑相: 足端从 +W/2 反向扫到 -W/2 (推身体侧移) */
        float u = phi / beta;
        out->y = d_signed + dir * (W * 0.5f - W * u);
        out->z = g_walk_params.stand_h;
    } else {
        /* 摆动相: 抬腿从 -W/2 扫回 +W/2 */
        float u = (phi - beta) / (1.0f - beta);
        out->y = d_signed + dir * (-W * 0.5f + W * u);
        out->z = g_walk_params.stand_h - H * sinf(3.14159265f * u);
    }
    out->x = g_walk_params.foot_x_shift + g_walk_params.foot_x_corr[leg];   /* x 固定 + 单腿零位修正 */
}
