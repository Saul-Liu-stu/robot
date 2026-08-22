/*
 * drive_ctrl.c
 * 摇杆驱动 + 倾斜平衡实现
 *
 * 速度换算: 命令 ±50 = 占空比 ±50% (1:1 映射) → 空载输出转速 ≈ ±180 RPM
 *          (实测: 50%占空比≈180RPM), 即目标转速 = 3.6 × 速度值。
 *          负载下实际转速低于空载, 故"到速"用
 *          平台期判定 (转速300ms窗口增量 < 2 RPM = 加速过程结束) 而非死等目标值。
 */
#include "drive_ctrl.h"
#include "motor_control.h"
#include "encoder.h"

extern volatile uint32_t uwTick;

drive_params_t g_drive = {
    .slew      = 0.5f,    /* 占空比斜坡 %/5ms, 满量程50%用0.5s (保持原设计手感) */
    .k_pitch   = 0.3f,    /* 满加速差50% → 15mm 低头 */
    .k_roll    = 0.2f,    /* 转向满50 → 10mm 压弯 */
    .pitch_max = 15.0f,   /* ≈4.5° */
    .roll_max  = 12.0f,   /* 压弯上限 (D按键用10mm) */
    .tilt_slew = 0.25f,   /* 15mm 约300ms过渡, 别太猛 */
};

static int8_t  s_cmd_sp = 0;   /* 命令速度 -50~+50 */
static int8_t  s_cmd_st = 0;   /* 命令转向 -50~+50 */
static float   s_duty   = 0;   /* 斜坡后的实际速度占空比 -100~+100 */
static float   s_tilt_p = 0;   /* 当前俯仰倾斜 (mm, +低头) */
static float   s_tilt_r = 0;   /* 当前侧倾倾斜 (mm, +右倾) */
static uint8_t s_active = 0;   /* 驱动激活 (写电机) */

/* 方向按键状态: 固定倾斜全程保持, 停止才回平 */
static uint8_t s_btn_mode = 0;
static uint8_t s_btn_dir  = 0;
static uint8_t s_flatten_pending = 0;   /* 切换动作: 先回平再压新方向 */

/* 方向按键固定倾斜表 (mm): {pitch(+低头), roll(+右倾/右低)} */
static const float BTN_TILT[4][2] = {
    { 10.0f,  0.0f},   /* 0 前进: 低头 10mm */
    {-10.0f,  0.0f},   /* 1 后退: 抬头 10mm */
    {  6.0f, -10.0f},  /* 2 左前: 低头 6mm + 左倾 10mm (左边低) */
    {  6.0f,  10.0f},  /* 3 右前: 低头 6mm + 右倾 10mm (右边低) */
};

/* 编码器平台期检测: 100ms 采样, 3 点历史 (300ms 窗口) */
static uint32_t s_rpm_tick = 0;
static float    s_rpm_hist[3] = {0, 0, 0};
static uint8_t  s_rpm_hist_i  = 0;
static uint8_t  s_plateau     = 0;
static float    s_rpm_last[4] = {0, 0, 0, 0};   /* 最近采样缓存 (J 上报用) */

void DriveCtrl_SetCmd(int8_t speed, int8_t steer)
{
    if (speed > 50)  speed = 50;
    if (speed < -50) speed = -50;
    if (steer > 50)  steer = 50;
    if (steer < -50) steer = -50;
    s_cmd_sp = speed;
    s_cmd_st = steer;
    s_btn_mode = 0;   /* 摇杆接管 */
    if (speed != 0 || steer != 0) s_active = 1;
    /* 新加速阶段: 清平台期历史 */
    s_rpm_hist[0] = s_rpm_hist[1] = s_rpm_hist[2] = 0;
    s_plateau = 0;
}

void DriveCtrl_SetButton(uint8_t dir, int8_t spd)
{
    if (dir > 3) dir = 0;
    if (spd > 50) spd = 50;
    if (spd < 10) spd = 10;
    /* 切换动作(换方向或从摇杆切来): 先回平再压新方向 */
    if (!s_btn_mode || s_btn_dir != dir)
        s_flatten_pending = 1;
    s_btn_mode = 1;
    s_btn_dir  = dir;
    if (dir == 0) {                       /* 前进 */
        s_cmd_sp = spd; s_cmd_st = 0;
    } else if (dir == 1) {                /* 后退 */
        s_cmd_sp = (int8_t)-spd; s_cmd_st = 0;
    } else {                              /* 左前/右前: 前向3/4 + 转向0.6, 弧线明显 */
        s_cmd_sp = (int8_t)(spd * 3 / 4);
        s_cmd_st = (dir == 2) ? (int8_t)(-spd * 3 / 5) : (int8_t)(spd * 3 / 5);
    }
    s_active = 1;
    s_plateau = 0;
    s_rpm_hist[0] = s_rpm_hist[1] = s_rpm_hist[2] = 0;
}

void DriveCtrl_Reset(void)
{
    s_cmd_sp = 0;
    s_cmd_st = 0;
    s_duty   = 0;
    s_active = 0;
    s_tilt_p = 0;
    s_tilt_r = 0;
    s_plateau = 0;
    s_btn_mode = 0;
    s_flatten_pending = 0;
    for (int i = 0; i < 4; i++)
        Motor_Stop((uint8_t)i);
}

