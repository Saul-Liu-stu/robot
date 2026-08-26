/*
 * attitude_control.c
 * IMU 行为层实现: 站立自稳 (H/K高站姿) + Z 标定
 * 数据源: imu.c 解析的 pitch/roll (WT9011G4K, 10Hz 主动上报)
 * 2026-08-23: 坡度自适应层已删除 (未实测、与自稳职责重叠), Z 改为纯自稳标定
 */
#include "attitude_control.h"

/* 站立自稳参数 (H/K 高站姿专用, 实测调好后写死)
 * 2026-08-26 精度升级: 死区1.0→0.5° / 低通0.6s→0.35s / 新增积分项消重心偏载残差 */
#define LV_LPF     0.25f   /* 低通系数 (10Hz 调用 → 约0.35s时间常数; 原0.15≈0.6s偏慢) */
#define LV_HALF_BL 190.0f  /* 前后髋距半长 mm (与 leg_ik.h BODY_BL=380 一致) */
#define LV_HALF_BW 90.0f   /* 左右髋距半宽 mm (与 leg_ik.h BODY_BW=180 一致) */
#define LV_Z_ROOM_BASE 304.6f  /* 伸腿上限基准: 直腿点足端z(309.6@x=15) − 5mm安全余量 */
#define LV_DEAD     0.7f   /* 死区 (deg): 1.0太粗/0.5偏敏感, 折中0.7 (软死区无阶跃) */
#define LV_DEAD_FAST 0.5f  /* W原地抬腿快速模式死区: 翘板修正要快 */
#define LV_LPF_FAST 0.50f  /* W快速低通 (50ms调用 → 约0.1s时间常数) */
#define LV_KI       0.05f  /* 积分系数 (/tick): 消重心偏载常驻残差, 约2s收敛 */
#define LV_I_MAX_P  3.0f   /* pitch积分抗饱和上限 (deg, ≈10mm) */
#define LV_I_MAX_R  6.0f   /* roll积分抗饱和上限 (deg, ≈9mm) */

#define POSE_NUM    5       /* 姿态槽位: H/K/L/M/P */
#define CALIB_N     400     /* 400 × 5ms = 2 秒标定采样 */

/* 姿态ID: 0=H(0,0高) 1=K(1,0高) 2=L(1,0低) 3=M(0,1低) 4=P(0,1高)
 * 上电默认低趴(0,0低)并入槽0与H共用, 使用前按 Z 标定 */
static uint8_t  s_pose     = 0;
static float    s_offset[POSE_NUM] = {0, 0, 0, 0, 0};    /* pitch 零偏 */
static float    s_roffset[POSE_NUM] = {0, 0, 0, 0, 0};   /* roll 零偏 */
static uint8_t  s_calib_done[POSE_NUM] = {0, 0, 0, 0, 0}; /* 已标定 (自稳允许开启) */

static uint8_t  s_calib      = 0;
static uint32_t s_calib_n    = 0;
static float    s_calib_sum  = 0;
static float    s_calib_rsum = 0;   /* roll 标定和 */

/* 自稳状态 */
static uint8_t  s_lv_en = 0;    /* L:命令开关 */
static float    s_lp_p  = 0;    /* 低通后的相对零偏 pitch (deg, 前倾为正) */
static float    s_lp_r  = 0;    /* 低通后的相对零偏 roll  (deg, 右倾为正) */
static float    s_i_p   = 0;    /* pitch 积分 (deg, 消常驻残差) */
static float    s_i_r   = 0;    /* roll 积分 (deg) */

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

void AttCtrl_CalibTick(float pitch, float roll)
{
    if (!s_calib) return;
    s_calib_sum  += pitch;
    s_calib_rsum += roll;
    if (++s_calib_n >= CALIB_N) {
        /* 标定完成: 零偏存入当前姿态槽, 自稳允许开启 */
        s_offset[s_pose]     = s_calib_sum  / (float)s_calib_n;
        s_roffset[s_pose]    = s_calib_rsum / (float)s_calib_n;
        s_calib_done[s_pose] = 1;
        s_calib = 0;
    }
}

void AttCtrl_CalibStart(void)
{
    s_calib      = 1;
    s_calib_n    = 0;
    s_calib_sum  = 0;
    s_calib_rsum = 0;
}

uint8_t AttCtrl_CalibBusy(void) { return s_calib; }

/* ====== 站立自稳 (H/K 高站姿) ====== */

