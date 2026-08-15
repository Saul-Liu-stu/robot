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
    .stand_h  = 240.0f,   /* 站高 240mm (实测狗形站姿) */
};

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
    out->y = d_signed;   /* 足端保持在腿平面内 */
}
