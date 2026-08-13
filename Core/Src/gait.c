/*
 * gait.c
 * 四足步态引擎 — 足端轨迹 + 二维 IK
 *
 * 原理:
 *   每条腿的 local_phase 决定脚在腿系的目标位置 (x前, z下, mm):
 *     摆动相 (0~179°):  脚从后往前扫, 中途抬起 STEP_LIFT_MM
 *     支撑相 (180~359°): 脚贴地 (z=HIP_H_MM) 从前往后推
 *   脚目标 → IK2D_Solve → 舵机角 → Leg_SetJoint
 *
 * 步态 = 四条腿的相位偏移组合 (Trot: 0/180/180/0 对角同步)
 */
#include "gait.h"
#include "leg_control.h"
#include "ik2d.h"
#include <math.h>

#define PI 3.14159265f

/* 各步态的腿相位偏移(°) */
static const uint16_t gait_offsets[GAIT_NUM][4] = {
    /* STAND */ {   0,   0,   0,   0 },
    /* TROT  */ {   0, 180, 180,   0 },
    /* WALK  */ {   0,  90, 180, 270 },
    /* CRAWL */ {   0, 120, 240,  60 },
};

static GaitType  g_gait_type = GAIT_STAND;
static uint16_t  g_phase     = 0;          /* 全局相位 0~359 */
static LegPhase  g_leg_phase[4];
static uint32_t  g_last_ms   = 0;
static uint32_t  g_ramp_start = 0;         /* 过渡斜坡起始时刻 */
static uint8_t   g_debug_pause = 0;        /* 单步调试: 1=停住不走 */
static uint16_t  g_dbg_th[4] = {0};        /* 单步插值起点 (实际舵机角) */
static uint16_t  g_dbg_ca[4] = {0};
static uint8_t   g_dbg_sync  = 1;          /* 1=下次单步前重新同步站姿 */
#define GAIT_RAMP_MS  800                 /* 斜坡时长 800ms (带符号!) */

/* ========== 足端轨迹 (腿系, mm) ========== */

/* 某腿相位的脚目标: x 前为正, z 下为正 (从髋算起) */
static void foot_target(uint16_t local_phase, float *x, float *z)
{
    float S = STEP_LEN_MM, L = STEP_LIFT_MM;

    if (local_phase < 180) {
        /* 摆动相: 脚从后(-S/2)往前(+S/2), 中途抬 L 高 */
        float p = local_phase;
        *x = -S / 2.0f + S * p / 180.0f;
        *z = HIP_H_MM - L * sinf(PI * p / 180.0f);
    } else {
        /* 支撑相: 脚贴地(z=髋高), 从前往后推身体 */
        float p = local_phase - 180.0f;
        *x = S / 2.0f - S * p / 180.0f;
        *z = HIP_H_MM;
    }
}

/* 计算某腿当前舵机角 (足轨迹 + IK) */
static void solve_leg(uint8_t leg, uint16_t local_phase,
                      uint16_t *th, uint16_t *ca)
{
    float x, z;
    foot_target(local_phase, &x, &z);
    IK2D_Solve(leg, x, z, th, ca);
}

/* ========== 公开函数 ========== */

void Gait_Init(void)
{
    g_gait_type = GAIT_STAND;
    g_phase = 0;
    g_last_ms = uwTick;
    for (int i = 0; i < 4; i++) {
        g_leg_phase[i].phase    = 0;
        g_leg_phase[i].is_swing = 0;
    }
}

void Gait_SetType(GaitType type)
{
    if (type >= GAIT_NUM) return;
    if (g_gait_type == GAIT_STAND && type != GAIT_STAND) {
        g_ramp_start = uwTick;   /* 从站立切步态 → 启动过渡斜坡 */
    }
    g_debug_pause = 0;           /* 任何显式切步态都解除调试暂停 */
    g_gait_type = type;
    g_last_ms = uwTick;          /* 重置时间基准, 防恢复瞬间相位猛跳 */
}

