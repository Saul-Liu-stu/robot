# -*- coding: utf-8 -*-
"""U 站起完整路径仿真 — 趴姿(72mm) → 前顶低趴(191.3mm) 单段站起
逐 0.1s 采样验证: 每拍足端目标是否在 IK 限位内(无钳位)、路径是否单调、
膝高是否全程离地、重心挪移是否可行。与固件公式一致。"""
import math

L1, L2 = 130.0, 180.0
Q2, Q3 = (-90, 90), (0, 140)
CROUCH_FR, CROUCH_R = 114.5, 144.5
PARK_H, TARGET_H = 72.0, 191.3
RISE_CM_X, RISE_CM_Y = 15.0, 10.0
T_RISE = 3.5


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def ik(leg, x, y, z, rev):
    """与固件 LegIK 一致, 返回 (q1,q2,q3,是否被钳位)"""
    d = -10.0 if leg in (0, 2) else 10.0
    yz2 = y * y + z * z
    zf = math.sqrt(max(yz2 - d * d, 0.0))
    D2 = x * x + zf * zf
    Dcur = clamp(math.sqrt(D2), abs(L1 - L2) * 1.0001 + 0.001, (L1 + L2) * 0.9999)
    sc = Dcur / math.sqrt(D2)
    xf, zf2 = x * sc, zf * sc
    cq3 = (xf * xf + zf2 * zf2 - L1 * L1 - L2 * L2) / (2 * L1 * L2)
    q3 = clamp(math.degrees(math.acos(clamp(cq3, -1, 1))), *Q3)
    if rev:
        q3 = -q3
    q3r = math.radians(q3)
    q2 = math.degrees(math.atan2(xf, zf2) - math.atan2(L2 * math.sin(q3r), L1 + L2 * math.cos(q3r)))
    q2c = clamp(q2, *Q2)
    den = d * d + zf * zf
    q1 = math.degrees(math.atan2((d * z - zf * y) / den, (d * y + zf * z) / den))
    return q1, q2, q2c, q3, (q2 != q2c)


def profile(h):
    """固件 StandFootX 站起分支: 返回 (xf, xr) 含双边界与重心挪移"""
    xq2 = (130.0 + math.sqrt(32400.0 - h * h) + 2.0) if h < 180.0 else 0.0
    bf = math.sqrt(53361.0 - h * h) if 53361.0 > h * h else 0.0
    br = math.sqrt(43594.0 - h * h) if 43594.0 > h * h else 0.0
    xf = max(15.0 + CROUCH_FR, bf, xq2)
    xr = max(CROUCH_R - 15.0, br, xq2)
    cmx = RISE_CM_X * clamp((h - 100.0) / 60.0, 0, 1)
    return xf + cmx, xr + cmx, cmx


print("t(s)  h(mm)  前足x  后足x  cmx | 前q2(钳?) 前q3   后q2(钳?) 后q3   膝高前/后")
prev = None
problems = []
for i in range(36):   # 0..3.5s 每0.1s
    t = i * 0.1
    h = PARK_H + (TARGET_H - PARK_H) * (t / T_RISE)
    h = min(h, TARGET_H)
    xf, xr, cmx = profile(h)
    cmy = RISE_CM_Y * clamp((h - 100.0) / 60.0, 0, 1)
    # 前腿(反折) FL/FR 对称, 后腿(正常) RL/RR 对称
    q1f, q2f, q2fc, q3f, clp_f = ik(0, xf, -10.0 + cmy, h, True)
    q1r, q2r, q2rc, q3r, clp_r = ik(2, -xr, -10.0 + cmy, h, False)
    knee_f = h - L1 * math.cos(math.radians(q2f))    # 几何膝高
    knee_r = h - L1 * math.cos(math.radians(q2r))
    flag = ""
    if clp_f or clp_r:
        flag = "  ✗✗ 钳位!"
        problems.append((t, h, xf, xr, clp_f, clp_r))
    if prev is not None and (xf > prev[2] + 0.01 or xr > prev[3] + 0.01):
        flag += "  ⚠非单调!"
        problems.append((t, h, xf, xr, "非单调", ""))
    if knee_f < 60 or knee_r < 60:
        flag += "  ⚠膝低!"
        problems.append((t, h, knee_f, knee_r, "膝低", ""))
    print("t=%.1f  h=%5.1f  xf=%6.1f  xr=%6.1f  cmx=%4.1f | "
          "q2f=%+6.1f%s q3f=%+6.1f  q2r=%+6.1f%s q3r=%+6.1f  | 膝 %5.1f/%5.1f%s"
          % (t, h, xf, xr, cmx, q2f, "(钳)" if clp_f else "", q3f,
             q2r, "(钳)" if clp_r else "", q3r, knee_f, knee_r, flag))
    prev = (t, h, xf, xr)

print()
if problems:
    print("✗ 发现问题 %d 处:" % len(problems))
    for p in problems:
        print("  ", p)
else:
    print("✓ 全路径无钳位、单调、膝高安全")
