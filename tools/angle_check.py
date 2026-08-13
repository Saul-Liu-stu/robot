# -*- coding: utf-8 -*-
"""步态关节角 + 舵机角度随时间变化, 用于检查标定是否正确"""
import sys, math, numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import font_manager

for fp in [r'C:\Windows\Fonts\msyh.ttc', r'C:\Windows\Fonts\simhei.ttf']:
    try: font_manager.fontManager.addfont(fp)
    except: pass
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei']
plt.rcParams['axes.unicode_minus'] = False

import robot_config as RC
from leg_ik import ik
from gait import foot_target

# ========== 舵机标定数据 (从 docs/硬件/舵机标定数据.md) ==========
calib_zero = [[98, 240, 153], [94, 35, 74], [50, 135, 220], [150, 200, 30]]
calib_dir  = [[1.0, -1.0, 1.0], [-1.0, 1.0, -1.0], [-1.0, -1.0, 1.0], [1.0, 1.0, -1.0]]
joint_names = ['外展 Abd', '大腿 Thigh', '小腿 Calf']
leg_names   = ['左前 FL', '右前 FR', '左后 RL', '右后 RR']
colors      = ['#d62728', '#1f77b4', '#2ca02c', '#ff7f0e']

DT = 0.005
TS = np.arange(0, RC.PERIOD * 2, DT)

def geom_to_servo(leg, q1, q2, q3):
    """几何角(deg) → 舵机角(0~270)"""
    c = calib_zero[leg]
    d = calib_dir[leg]
    return (c[0] + d[0] * q1, c[1] + d[1] * q2, c[2] + d[2] * q3)

# 预计算全程数据
data = {l: {'q1': [], 'q2': [], 'q3': [],
            's1': [], 's2': [], 's3': [],
            'fx': [], 'fy': [], 'fz': []} for l in range(4)}

for t in TS:
    for leg in range(4):
        d_s = RC.D_SIGN[leg] * RC.LEG_HIP_D
        ft  = foot_target(leg, t, d_s)
        q1, q2, q3, st = ik(*ft, d_s)
        s1, s2, s3 = geom_to_servo(leg, np.degrees(q1), np.degrees(q2), np.degrees(q3))
        d = data[leg]
        d['q1'].append(np.degrees(q1)); d['q2'].append(np.degrees(q2)); d['q3'].append(np.degrees(q3))
        d['s1'].append(s1); d['s2'].append(s2); d['s3'].append(s3)
        d['fx'].append(ft[0]); d['fy'].append(ft[1]); d['fz'].append(ft[2])

fig = plt.figure(figsize=(18, 14))

# ===== 上排: 关节几何角 q (4腿×3关节) =====
for j in range(3):
    ax = fig.add_subplot(3, 3, j + 1)
    for leg in range(4):
        key = f'q{j+1}'
        ax.plot(TS, data[leg][key], color=colors[leg], lw=1.2, label=leg_names[leg])
    ax.set_ylabel(f'q{j+1} (deg)')
    ax.set_title(f'几何角 — {joint_names[j]}')
    ax.grid(True, alpha=0.3)
    if j == 0: ax.legend(fontsize=7, loc='upper right')

# ===== 中排: 舵机角 s (0~270°) =====
for j in range(3):
    ax = fig.add_subplot(3, 3, j + 4)
    for leg in range(4):
        key = f's{j+1}'
        ax.plot(TS, data[leg][key], color=colors[leg], lw=1.2)
        # 标注 zero_deg 基准线
        ax.axhline(y=calib_zero[leg][j], color=colors[leg], ls=':', lw=0.8,
                   label=f'{leg_names[leg]} zero={calib_zero[leg][j]}')
    ax.set_ylabel(f's{j+1} (0~270)')
    ax.set_title(f'舵机角 — {joint_names[j]}')
    ax.set_ylim(0, 270)
    ax.grid(True, alpha=0.3)
    if j == 0: ax.legend(fontsize=6, loc='upper right')

# ===== 下排: 足端轨迹 X-Z (侧视) + X-Y (俯视) + 支撑/摆动标记 =====
ax_xz = fig.add_subplot(3, 3, 7)
for leg in range(4):
    ax_xz.plot(data[leg]['fx'], data[leg]['fz'], color=colors[leg], lw=1,
               label=leg_names[leg])
ax_xz.set_xlabel('X 前 (mm)'); ax_xz.set_ylabel('Z 下 (mm)')
ax_xz.set_title('足端轨迹 侧视 (X-Z)')
ax_xz.grid(True, alpha=0.3)
ax_xz.legend(fontsize=7)

ax_xy = fig.add_subplot(3, 3, 8)
for leg in range(4):
    ax_xy.plot(data[leg]['fx'], data[leg]['fy'], color=colors[leg], lw=1)
ax_xy.set_xlabel('X 前 (mm)'); ax_xy.set_ylabel('Y 右 (mm)')
ax_xy.set_title('足端轨迹 俯视 (X-Y)')
ax_xy.grid(True, alpha=0.3)

# 摆动态标记
ax_sw = fig.add_subplot(3, 3, 9)
for leg in range(4):
    swing = [1 if abs(fz) > RC.STAND_H * 0.5 else 0 for fz in data[leg]['fz']]
    ax_sw.plot(TS, [s + leg * 1.2 for s in swing], color=colors[leg], lw=2,
               label=leg_names[leg])
ax_sw.set_xlabel('t (s)'); ax_sw.set_ylabel('')
ax_sw.set_title('摆动态: 高=摆动 低=支撑')
ax_sw.set_yticks([l*1.2+0.5 for l in range(4)])
ax_sw.set_yticklabels(leg_names)
ax_sw.grid(True, alpha=0.3)

fig.suptitle(f'步态关节角 & 舵机角 (步幅={RC.STEP_LEN}mm 抬腿={RC.STEP_H}mm 周期={RC.PERIOD}s)', fontsize=14)
plt.tight_layout()
out = 'angle_check.png'
plt.savefig(out, dpi=120)
print(f'已保存: {out}')

# 打印关键值
print(f'\n{"腿":8s} {"关节":6s} {"舵机号":>5s} {"zero_deg":>8s} '
      f'{"舵机min":>8s} {"舵机max":>8s} {"摆幅":>8s} {"超限?":>6s}')
print('-' * 70)
for leg in range(4):
    for j in range(3):
        s = data[leg][f's{j+1}']
        smin, smax, zero = min(s), max(s), calib_zero[leg][j]
        warn = '!!' if smin < 0 or smax > 270 else 'OK'
        servo_n = [1,5,9, 2,6,10, 3,7,11, 4,8,12][leg*3 + j]
        print(f'{leg_names[leg]:8s} {joint_names[j]:6s} {servo_n:5d} {zero:8.0f} '
              f'{smin:8.0f} {smax:8.0f} {smax-smin:8.0f} {warn:>6s}')
