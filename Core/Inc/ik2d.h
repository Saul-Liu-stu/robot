/*
 * ik2d.h
 * 二维矢状面 IK — 每条腿独立几何参数
 *
 * 坐标系(腿系, FRD): x前, z下, 原点在髋转轴, 单位 mm
 * 几何角: q2 = 大腿与竖直夹角 (前为正), q3 = 膝弯角 (0=伸直)
 *
 * 正解: x = L1·sin(q2) + L2·sin(q2+q3)
 *       z = L1·cos(q2) + L2·cos(q2+q3)
 * 逆解: D = sqrt(x²+z²)
 *       q3 = acos((D²-L1²-L2²)/(2·L1·L2))
 *       q2 = atan2(x,z) - atan2(L2·sin(q3), L1+L2·cos(q3))
 *
 * 舵机换算: servo = zero_deg + dir × q_deg
 *   zero_deg = 几何零点时舵机读数 (大腿竖直 / 大小腿成直线)
 */
#ifndef INC_IK2D_H_
#define INC_IK2D_H_

#include <stdint.h>

typedef struct {
    float l1, l2;        /* 大腿/小腿长度 mm */
    float th_zero;       /* 大腿零点 (大腿竖直时舵机角) */
    float ca_zero;       /* 小腿零点 (大小腿成直线时舵机角) */
    float th_dir;        /* 大腿 dir */
    float ca_dir;        /* 小腿 dir */
} LegGeom2D;

/* 四腿几何参数 (实测) */
extern const LegGeom2D g_leg_geom[4];

/* 脚目标(腿系 x前 z下 mm) → 舵机角 (0~270)
 * 返回 0 成功; 目标超出腿长时等比例缩回可达范围 */
int IK2D_Solve(uint8_t leg, float x, float z,
               uint16_t *th_servo, uint16_t *ca_servo);

/* 正解: 舵机角 → 足端位置 (验证用) */
void IK2D_FK(uint8_t leg, uint16_t th_servo, uint16_t ca_servo,
             float *x, float *z);

#endif /* INC_IK2D_H_ */
