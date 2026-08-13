/*
 * ik2d.c — 二维矢状面 IK 实现
 * 数据来源: docs/硬件/2DIK测量清单.md (实测)
 */
#include "ik2d.h"
#include <math.h>

#define RAD2DEG (57.29578f)
#define DEG2RAD (0.0174533f)

/* {L1, L2, 大腿零点, 小腿零点, 大腿dir, 小腿dir} */
const LegGeom2D g_leg_geom[4] = {
    { 130.0f, 180.0f, 212.0f,  93.0f, -1.0f,  1.0f },  /* FL */
    { 130.0f, 180.0f,  65.0f, 135.0f,  1.0f, -1.0f },  /* FR */
    { 130.0f, 180.0f, 110.0f, 165.0f, -1.0f,  1.0f },  /* RL */
    { 130.0f, 180.0f, 230.0f,  90.0f,  1.0f, -1.0f },  /* RR */
};

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

int IK2D_Solve(uint8_t leg, float x, float z,
               uint16_t *th_servo, uint16_t *ca_servo)
{
    const LegGeom2D *g = &g_leg_geom[leg & 3];
    float L1 = g->l1, L2 = g->l2;

    /* 目标距离超腿长 → 等比例缩回 */
    float Dmax = (L1 + L2) * 0.999f;
    float D = sqrtf(x * x + z * z);
    if (D > Dmax) {
        float scale = Dmax / D;
        x *= scale;
        z *= scale;
        D = Dmax;
    }

    /* 2 连杆逆解 */
    float cq3 = (D * D - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
    cq3 = clampf(cq3, -1.0f, 1.0f);
    float q3 = acosf(cq3);
    float q2 = atan2f(x, z) - atan2f(L2 * sinf(q3), L1 + L2 * cosf(q3));

    /* 舵机换算 + 限幅 */
    float s_th = g->th_zero + g->th_dir * q2 * RAD2DEG;
    float s_ca = g->ca_zero + g->ca_dir * q3 * RAD2DEG;
    *th_servo = (uint16_t)clampf(s_th, 0.0f, 270.0f);
    *ca_servo = (uint16_t)clampf(s_ca, 0.0f, 270.0f);
    return 0;
}

void IK2D_FK(uint8_t leg, uint16_t th_servo, uint16_t ca_servo,
             float *x, float *z)
{
    const LegGeom2D *g = &g_leg_geom[leg & 3];
    float q2 = (th_servo - g->th_zero) / g->th_dir * DEG2RAD;
    float q3 = (ca_servo - g->ca_zero) / g->ca_dir * DEG2RAD;
    *x = g->l1 * sinf(q2) + g->l2 * sinf(q2 + q3);
    *z = g->l1 * cosf(q2) + g->l2 * cosf(q2 + q3);
}
