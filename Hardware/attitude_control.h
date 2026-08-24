/*
 * attitude_control.h
 * IMU 行为层: 站立自稳 (H/K高站姿) + Z 标定
 *
 * 自稳: roll/pitch 相对 Z 标定零偏的偏差 → 四腿 z 差动补偿 (往哪边倾哪边腿伸长)
 *       → 不平地面站立/无倾斜驱动时身体保持水平。仅 H/K 高站姿 (P 后顶站 RR 大腿余量 7° 不启用)。
 * 2026-08-23: 坡度自适应层已删除, Z 改为纯自稳标定。
 *
 * 符号约定 (实测): 前倾 = pitch 正; 自稳按右倾 = roll 正 (待实测定向)。
 */
#ifndef INC_ATTITUDE_CONTROL_H_
#define INC_ATTITUDE_CONTROL_H_

#include <stdint.h>

/* 每5ms调用: 标定状态机 (进行中时采样2秒均值存为当前姿态 pitch/roll 零偏) */
void AttCtrl_CalibTick(float pitch, float roll);

/* Z命令: 开始标定当前姿态零偏 (放平地保持静止2秒) */
void AttCtrl_CalibStart(void);

/* 标定是否进行中 */
uint8_t AttCtrl_CalibBusy(void);

/* 每5ms调用: 设置当前姿态ID (按前后腿膝分支+站高区分, 零偏按姿态分存) */
void AttCtrl_SetPose(uint8_t front_rev, uint8_t rear_rev, uint8_t high);

/* L:命令: 站立自稳开关。返回 0=已开 1=已关 2=当前姿态未标定(先Z) */
uint8_t AttCtrl_LevelToggle(void);

/* L:1 开关状态 (主循环拼 active 用; V模式1 无倾斜驱动时自稳自动跟随) */
uint8_t AttCtrl_LevelEnabled(void);

/* 每50/100ms调用: 输出四腿 z 补偿(mm, 死区±1°(fast 0.5°), 上限按站高动态算: 304.6−stand_h)。
 * active 由主循环判定 (站立静止/V模式1无倾斜驱动/W原地抬腿), 不满足输出全0;
 * fast=1 用快速低通 (W原地抬腿防翘板, 50ms 调用)。 */
void AttCtrl_LevelUpdate(float pitch, float roll, float stand_h, uint8_t active,
                         uint8_t fast, float dz[4]);

#endif /* INC_ATTITUDE_CONTROL_H_ */
