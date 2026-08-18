/*
 * attitude_control.h
 * IMU 坡度自适应层 (仅上坡, 仅轮式滚动模式)
 *
 * 目标: 轮式爬坡时身体保持与坡面平行 (防后腿下陷过多导致的抬头后仰)。
 * 原理: pitch 长期均值 (低通约2.5s) 判断上坡 → 足端整体后移 (身体前压)
 *       → 前后腿载荷重新均衡 → 下陷一致 → 身体回到与坡面平行。
 *
 * 符号约定 (实测): 前倾 = pitch 正 → 上坡 = pitch 负。
 * 不参与镇定 (镇定层暂缓), 不写下坡分支。
 */
#ifndef INC_ATTITUDE_CONTROL_H_
#define INC_ATTITUDE_CONTROL_H_

#include <stdint.h>

/* 坡度层参数 (Q: 命令在线调, 调好后写死) */
typedef struct {
    float lpf_alpha;    /* 坡度低通系数 (每5ms): 0.002 ≈ 2.5s 时间常数 */
    float up_thr;       /* 上坡判定阈值 (deg) */
    float hyst;         /* 退出迟滞 (deg): 回落到 up_thr-hyst 才退出上坡 */
    float gain;         /* 足端后移力度 (mm/deg 坡度) */
    float shift_max;    /* 足端后移上限 (mm) */
} att_ctrl_params_t;

extern att_ctrl_params_t g_att;

/* 每5ms调用: 输出足端修正 (负值=后移, mm) 与坡度状态 (0平/1上坡) */
void AttCtrl_Update(float pitch, float *out_shift, int *out_state);

/* 每5ms调用: 标定状态机 (进行中时采样2秒均值存为当前姿态零偏) */
void AttCtrl_CalibTick(float pitch);

/* Z命令: 未开启→开始标定(完成后自动开启); 已开启→关闭 */
void AttCtrl_Toggle(void);

/* 标定是否进行中 */
uint8_t AttCtrl_CalibBusy(void);

/* 当前姿态功能是否开启 */
uint8_t AttCtrl_Enabled(void);

/* 每5ms调用: 设置当前姿态ID (按前后腿膝分支+站高区分, 零偏按姿态分存) */
void AttCtrl_SetPose(uint8_t front_rev, uint8_t rear_rev, uint8_t high);

#endif /* INC_ATTITUDE_CONTROL_H_ */