uint8_t AttCtrl_LevelToggle(void)
{
    if (!s_calib_done[s_pose]) return 2;   /* 当前姿态未标定, 先 Z */
    s_lv_en = !s_lv_en;
    if (!s_lv_en) { s_lp_p = 0.0f; s_lp_r = 0.0f; }
    return s_lv_en ? 0u : 1u;              /* 0=已开 1=已关 */
}

uint8_t AttCtrl_LevelEnabled(void) { return s_lv_en; }

/* L:1/L:0 绝对值开关 (APP状态与固件同步用): 返回 0=开 1=关 2=未标定 */
uint8_t AttCtrl_LevelSet(uint8_t on)
{
    if (on && !s_calib_done[s_pose]) return 2;   /* 当前姿态未标定, 先 Z */
    if (on)  s_lv_en = 1;
    else if (s_lv_en) {
        s_lv_en = 0;
        s_lp_p = 0.0f; s_lp_r = 0.0f;
        s_i_p = 0.0f;  s_i_r  = 0.0f;
    }
    return s_lv_en ? 0u : 1u;
}

void AttCtrl_LevelUpdate(float pitch, float roll, float stand_h, uint8_t active,
                         uint8_t fast, float dz[4])
{
    for (int i = 0; i < 4; i++) dz[i] = 0.0f;
    /* 仅 active(主循环判定) + 已标定 + H/K高站姿 时输出 */
    if (!active || !s_calib_done[s_pose] || s_pose > 1) {
        s_lp_p = 0.0f; s_lp_r = 0.0f;   /* 不输出时清低通, 重新激活从0平滑爬升 */
        s_i_p = 0.0f; s_i_r = 0.0f;     /* 积分同步清零 */
        return;
    }
    /* fast=W原地抬腿: 快速低通+减半死区 (对角支撑翘板修正要快) */
    float alpha = fast ? LV_LPF_FAST : LV_LPF;
    float dead  = fast ? LV_DEAD_FAST : LV_DEAD;

    /* 相对零偏的低通角度 (前倾为正 / 右倾为正) */
    float flat_p = pitch - s_offset[s_pose];
    float flat_r = roll  - s_roffset[s_pose];
    s_lp_p += alpha * (flat_p - s_lp_p);
    s_lp_r += alpha * (flat_r - s_lp_r);

    /* 死区: 微小晃动不响应 (软死区, 无阶跃) */
    float dp = s_lp_p, dr = s_lp_r;
    if (dp >  dead)      dp -= dead;
    else if (dp < -dead) dp += dead;
    else                 dp = 0.0f;
    if (dr >  dead)      dr -= dead;
    else if (dr < -dead) dr += dead;
    else                 dr = 0.0f;

    /* 积分项: 累积死区后残差, 消除重心偏载的常驻倾斜 (纯比例控制的稳态下垂)
     * 仅正常模式; W快速档不加积分 (翘板动态快, 积分易发散) */
    if (!fast) {
        s_i_p += LV_KI * dp;
        s_i_r += LV_KI * dr;
        if (s_i_p >  LV_I_MAX_P) s_i_p =  LV_I_MAX_P;
        if (s_i_p < -LV_I_MAX_P) s_i_p = -LV_I_MAX_P;
        if (s_i_r >  LV_I_MAX_R) s_i_r =  LV_I_MAX_R;
        if (s_i_r < -LV_I_MAX_R) s_i_r = -LV_I_MAX_R;
    }

    /* 往哪边倾哪边腿伸长撑回水平: 前倾→前腿伸/后腿收; 右倾→右腿伸/左腿收
     * 输出 = 比例(死区后角度) + 积分(残差) */
    float pc = (dp + s_i_p) * 0.0174533f * LV_HALF_BL;
    float rc = (dr + s_i_r) * 0.0174533f * LV_HALF_BW;
    dz[0] =  pc - rc;    /* FL */
    dz[1] =  pc + rc;    /* FR */
    dz[2] = -pc - rc;    /* RL */
    dz[3] = -pc + rc;    /* RR */
    /* 单腿z上限: 按当前站高算剩余伸腿量 (站高越低放得越开)
     * 280≈24.6mm(7.5°坡) / 230≈74.6mm(23°坡) / 220≈84.6mm(26.5°坡) / 210≈94.6mm(30°坡) */
    float dz_lim = LV_Z_ROOM_BASE - stand_h;
    if (dz_lim < 0.0f) dz_lim = 0.0f;
    for (int i = 0; i < 4; i++) {
        if (dz[i] >  dz_lim) dz[i] =  dz_lim;
        if (dz[i] < -dz_lim) dz[i] = -dz_lim;
    }
}
