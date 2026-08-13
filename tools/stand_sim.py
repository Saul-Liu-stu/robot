#!/usr/bin/env python3
"""
四足站立姿态仿真 v2 — 使用 leg_ik.c 正版 FK
"""
import numpy as np
import matplotlib
matplotlib.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'DejaVu Sans']
matplotlib.rcParams['axes.unicode_minus'] = False
import matplotlib.pyplot as plt

# ========== leg_ik.h 参数 ==========
L1, L2, HIP_D = 130.0, 129.0, 10.0
BODY_BL, BODY_BW = 410.0, 164.0

# ========== 舵机标定数据 ==========
# [leg][joint] = [servo_num, zero_deg, dir]
# 腿编号: 0=FL左前, 1=FR右前, 2=RL左后, 3=RR右后
srv_map = [[1,5,9],[2,6,10],[3,7,11],[4,8,12]]
calib_zero = [[98,240,153],[94,35,74],[50,135,220],[150,200,30]]
calib_dir  = [[1.0,-1.0,1.0],[-1.0,1.0,-1.0],[-1.0,-1.0,1.0],[1.0,1.0,-1.0]]

hip_x = [BODY_BL/2, BODY_BL/2, -BODY_BL/2, -BODY_BL/2]
hip_y = [-BODY_BW/2, BODY_BW/2, -BODY_BW/2, BODY_BW/2]
hip_d_sign = [-HIP_D, HIP_D, -HIP_D, HIP_D]
leg_names = ["FL LeftFront", "FR RightFront", "RL LeftRear", "RR RightRear"]

def servo_to_geom(leg, servo_angles):
    """舵机角度 → 几何角度 (leg_ik.c line 96-98 逆向)"""
    z = calib_zero[leg]
    d = calib_dir[leg]
    return [(servo_angles[j] - z[j]) / d[j] for j in range(3)]

def fk(leg, q1, q2, q3):
    """leg_ik.c LegIK_FK 正版实现"""
    d = hip_d_sign[leg]
    q1r, q2r, q3r = np.radians(q1), np.radians(q2), np.radians(q3)
    xf = L1*np.sin(q2r) + L2*np.sin(q2r+q3r)
    zf = L1*np.cos(q2r) + L2*np.cos(q2r+q3r)
    c1, s1 = np.cos(q1r), np.sin(q1r)
    return xf, d*c1 - zf*s1, d*s1 + zf*c1

print("="*70)
print("  Standing Pose Simulation — leg IK FK (geometric angles at stand)")

# Get zero_deg as servo angles
servo_stand = [[calib_zero[l][j] for j in range(3)] for l in range(4)]

print(f"\n{'Leg':15s} {'Servo(Abd/Th/Ca)':25s} {'q1,q2,q3 (deg)':30s} {'Foot (x,y,z) mm':30s}")
print("-"*100)

leg_foot = []
for l in range(4):
    s = servo_stand[l]
    q = servo_to_geom(l, s)
    fx, fy, fz = fk(l, q[0], q[1], q[2])
    leg_foot.append((fx+hip_x[l], fy+hip_y[l], fz))
    print(f"{leg_names[l]:15s} {s[0]:3d} {s[1]:3d} {s[2]:3d}"
          f"        {q[0]:+6.1f} {q[1]:+6.1f} {q[2]:+6.1f}"
          f"          {fx+hip_x[l]:+8.1f} {fy+hip_y[l]:+8.1f} {fz:+8.1f}")

# ---- 绘图 ----
fig, (ax_side, ax_top) = plt.subplots(1, 2, figsize=(14, 7))

# 侧视图
for l, col in zip(range(4), ['red','blue','orange','green']):
    q = servo_to_geom(l, servo_stand[l])
    q2r, q3r = np.radians(q[1]), np.radians(q[2])
    knee_x = L1*np.sin(q2r)
    knee_z = L1*np.cos(q2r)
    foot_x = knee_x + L2*np.sin(q2r+q3r)
    foot_z = knee_z + L2*np.cos(q2r+q3r)
    ax_side.plot([hip_x[l], hip_x[l]+knee_x], [0, -knee_z], color=col, lw=3)
    ax_side.plot([hip_x[l]+knee_x, hip_x[l]+foot_x], [-knee_z, -foot_z], color=col, lw=2, ls='--')
    ax_side.plot(hip_x[l]+foot_x, -foot_z, 'o', color=col, ms=8)
    ax_side.plot(hip_x[l], 0, 'k+', ms=10)

ax_side.axhline(y=0, color='gray', ls=':', alpha=0.5)
ax_side.set_aspect('equal')
ax_side.set_xlabel('X (mm)')
ax_side.set_ylabel('Z (mm)')
ax_side.set_title('Side View')
ax_side.grid(alpha=0.3)

# 俯视图
for l, col in enumerate(['red','blue','orange','green']):
    fx, fy, fz = leg_foot[l]
    ax_top.plot(hip_x[l], hip_y[l], 'k+', ms=10)
    ax_top.plot(fx, fy, 'o', color=col, ms=10, label=leg_names[l])

bx = plt.Rectangle((-BODY_BL/2,-BODY_BW/2), BODY_BL, BODY_BW, fill=False, ec='gray', lw=2, ls=':')
ax_top.add_patch(bx)
ax_top.set_aspect('equal')
ax_top.set_xlabel('X (mm)')
ax_top.set_ylabel('Y (mm)')
ax_top.set_title('Top View')
ax_top.legend(fontsize=8)
ax_top.grid(alpha=0.3)

fig.suptitle('Quadruped Standing Pose (servo zero_deg → geometric FK)', fontsize=14)
plt.tight_layout()
plt.show()

# 检查
fz_vals = [fz for _,_,fz in leg_foot]
print(f"\nFoot depths: {[f'{z:.0f}' for z in fz_vals]} mm below hip")
print(f"Spread: {max(fz_vals)-min(fz_vals):.0f} mm")
print(f"Avg: {np.mean(fz_vals):.0f} mm")
print(f"\nL1+L2 = {L1+L2:.0f} mm (leg fully straight length)")
print("If standing height != avg foot depth, adjust STAND_H_MM in gait.h")
