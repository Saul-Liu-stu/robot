/*
 * gait.h
 * 四足步态引擎 — 足端轨迹 + 二维 IK
 *
 * 每个步态周期 = GAIT_CYCLE_MS 毫秒
 * 每条腿有独立的相位偏移(0~360°)，步态就是四条腿的相位组合:
 *   Trot:  Leg1=0°  Leg2=180°  Leg3=180°  Leg4=0°   (对角同步)
 *   Walk:  Leg1=0°  Leg2=90°   Leg3=180°  Leg4=270°  (轮序)
 *   Crawl: Leg1=0°  Leg2=120°  Leg3=240°  Leg4=60°   (匍匐)
 *
 * 每个相位周期内:
 *   - 前一半(0~180°): 摆动相 → 脚抬离地面往前扫
 *   - 后一半(180~360°):支撑相 → 脚贴地往后推身体
 *
 * 脚目标(mm) → ik2d.c 逆解 → 舵机角 → Leg_SetJoint
 */
#ifndef INC_GAIT_H_
#define INC_GAIT_H_

#include "main.h"
#include "leg_config.h"

/* 步态类型 */
typedef enum {
    GAIT_STAND = 0,   /* 站立不动 */
    GAIT_TROT,        /* 对角小跑 */
    GAIT_WALK,        /* 爬行 */
    GAIT_CRAWL,       /* 匍匐 */
    GAIT_NUM
} GaitType;

/* 单腿相位信息 */
typedef struct {
    uint16_t phase;     /* 当前相位 0~359° */
    uint8_t  is_swing;  /* 是否摆动相 */
} LegPhase;

/* ========== API ========== */

/* 初始化：双腿站立，相位清零 */
void Gait_Init(void);

/* 切换步态 */
void Gait_SetType(GaitType type);

/* 每循环调用一次，更新所有关节 */
void Gait_Update(void);

/* 获取当前相位信息（调试用） */
const LegPhase* Gait_GetPhase(uint8_t leg);

/* 调试: 单步推进 phase° 并执行一帧 */
void Gait_DebugStep(uint16_t deg);

/* 调试: 单步插值起点重新同步站姿 (回站姿后调用) */
void Gait_DebugReset(void);

#endif
