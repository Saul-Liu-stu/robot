/*
 * leg_ik.c — 3DOF 腿逆运动学实现
 * 数据: 用户实测 (2DIK测量清单 + 舵机标定), 侧摆 dir 以用户 +10° 实测为准
 */
#include "leg_ik.h"
#include <math.h>

#define RAD2DEG  (57.2957795f)
#define DEG2RAD  ( 0.0174533f)

/* 髋偏置符号: 左腿(FL/RL)为负, 右腿(FR/RR)为正 */
static const float s_hip_d[4] = { -LEG_HIP_D, LEG_HIP_D, -LEG_HIP_D, LEG_HIP_D };

/* 髋侧摆轴心在机身系中的 (x, y) */
static const float s_hip_x[4] = {  BODY_BL/2,  BODY_BL/2, -BODY_BL/2, -BODY_BL/2 };
static const float s_hip_y[4] = { -BODY_BW/2,  BODY_BW/2, -BODY_BW/2,  BODY_BW/2 };

/*
 * 舵机标定表 [腿][关节] (实测, 2026-08 复核)
 *   侧摆 q1 零点 = 腿竖直时舵机角; dir: +10° 实测 (右腿以用户实测为准)
 *   前摆 q2 零点 = 大腿竖直; 膝 q3 零点 = 大小腿成直线
 */
joint_calib_t g_leg_calib[4][3] = {
    /*         { q1侧摆,     q2前摆,     q3膝 } (zero, dir) */
    /* FL */ { { 95.0f,  1.0f}, {115.0f, -1.0f}, { 93.0f,  1.0f} },
    /* FR */ { { 94.0f, -1.0f}, {150.0f,  1.0f}, {135.0f, -1.0f} },
    /* RL */ { { 50.0f, -1.0f}, {110.0f, -1.0f}, {165.0f,  1.0f} },
    /* RR */ { {150.0f,  1.0f}, {230.0f,  1.0f}, {185.0f, -1.0f} },
};

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* 前腿膝分支: 0=正常(狗姿态) 1=反折(膝盖往前顶), 由 main.c 姿态切换控制 */
static uint8_t s_front_rev = 0;

void LegIK_SetFrontReversed(uint8_t rev)
{
    s_front_rev = rev ? 1 : 0;
}

ik_status_t LegIK_Solve(uint8_t leg, float x, float y, float z, leg_joint_deg_t *out)
{
    ik_status_t st = IK_OK;
    float d  = s_hip_d[leg & 3];
    float L1 = LEG_L1, L2 = LEG_L2;

    /* --- 解 q1: y-z 平面几何 --- */
    float yz2 = y * y + z * z;
    if (yz2 < d * d) {              /* 目标落进髋偏置圆柱内, 物理不可达 */
        yz2 = d * d;
        st = IK_UNREACHABLE;
    }
    float zf = sqrtf(yz2 - d * d);

    float den = d * d + zf * zf;
    float q1 = atan2f((d * z - zf * y) / den, (d * y + zf * z) / den);

    /* --- 平面二连杆解 q2, q3 --- */
    float xf = x;
    float Dmax = (L1 + L2) * 0.9999f;
    float Dmin = fabsf(L1 - L2) * 1.0001f + 0.001f;
    float D2 = xf * xf + zf * zf;
    if (D2 > Dmax * Dmax || D2 < Dmin * Dmin) st = IK_UNREACHABLE;
    float D = clampf(sqrtf(D2), Dmin, Dmax);

    float scale = D / sqrtf(D2);    /* 目标等比例缩放到可达距离, 保证输出连续 */
    xf *= scale;
    zf *= scale;

    float cq3 = (xf * xf + zf * zf - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
    float q3 = acosf(clampf(cq3, -1.0f, 1.0f));
    if (s_front_rev && leg < 2) q3 = -q3;   /* 前腿反折分支 */
    /* q2 公式对两个分支都成立 (atan2 自动带出符号) */
    float q2 = atan2f(xf, zf) - atan2f(L2 * sinf(q3), L1 + L2 * cosf(q3));

    /* --- 转角度 + 限位钳制 --- */
    float q1d = q1 * RAD2DEG, q2d = q2 * RAD2DEG, q3d = q3 * RAD2DEG;
    out->q1 = clampf(q1d, Q1_MIN_DEG, Q1_MAX_DEG);
    out->q2 = clampf(q2d, Q2_MIN_DEG, Q2_MAX_DEG);
    out->q3 = (s_front_rev && leg < 2)
            ? clampf(q3d, -Q3_MAX_DEG, Q3_MIN_DEG)   /* 反折: 膝 -140~0 */
            : clampf(q3d,  Q3_MIN_DEG, Q3_MAX_DEG);
    if (st == IK_OK && (out->q1 != q1d || out->q2 != q2d || out->q3 != q3d))
        st = IK_OUT_OF_LIMIT;
    return st;
}

ik_status_t LegIK_SolveServo(uint8_t leg, float x, float y, float z, uint16_t out_deg[3])
{
    leg_joint_deg_t j;
    ik_status_t st = LegIK_Solve(leg, x, y, z, &j);
    const joint_calib_t *c = g_leg_calib[leg & 3];

    float s1 = c[0].zero_deg + c[0].dir * j.q1;
    float s2 = c[1].zero_deg + c[1].dir * j.q2;
    float s3 = c[2].zero_deg + c[2].dir * j.q3;

    out_deg[0] = (uint16_t)clampf(s1, 0.0f, 270.0f);
    out_deg[1] = (uint16_t)clampf(s2, 0.0f, 270.0f);
    out_deg[2] = (uint16_t)clampf(s3, 0.0f, 270.0f);
    return st;
}

void LegIK_FK(uint8_t leg, const leg_joint_deg_t *j, float *x, float *y, float *z)
{
    float d  = s_hip_d[leg & 3];
    float q2 = j->q2 * DEG2RAD, q3 = j->q3 * DEG2RAD, q1 = j->q1 * DEG2RAD;
    float xf = LEG_L1 * sinf(q2) + LEG_L2 * sinf(q2 + q3);
    float zf = LEG_L1 * cosf(q2) + LEG_L2 * cosf(q2 + q3);
    float c1 = cosf(q1), s1 = sinf(q1);
    *x = xf;
    *y = d * c1 - zf * s1;
    *z = d * s1 + zf * c1;
}

void LegIK_GetHipPos(uint8_t leg, float *hx, float *hy)
{
    *hx = s_hip_x[leg & 3];
    *hy = s_hip_y[leg & 3];
}
