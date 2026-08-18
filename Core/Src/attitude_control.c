/*
 * attitude_control.c
 * IMU 坡度自适应层实现 (仅上坡, 仅轮式滚动模式, 镇定层暂缓)
 * 数据源: imu.c 解析的 pitch (WT9011G4K, 10Hz 主动上报)
 */
#include "attitude_control.h"

att_ctrl_params_t g_att = {
    .lpf_alpha = 0.002f,   /* 约 2.5s 时间常数 */
    .up_thr    = 5.0f,     /* 上坡判定阈值 (deg) */
    .hyst      = 2.0f,     /* 退出迟滞 (deg) */
    .gain      = 3.5f,     /* 每度坡度后移 3.5mm (≈站高×tan) */
    .shift_max = 40.0f,    /* 后移上限 */
};

#define POSE_NUM    5       /* 姿态槽位: H/K/L/M/P */
#define CALIB_N     400     /* 400 × 5ms = 2 秒标定采样 */

/* 姿态ID: 0=H(0,0高) 1=K(1,0高) 2=L(1,0低) 3=M(0,1低) 4=P(0,1高)
 * 上电默认低趴(0,0低)并入槽0与H共用, 使用前按 Z 标定 */
static uint8_t  s_pose     = 0;
static float    s_offset[POSE_NUM] = {0, 0, 0, 0, 0};
static uint8_t  s_enabled[POSE_NUM] = {0, 0, 0, 0, 0};

static uint8_t  s_calib      = 0;
static uint32_t s_calib_n    = 0;
static float    s_calib_sum  = 0;

static float    s_slope = 0;    /* 低通后的相对零偏 pitch (正=抬头/上坡方向为负) */
static uint8_t  s_state = 0;    /* 0=平 1=上坡 */

void AttCtrl_SetPose(uint8_t front_rev, uint8_t rear_rev, uint8_t high)
{
    if (high) {
        if (front_rev && !rear_rev)      s_pose = 1;   /* K */
        else if (rear_rev && !front_rev) s_pose = 4;   /* P */
        else                             s_pose = 0;   /* H */
    } else {
        if (front_rev && !rear_rev)      s_pose = 2;   /* L */
        else if (rear_rev && !front_rev) s_pose = 3;   /* M */
        else                             s_pose = 0;   /* 上电默认低趴并入H槽 */
    }
}

void AttCtrl_CalibTick(float pitch)
{
    if (!s_calib) return;
    s_calib_sum += pitch;
    if (++s_calib_n >= CALIB_N) {
        /* 标定完成: 零偏存入当前姿态槽, 自动开启功能 */
        s_offset[s_pose]  = s_calib_sum / (float)s_calib_n;
        s_enabled[s_pose] = 1;
        s_slope = 0;
        s_state = 0;
        s_calib = 0;
    }
}

void AttCtrl_Toggle(void)
{
    if (s_enabled[s_pose]) {
        s_enabled[s_pose] = 0;
        s_slope = 0;
        s_state = 0;
    } else {
        s_calib     = 1;
        s_calib_n   = 0;
        s_calib_sum = 0;
    }
}

uint8_t AttCtrl_CalibBusy(void) { return s_calib; }
uint8_t AttCtrl_Enabled(void)   { return s_enabled[s_pose]; }

void AttCtrl_Update(float pitch, float *out_shift, int *out_state)
{
    *out_shift = 0;
    *out_state = 0;
    if (!s_enabled[s_pose] || s_calib) return;

    /* 相对零偏的 pitch 低通 (前倾为正) */
    float flat = pitch - s_offset[s_pose];
    s_slope += g_att.lpf_alpha * (flat - s_slope);

    /* 上坡判定: 抬头(后仰)为负 → 上坡角 = -s_slope, 带迟滞 */
    float up_deg = -s_slope;
    if (s_state == 0) {
        if (up_deg > g_att.up_thr) s_state = 1;
    } else {
        if (up_deg < g_att.up_thr - g_att.hyst) s_state = 0;
    }

    *out_state = s_state;
    if (s_state) {
        /* 足端整体后移: 身体前压 → 前后腿载荷均衡 → 身体与坡面平行 */
        float sh = -g_att.gain * up_deg;
        if (sh < -g_att.shift_max) sh = -g_att.shift_max;
        *out_shift = sh;
    }
}
