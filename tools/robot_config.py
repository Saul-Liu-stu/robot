# -*- coding: utf-8 -*-
"""整机参数表 —— 与 ../robot_config.h 保持一致 (改参数两边都改)"""

# 腿部连杆 (mm), 实测值
LEG_L1 = 130.0     # 大腿: 髋前摆轴 -> 膝轴
LEG_L2 = 129.0     # 小腿: 膝轴 -> 轮轴心
LEG_HIP_D = 10.0   # 髋偏置: 侧摆轴 -> 前摆平面 (左腿取负)

# 机身 (mm), 实测值
BODY_BL = 410.0    # 前后髋轴心距
BODY_BW = 164.0    # 左右髋轴心距
WHEEL_DIA = 35.0   # 轮直径, 半径 17.5mm

# 关节限位 (rad)
import math
Q1_MIN, Q1_MAX = math.radians(-60), math.radians(60)
Q2_MIN, Q2_MAX = math.radians(-90), math.radians(90)
Q3_MIN, Q3_MAX = 0.0, math.radians(140)

# 步态参数
STAND_H = 200.0    # 站立足端 z (mm)
STEP_LEN = 60.0    # 步幅 (mm)
STEP_H = 25.0      # 抬腿高 (mm)
PERIOD = 1.2       # 周期 (s)
DUTY = 0.6         # 支撑相比例

# 腿: 0=FL 1=FR 2=RL 3=RR
HIP_X = [BODY_BL / 2, BODY_BL / 2, -BODY_BL / 2, -BODY_BL / 2]
HIP_Y = [-BODY_BW / 2, BODY_BW / 2, -BODY_BW / 2, BODY_BW / 2]
D_SIGN = [-1.0, 1.0, -1.0, 1.0]          # 髋偏置符号: 左负右正
LEG_NAMES = ['左前FL', '右前FR', '左后RL', '右后RR']