void Gait_Update(void)
{
    uint32_t now = uwTick;

    /* STAND 模式不写舵机 */
    if (g_gait_type == GAIT_STAND) return;
    /* 单步调试暂停 */
    if (g_debug_pause) return;

    /* 步态周期内相位推进 */
    {
        uint32_t dt = now - g_last_ms;
        uint32_t advance = (uint32_t)GAIT_CYCLE_MS / 360;  /* 每度耗时(ms) */

        if (advance > 0 && dt >= advance) {
            g_phase = (g_phase + (uint16_t)(dt / advance)) % 360;
        }
        g_last_ms = now;
    }

    /* 站立→步态过渡斜坡: 足轨迹幅度从 0 渐入 */
    float ramp = 1.0f;
    if (g_ramp_start) {
        uint32_t elapsed = now - g_ramp_start;
        if (elapsed < GAIT_RAMP_MS) {
            ramp = (float)elapsed / (float)GAIT_RAMP_MS;
        } else {
            g_ramp_start = 0;
        }
    }

    /* 逐腿: 足轨迹 → IK → 写舵机 */
    for (int leg = 0; leg < 4; leg++) {
        uint16_t local_phase = (g_phase + gait_offsets[g_gait_type][leg]) % 360;
        g_leg_phase[leg].phase    = local_phase;
        g_leg_phase[leg].is_swing = (local_phase < 180) ? 1 : 0;

        float x, z;
        foot_target(local_phase, &x, &z);
        /* 斜坡: x 幅度缩放, z 从髋高(贴地)渐入抬腿 */
        x *= ramp;
        z  = HIP_H_MM + (z - HIP_H_MM) * ramp;

        uint16_t th, ca;
        IK2D_Solve((uint8_t)leg, x, z, &th, &ca);
        Leg_SetJoint((uint8_t)leg, JOINT_THIGH, th);
        Leg_SetJoint((uint8_t)leg, JOINT_CALF,  ca);
    }
}

const LegPhase* Gait_GetPhase(uint8_t leg)
{
    return (leg < 4) ? &g_leg_phase[leg] : NULL;
}

/* 单步插值起点重新同步到站姿 (G 回站姿后调用) */
void Gait_DebugReset(void)
{
    g_dbg_sync = 1;
    g_phase    = 0;   /* 相位归零, 第一个A从动作1开始 */
    g_debug_pause = 0;
    g_gait_type = GAIT_STAND;
    g_ramp_start = 0;
}

/* 调试: 平滑推进 phase°, 500ms 内从当前姿态过渡到目标姿态 */
void Gait_DebugStep(uint16_t deg)
{
    /* 1. 插值起点 = 实际舵机角 (每轮调试开始前同步一次站姿) */
    if (g_dbg_sync) {
        for (int leg = 0; leg < 4; leg++) {
            IK2D_Solve((uint8_t)leg, 0.0f, HIP_H_MM,
                       &g_dbg_th[leg], &g_dbg_ca[leg]);
        }
        g_dbg_sync = 0;
    }

    /* 2. 推进相位, 计算目标舵机角 (足轨迹 + IK) */
    g_phase = (g_phase + deg) % 360;
    g_ramp_start = 0;

    uint16_t tgt_th[4], tgt_ca[4];
    for (int leg = 0; leg < 4; leg++) {
        uint16_t lp = (g_phase + gait_offsets[g_gait_type][leg]) % 360;
        solve_leg((uint8_t)leg, lp, &tgt_th[leg], &tgt_ca[leg]);
    }

    /* 3. 插值过渡 (全程 int32_t 有符号运算) */
    for (int32_t s = 1; s <= 20; s++) {
        for (int leg = 0; leg < 4; leg++) {
            int32_t d_th = (int32_t)tgt_th[leg] - (int32_t)g_dbg_th[leg];
            int32_t d_ca = (int32_t)tgt_ca[leg] - (int32_t)g_dbg_ca[leg];
            uint16_t th = (uint16_t)((int32_t)g_dbg_th[leg] + d_th * s / 20);
            uint16_t ca = (uint16_t)((int32_t)g_dbg_ca[leg] + d_ca * s / 20);
            Leg_SetJoint((uint8_t)leg, JOINT_THIGH, th);
            Leg_SetJoint((uint8_t)leg, JOINT_CALF,  ca);
        }
        HAL_Delay(25);
    }

    /* 4. 更新实际位置 + 停住 */
    for (int leg = 0; leg < 4; leg++) {
        g_dbg_th[leg] = tgt_th[leg];
        g_dbg_ca[leg] = tgt_ca[leg];
    }
    g_debug_pause = 1;   /* 主循环 Gait_Update 暂停, 停在当前姿态 */
}
