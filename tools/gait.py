# -*- coding: utf-8 -*-
"""trot 步态发生器 —— 与 ../gait.c 逐行对应 (黄金参考)"""
import math
from robot_config import STEP_LEN, STEP_H, PERIOD, DUTY, STAND_H

# trot: FL 与 RR 同相, FR 与 RL 同相, 错半拍
PHASE_TROT = [0.0, 0.5, 0.5, 0.0]


def foot_target(leg, t, d_signed,
                S=STEP_LEN, H=STEP_H, T=PERIOD, beta=DUTY, stand_h=STAND_H):
    """某腿某时刻足端目标 (腿系 FRD, mm) -> (x, y, z)"""
    phi = (t / T + PHASE_TROT[leg]) % 1.0

    if phi < beta:                       # 支撑相: 贴地匀速后移
        u = phi / beta
        x = S * 0.5 - S * u
        z = stand_h
    else:                                # 摆动相: 半椭圆弧线回到前方
        u = (phi - beta) / (1.0 - beta)
        x = -S * 0.5 + S * u
        z = stand_h - H * math.sin(math.pi * u)
    return x, d_signed, z
