# -*- coding: utf-8 -*-
"""3DOF 腿逆/正运动学 —— 与 ../leg_ik.c 逐行对应 (黄金参考)"""
import math
from robot_config import LEG_L1, LEG_L2, Q1_MIN, Q1_MAX, Q2_MIN, Q2_MAX, Q3_MIN, Q3_MAX


def _clamp(v, lo, hi):
    return max(lo, min(hi, v))


def ik(x, y, z, d, l1=LEG_L1, l2=LEG_L2):
    """腿系足端目标 (x前, y右, z下, mm) + 带符号髋偏置 d -> (q1,q2,q3, status)
    status: 0=OK 1=不可达(已钳位) 2=超限位(已钳位)"""
    st = 0
    yz2 = y * y + z * z
    if yz2 < d * d:
        yz2 = d * d
        st = 1
    zf = math.sqrt(yz2 - d * d)

    den = d * d + zf * zf
    q1 = math.atan2(d * z - zf * y, d * y + zf * z)

    xf = x
    dmax = (l1 + l2) * 0.9999
    dmin = abs(l1 - l2) * 1.0001 + 0.001
    D2 = xf * xf + zf * zf
    if D2 > dmax * dmax or D2 < dmin * dmin:
        st = 1
    D = _clamp(math.sqrt(D2), dmin, dmax)
    scale = D / math.sqrt(D2)
    xf *= scale
    zf *= scale

    cq3 = (xf * xf + zf * zf - l1 * l1 - l2 * l2) / (2 * l1 * l2)
    q3 = math.acos(_clamp(cq3, -1.0, 1.0))
    q2 = math.atan2(xf, zf) - math.atan2(l2 * math.sin(q3), l1 + l2 * math.cos(q3))

    q1c = _clamp(q1, Q1_MIN, Q1_MAX)
    q2c = _clamp(q2, Q2_MIN, Q2_MAX)
    q3c = _clamp(q3, Q3_MIN, Q3_MAX)
    if st == 0 and (q1c != q1 or q2c != q2 or q3c != q3):
        st = 2
    return q1c, q2c, q3c, st


def fk(q1, q2, q3, d, l1=LEG_L1, l2=LEG_L2):
    """关节角 -> 腿系足端位置 (用于验证 IK)"""
    xf = l1 * math.sin(q2) + l2 * math.sin(q2 + q3)
    zf = l1 * math.cos(q2) + l2 * math.cos(q2 + q3)
    c1, s1 = math.cos(q1), math.sin(q1)
    return xf, d * c1 - zf * s1, d * s1 + zf * c1


def leg_segments(q1, q2, q3, d, l1=LEG_L1, l2=LEG_L2):
    """返回腿系下各关节点坐标 (髋原点, 前摆轴, 膝, 足端), 供画图"""
    c1, s1 = math.cos(q1), math.sin(q1)
    P = (0.0, d * c1, d * s1)                       # 髋前摆轴心
    a2 = q2
    a23 = q2 + q3
    K = (P[0] + l1 * math.sin(a2),
         P[1] - l1 * math.cos(a2) * s1,
         P[2] + l1 * math.cos(a2) * c1)
    F = (K[0] + l2 * math.sin(a23),
         K[1] - l2 * math.cos(a23) * s1,
         K[2] + l2 * math.cos(a23) * c1)
    return [(0.0, 0.0, 0.0), P, K, F]