static float clampf2(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void DriveCtrl_Update(void)
{
    /* --- 占空比斜坡: 以固定斜率逼近命令 (起步/刹车都是渐变的) --- */
    float target = (float)s_cmd_sp;   /* 1:1 映射: V:50 = 50% 占空比 */
    if (s_duty < target) {
        s_duty += g_drive.slew;
        if (s_duty > target) s_duty = target;
    } else if (s_duty > target) {
        s_duty -= g_drive.slew;
        if (s_duty < target) s_duty = target;
    }

    /* --- 编码器采样: 每100ms取四轮平均 |RPM|, 300ms窗口判平台期 --- */
    if ((uint32_t)(uwTick - s_rpm_tick) >= 100u) {
        s_rpm_tick = uwTick;
        float rpm_abs = 0;
        for (int i = 0; i < 4; i++) {
            float r = Encoder_GetRPM((uint8_t)i);
            if (r < 0) r = -r;
            s_rpm_last[i] = r;
            rpm_abs += r;
        }
        rpm_abs /= 4.0f;
        s_rpm_hist[s_rpm_hist_i] = rpm_abs;
        s_rpm_hist_i = (s_rpm_hist_i + 1) % 3;
        float rpm_old = s_rpm_hist[s_rpm_hist_i];   /* 300ms前 */
        float rpm_max = s_rpm_hist[0];
        if (s_rpm_hist[1] > rpm_max) rpm_max = s_rpm_hist[1];
        if (s_rpm_hist[2] > rpm_max) rpm_max = s_rpm_hist[2];
        uint8_t ramping = (s_duty < target - 1.0f) || (s_duty > target + 1.0f);
        /* 平台期: 仍在斜坡中但转速不再上升(负载限速) → 提前回平
         * 编码器无信号时 rpm_max=0 永远不触发, 退化为斜坡完成自动回平 */
        s_plateau = ramping && (rpm_max > 10.0f) && (rpm_abs - rpm_old < 2.0f);
    }

    /* --- 倾斜目标 --- */
    float tp, tr;
    if (s_btn_mode) {
        if (s_flatten_pending) {
            /* 切换动作: 先回平, 到位后再压新方向 */
            tp = 0.0f;
            tr = 0.0f;
            if (s_tilt_p > -0.5f && s_tilt_p < 0.5f &&
                s_tilt_r > -0.5f && s_tilt_r < 0.5f)
                s_flatten_pending = 0;
        } else {
            /* 方向按键: 固定倾斜全程保持, 只有停止(S)才回平 */
            tp = BTN_TILT[s_btn_dir][0];
            tr = BTN_TILT[s_btn_dir][1];
        }
    } else {
        tp = s_plateau ? 0.0f
             : g_drive.k_pitch * ((float)s_cmd_sp - s_duty);   /* +低头 */
        tr = g_drive.k_roll * (float)s_cmd_st;                  /* +右倾 */
    }
    tp = clampf2(tp, -g_drive.pitch_max, g_drive.pitch_max);
    tr = clampf2(tr, -g_drive.roll_max,  g_drive.roll_max);
    /* 倾斜过渡斜坡 (别太猛) */
    s_tilt_p += clampf2(tp - s_tilt_p, -g_drive.tilt_slew, g_drive.tilt_slew);
    s_tilt_r += clampf2(tr - s_tilt_r, -g_drive.tilt_slew, g_drive.tilt_slew);

    /* --- 电机差速输出 --- */
    if (s_active) {
        float left  = s_duty + (float)s_cmd_st;   /* 1:1: 转向±50 = 差速±50% */
        float right = s_duty - (float)s_cmd_st;
        int l = (int)left;   if (l > 100) l = 100;  if (l < -100) l = -100;
        int r = (int)right;  if (r > 100) r = 100;  if (r < -100) r = -100;
        /* 左轮 A/C, 右轮 B/D (实机方向反了交换即可) */
        Motor_Set(MOTOR_A, (uint8_t)(l < 0 ? -l : l), l >= 0 ? 1 : 0);
        Motor_Set(MOTOR_C, (uint8_t)(l < 0 ? -l : l), l >= 0 ? 1 : 0);
        Motor_Set(MOTOR_B, (uint8_t)(r < 0 ? -r : r), r >= 0 ? 1 : 0);
        Motor_Set(MOTOR_D, (uint8_t)(r < 0 ? -r : r), r >= 0 ? 1 : 0);
        /* V:0:0 斜坡归零后释放电机控制权 */
        if (s_cmd_sp == 0 && s_cmd_st == 0 && s_duty == 0.0f)
            s_active = 0;
    }
}

void DriveCtrl_GetTilt(float *pitch_mm, float *roll_mm)
{
    *pitch_mm = s_tilt_p;
    *roll_mm  = s_tilt_r;
}

uint8_t DriveCtrl_Active(void) { return s_active; }

float DriveCtrl_GetRPM(uint8_t idx)
{
    return (idx < 4) ? s_rpm_last[idx] : 0.0f;
}
