# -*- coding: utf-8 -*-
"""U 站起阶段A 前腿(反折)足端轨迹可行性扫描 — 与固件 LegIK 限幅一致
输出: 各高度下 q2/q3 均未钳位的可行 x 区间, 以及当前固件轨迹的钳位情况"""
import math

L1, L2 = 130.0, 180.0
Q2 = (-90, 90)
Q3 = (0, 140)


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def ik_rev(x, h):
    d2 = x * x + h * h
    cq3 = (d2 - L1 * L1 - L2 * L2) / (2 * L1 * L2)
    q3 = clamp(math.degrees(math.acos(clamp(cq3, -1, 1))), *Q3)
    q3 = -q3
    q3r = math.radians(q3)
    q2 = math.degrees(math.atan2(x, h) - math.atan2(L2 * math.sin(q3r), L1 + L2 * math.cos(q3r)))
    q2c = clamp(q2, *Q2)
    return q2, q3, q2c, (q2 != q2c)


print("=== 反折前腿可行 x 区间 (q2/q3 均未钳位且 q3>=-85) ===")
for h in [72, 100, 120, 140, 160, 179.3, 200, 220, 240]:
    ok = [x for x in range(0, 315) if (lambda r: (not r[3]) and r[1] > -85.01)(ik_rev(float(x), h))]
    if ok:
        print("  h=%5.0f: x in [%3d, %3d]" % (h, ok[0], ok[-1]))
    else:
        print("  h=%5.0f: 无可行点!" % h)

print()
print("=== 当前固件阶段A前足轨迹: xf = max(15+105k, sqrt(53361-h^2)) ===")
for h in [72, 100, 140, 179.3, 220, 240]:
    k = (280.0 - h) / (280.0 - 240.0)
    k = clamp(k, 0, 1)
    bf = math.sqrt(53361 - h * h) if 53361 > h * h else 0.0
    xf = max(15.0 + 105.0 * k, bf)
    q2, q3, q2c, clipped = ik_rev(xf, h)
    mark = "✗ 钳位!" if (clipped or q3 <= -85.01) else "✓ 可行"
    print("  h=%5.0f: xf=%6.1f -> q2=%+7.1f(钳%+.0f) q3=%+6.1f  %s" % (h, xf, q2, q2c, q3, mark))
