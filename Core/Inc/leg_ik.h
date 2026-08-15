/*
 * leg_ik.h
 * 3DOF 腿逆运动学 — 三关节 (髋侧摆 + 髋前摆 + 膝)
 *
 * 正解 (腿系 FRD: x前 y右 z下, 原点髋):
 *   x_f = L1 sin q2 + L2 sin(q2+q3)
 *   z_f = L1 cos q2 + L2 cos(q2+q3)
 *   x = x_f
 *   y = d cos q1 - z_f sin q1
 *   z = d sin q1 + z_f cos q1
 * 逆解:
 *   z_f = sqrt(y^2 + z^2 - d^2)
 *   q1  = atan2( d*z - z_f*y , d*y + z_f*z ) / (d²+z_f²) 形式
 *   q3  = acos( (x_f²+z_f² - L1² - L2²) / (2 L1 L2) )
 *   q2  = atan2(x_f, z_f) - atan2( L2 sin q3 , L1 + L2 cos q3 )
 */
#ifndef INC_LEG_IK_H_
#define INC_LEG_IK_H_

#include <stdint.h>

/* 腿部连杆 (mm, 实测: 髋前摆轴→膝轴 / 膝轴→脚底着地点) */
#define LEG_L1        130.0f
#define LEG_L2        180.0f
#define LEG_HIP_D      10.0f   /* 髋偏置: 侧摆轴→前摆平面, 左腿取负 */

/* 机身 (mm, 实测) */
#define BODY_BL       380.0f
#define BODY_BW       180.0f

/* 关节几何限位 (deg) */
#define Q1_MIN_DEG   (-60.0f)   /* 髋侧摆 ±60° */
#define Q1_MAX_DEG   ( 60.0f)
#define Q2_MIN_DEG   (-90.0f)   /* 髋前摆 ±90° */
#define Q2_MAX_DEG   ( 90.0f)
#define Q3_MIN_DEG   (  0.0f)   /* 膝 0~140° */
#define Q3_MAX_DEG   (140.0f)

typedef struct {
    float q1, q2, q3;   /* 几何关节角 (deg) */
} leg_joint_deg_t;

/* 舵机标定: servo = zero_deg + dir * q */
typedef struct {
    float zero_deg;
    float dir;
} joint_calib_t;

typedef enum {
    IK_OK = 0,
    IK_UNREACHABLE,   /* 不可达 (已钳位) */
    IK_OUT_OF_LIMIT   /* 超出关节限位 (已钳位) */
} ik_status_t;

/* 四腿标定表 [leg][关节] = [侧摆q1, 前摆q2, 膝q3] (实测) */
extern joint_calib_t g_leg_calib[4][3];

ik_status_t LegIK_Solve(uint8_t leg, float x, float y, float z,
                        leg_joint_deg_t *out);
ik_status_t LegIK_SolveServo(uint8_t leg, float x, float y, float z,
                             uint16_t out_deg[3]);
void LegIK_FK(uint8_t leg, const leg_joint_deg_t *j,
              float *x, float *y, float *z);
void LegIK_GetHipPos(uint8_t leg, float *hx, float *hy);

#endif /* INC_LEG_IK_H_ */
