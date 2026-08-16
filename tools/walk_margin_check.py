# -*- coding: utf-8 -*-
"""低站姿 240mm 行走全周期舵机角度扫描 — 与固件 walk_gait.c / leg_ik.c 公式一致"""
import math

L1, L2, D = 130.0, 180.0, 10.0
import sys
S, H, PERIOD, DUTY = 60.0, 25.0, 1.2, 0.6
STAND_H = float(sys.argv[1]) if len(sys.argv) > 1 else 240.0
X_SHIFT = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
PHASE = [0.0, 0.5, 0.5, 0.0]          # FL FR RL RR trot 相位
HIP_D = [-D, D, -D, D]

# [腿][关节] = (zero, dir)  q1侧摆 q2前摆 q3膝
CALIB = [
    [(95,1),(115,-1),(93,1)],    # FL (大腿重装 2026-08-16)
    [(94,-1),(150,1),(135,-1)],  # FR (大腿重装 2026-08-16)
    [(50,-1),(110,-1),(165,1)],  # RL
    [(150,1),(230,1),(185,-1)],  # RR (小腿重装 2026-08-16)
]
Q1 = (-60, 60); Q2 = (-90, 90); Q3 = (0, 140)

def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v

def foot(leg, t):
    phi = t / PERIOD + PHASE[leg]
    phi -= math.floor(phi)
    if phi < DUTY:
        u = phi / DUTY
        x = S*0.5 - S*u
    else:
        u = (phi - DUTY) / (1 - DUTY)
        x = -S*0.5 + S*u
        return x + X_SHIFT, HIP_D[leg], STAND_H - H*math.sin(math.pi*u)
    return x + X_SHIFT, HIP_D[leg], STAND_H

def ik(leg, x, y, z):
    d = HIP_D[leg]
    yz2 = y*y + z*z
    if yz2 < d*d: yz2 = d*d
    zf = math.sqrt(yz2 - d*d)
    den = d*d + zf*zf
    q1 = math.atan2((d*z - zf*y)/den, (d*y + zf*z)/den)
    xf = x
    D2 = xf*xf + zf*zf
    Dmin = abs(L1-L2)*1.0001 + 0.001
    Dmax = (L1+L2)*0.9999
    if D2 > Dmax*Dmax or D2 < Dmin*Dmin:
        pass  # 不可达, 下面缩放处理
    Dcur = clamp(math.sqrt(D2), Dmin, Dmax)
    sc = Dcur / math.sqrt(D2)
    xf *= sc; zf *= sc
    cq3 = (xf*xf + zf*zf - L1*L1 - L2*L2) / (2*L1*L2)
    q3 = math.acos(clamp(cq3, -1, 1))
    q2 = math.atan2(xf, zf) - math.atan2(L2*math.sin(q3), L1 + L2*math.cos(q3))
    return (clamp(math.degrees(q1), *Q1),
            clamp(math.degrees(q2), *Q2),
            clamp(math.degrees(q3), *Q3))

N = 240  # 每周期采样点
LEG_NAMES = ['FL', 'FR', 'RL', 'RR']
J_NAMES = ['侧摆q1', '前摆q2', '膝q3']
print(f"站高 {STAND_H}mm  步幅 {S}mm  抬腿 {H}mm  周期 {PERIOD}s  duty {DUTY}\n")

for leg in range(4):
    print(f"--- {LEG_NAMES[leg]} ---")
    for j in range(3):
        zero, dirn = CALIB[leg][j]
        sv = []
        for i in range(N + 1):
            t = i / N * PERIOD
            x, y, z = foot(leg, t)
            q = ik(leg, x, y, z)[j]
            s_raw = zero + dirn * q
            s_clp = clamp(s_raw, 0, 270)
            sv.append((t, s_raw, s_clp))
        mn = min(s[2] for s in sv); mx = max(s[2] for s in sv)
        clip_pts = [(t, s_raw, s_clp) for t, s_raw, s_clp in sv if s_raw != s_clp]
        print(f"  {J_NAMES[j]}: 舵机角 [{mn:6.1f}°, {mx:6.1f}°]  余量(下/上): {mn:5.1f}°/{270-mx:5.1f}°", end='')
        if clip_pts:
            t0, raw, clp = clip_pts[0]
            print(f"  ⚠钳位! 目标 {raw:+.1f}° 被钳到 {clp:.0f}° (t={t0:.2f}s, 摆动相)")
        else:
            print("  无钳位")
print()

# RR 小腿钳位导致的真实抬腿高度
leg = 3
print("--- RR 小腿钳位对抬腿的实际影响 ---")
for u in [0.25, 0.5, 0.75]:
    phi = PHASE[leg] + 0.6 + u*0.4
    x = -S*0.5 + S*u
    z_req = STAND_H - H*math.sin(math.pi*u)
    _, y, _ = x, HIP_D[leg], z_req
    q3_t = ik(leg, x, y, z_req)[2]
    s_req = CALIB[leg][2][0] + CALIB[leg][2][1]*q3_t
    s_clp = clamp(s_req, 0, 270)
    # 反推钳位后实际 q3 -> 实际 z
    q3_act = (s_clp - CALIB[leg][2][0]) / CALIB[leg][2][1]
    zf_act = math.sqrt(L1*L1 + L2*L2 + 2*L1*L2*math.cos(math.radians(q3_act))) if x == 0 else None
    # zf_act 需要解 x 分量, 这里用数值法: 扫 q2 使 x 匹配
    q2 = math.atan2(x, 0) if False else None
    best = None
    for q2d in [v/10 for v in range(-900, 901)]:
        q2r = math.radians(q2d)
        xf = L1*math.sin(q2r) + L2*math.sin(q2r + math.radians(q3_act))
        zf = L1*math.cos(q2r) + L2*math.cos(q2r + math.radians(q3_act))
        if abs(xf - x) < 0.5:
            # 检查 q2 限位内
            q2_c = clamp(q2d, *Q2)
            if q2_c != q2d: continue
            best = zf
            break
    z_act = best
    print(f"  摆动进度 u={u:.2f}: 目标z={z_req:5.1f}mm 目标伺服{s_req:+6.1f}° → "
          f"实际伺服{s_clp:.0f}° → 实际z={z_act:5.1f}mm (抬腿 {STAND_H-z_act:4.1f}mm / 要求 {H*math.sin(math.pi*u):4.1f}mm)")
