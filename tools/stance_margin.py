# -*- coding: utf-8 -*-
"""静态姿态舵机角度/余量/膝高计算 — 与固件 StandFootX + LegIK 公式一致

用法: python stance_margin.py <stand_h>   (默认 240)
足端: x = 15 + CROUCH 偏移 (前正后负), y = ±10, z = stand_h
膝高(几何) = stand_h - L1*cos(q2)   (髋线下方, 与轮径无关)
实际离地   = 几何 + 轮半径 54 (108mm 轮)
"""
import math, sys

L1, L2, D = 130.0, 180.0, 10.0
SHIFT = 15.0
CROUCH_F, CROUCH_FR = 30.0, 114.5    # 前腿 正常/反折 低趴前伸 (L 视觉姿态: 前足+129.5)
CROUCH_R, CROUCH_RR = 144.5, 135.0   # 后腿 正常/反折 低趴后伸 (L 后足-129.5)
H = float(sys.argv[1]) if len(sys.argv) > 1 else 240.0
CALIB = [
    [(95, 1), (115, -1), (93, 1)],     # FL
    [(99, -1), (150, 1), (135, -1)],   # FR (外展 2026-08-21 收5°)
    [(50, -1), (110, -1), (165, 1)],   # RL
    [(150, 1), (230, 1), (185, -1)],   # RR
]
NAMES = ['FL', 'FR', 'RL', 'RR']
J_NAMES = ['q1侧摆', 'q2大腿', 'q3膝']
Q1, Q2, Q3 = (-60, 60), (-90, 90), (0, 140)


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def solve(leg, x, y, z, rev):
    """与固件 LegIK_SolveServo 一致 (q3 反折取负)"""
    zf = math.sqrt(max(y * y + z * z - D * D, 0.0))
    D2 = x * x + zf * zf
    Dcur = clamp(math.sqrt(D2), abs(L1 - L2) * 1.0001 + 0.001, (L1 + L2) * 0.9999)
    sc = Dcur / math.sqrt(D2)
    xf, zf = x * sc, zf * sc
    cq3 = (xf * xf + zf * zf - L1 * L1 - L2 * L2) / (2 * L1 * L2)
    q3 = clamp(math.degrees(math.acos(clamp(cq3, -1, 1))), *Q3)
    if rev:
        q3 = -q3   # 反折分支: 先按正分支限幅再取负 (同固件 knee_reversed)
    q3r = math.radians(q3)
    q2 = math.atan2(xf, zf) - math.atan2(L2 * math.sin(q3r), L1 + L2 * math.cos(q3r))
    den = D * D + zf * zf
    q1 = math.atan2((D * z - zf * y) / den, (D * y + zf * z) / den)
    return (clamp(math.degrees(q1), *Q1),
            clamp(math.degrees(q2), *Q2),
            q3)   # q3 已在正分支限幅, 反折取负后不再限幅


def report(title, front_rev, rear_rev):
    print(f"--- {title} (站高 {H}mm) ---")
    worst = None
    for leg in range(4):
        rev = (leg < 2 and front_rev) or (leg >= 2 and rear_rev)
        xo = (CROUCH_FR if leg < 2 else CROUCH_RR) if rev else (CROUCH_F if leg < 2 else CROUCH_R)
        x = SHIFT + (xo if leg < 2 else -xo)
        y = -D if leg in (0, 2) else D
        q = solve(leg, x, y, H, rev)
        knee_h = H - L1 * math.cos(math.radians(q[1]))
        s = [CALIB[leg][j][0] + CALIB[leg][j][1] * q[j] for j in range(3)]
        m = [(s[j], 270 - s[j]) for j in range(3)]
        mmin = min(s[0], s[1], s[2], 270 - s[0], 270 - s[1], 270 - s[2])
        if worst is None or mmin < worst[0]:
            worst = (mmin, NAMES[leg], J_NAMES[[s[0], s[1], s[2], 270-s[0], 270-s[1], 270-s[2]].index(mmin) % 3])
        print(f"  {NAMES[leg]}: q=({q[0]:+6.1f},{q[1]:+6.1f},{q[2]:+6.1f})  "
              f"伺服=({s[0]:6.1f},{s[1]:6.1f},{s[2]:6.1f})  "
              f"余量下=({m[0][0]:5.1f},{m[1][0]:5.1f},{m[2][0]:5.1f})  "
              f"膝高(几何/实际)={knee_h:6.1f}/{knee_h + 54.0:6.1f}mm")
    print(f"  >>> 全机最小余量: {worst[0]:.1f}° ({worst[1]} {worst[2]})")
    print()


report("L 前顶低趴 (前反折+后正常, 足 +129.5/-129.5)", True, False)
report("M 后顶低趴 (前正常+后反折, 足 +45/-120)", False, True)


def report_sit():
    """坐姿 O: 前腿竖直伸直 (x=0, z=310) + 后腿反折深蹲 (x=-80, z=240)"""
    print(f"--- 坐姿 O (前腿竖直伸直 + 后腿反折深蹲) ---")
    worst = None
    for leg in range(4):
        x = 0.0 if leg < 2 else -80.0
        z = 310.0 if leg < 2 else 240.0
        y = -D if leg in (0, 2) else D
        q = solve(leg, x, y, z, leg >= 2)
        s = [CALIB[leg][j][0] + CALIB[leg][j][1] * q[j] for j in range(3)]
        m = [(s[j], 270 - s[j]) for j in range(3)]
        mmin = min(s[0], s[1], s[2], 270 - s[0], 270 - s[1], 270 - s[2])
        if worst is None or mmin < worst[0]:
            worst = (mmin, NAMES[leg])
        print(f"  {NAMES[leg]}: q=({q[0]:+6.1f},{q[1]:+6.1f},{q[2]:+6.1f})  "
              f"伺服=({s[0]:6.1f},{s[1]:6.1f},{s[2]:6.1f})  "
              f"余量下=({m[0][0]:5.1f},{m[1][0]:5.1f},{m[2][0]:5.1f})")
    print(f"  >>> 全机最小余量: {worst[0]:.1f}° ({worst[1]})")
    print()


report_sit()
