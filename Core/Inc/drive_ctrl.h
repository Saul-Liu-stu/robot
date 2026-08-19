/*
 * drive_ctrl.h
 * 摇杆驱动 (V 命令) + 起停/转弯倾斜平衡
 *
 * 功能:
 *   1. V:速度:转向 差速驱动, 占空比以固定斜率逼近命令 (起步温和)
 *   2. 俯仰倾斜补偿: 倾斜 ∝ (命令-实际占空比), 加速=低头/减速=抬头,
 *      编码器转速平台期(到速)提前回平
 *   3. 侧倾倾斜补偿: 倾斜 ∝ 转向量, 转向期间持续压弯, 回零回平
 *   4. 编码器转速采样缓存 (J 命令上报用)
 *
 * 倾斜实现: 前后/左右足端 z 差动 (几何倾斜), 上限 pitch 15mm / roll 10mm。
 */
#ifndef INC_DRIVE_CTRL_H_
#define INC_DRIVE_CTRL_H_

#include <stdint.h>

typedef struct {
    float slew;        /* 占空比斜坡 (%/5ms), 1.0 = 0.5s 从0到满 */
    float k_pitch;     /* 俯仰倾斜增益 (mm / 占空比差%) */
    float k_roll;      /* 侧倾倾斜增益 (mm / 转向单位) */
    float pitch_max;   /* 俯仰倾斜上限 (mm) */
    float roll_max;    /* 侧倾倾斜上限 (mm) */
    float tilt_slew;   /* 倾斜变化速率 (mm/5ms) */
} drive_params_t;

extern drive_params_t g_drive;

/* V:sp:st 命令入口 (sp/st: -50~+50) */
void DriveCtrl_SetCmd(int8_t speed, int8_t steer);

/* D 方向按键入口: dir 0=前进 1=后退 2=左前 3=右前, spd 10~50 (占空比%)
 * 固定倾斜全程保持, 只有停止(S)才回平 (与 V 的到速回平行为不同) */
void DriveCtrl_SetButton(uint8_t dir, int8_t spd);

/* 急停复位 (R/B/S/T/W/E/F 等接管电机时调用): 立即停轮+回平 */
void DriveCtrl_Reset(void);

/* 每5ms调用: 占空比斜坡 + 电机差速输出 + 倾斜状态机 + 编码器平台期检测 */
void DriveCtrl_Update(void);

/* STAND 分支取当前倾斜 (mm): pitch>0=低头, roll>0=右倾 */
void DriveCtrl_GetTilt(float *pitch_mm, float *roll_mm);

/* 驱动是否激活 (非零命令或斜坡未归零) */
uint8_t DriveCtrl_Active(void);

/* 最近一次编码器采样转速 (RPM, idx 0~3 = 电机A~D), J 上报用 */
float DriveCtrl_GetRPM(uint8_t idx);

#endif /* INC_DRIVE_CTRL_H_ */
