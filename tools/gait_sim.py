# -*- coding: utf-8 -*-
"""步态仿真 — 舵机角度空间直驱, 实时显示 12 路舵机角度"""
import sys, math, numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager, animation
from mpl_toolkits.mplot3d import Axes3D

for fp in [r'C:\Windows\Fonts\msyh.ttc', r'C:\Windows\Fonts\simhei.ttf']:
    try: font_manager.fontManager.addfont(fp)
    except: pass
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei']
plt.rcParams['axes.unicode_minus'] = False

# ======== 标定数据 ========
STAND_POSE = [[98,240,153],[94,35,74],[50,135,220],[150,200,30]]
CALIB_DIR  = [[1.0,-1.0,1.0],[-1.0,1.0,-1.0],[-1.0,-1.0,1.0],[1.0,1.0,-1.0]]
LEG_NAMES  = ['左前FL','右前FR','左后RL','右后RR']
J_NAMES    = ['外展','大腿','小腿']

# ======== gait.c 参数 ========
STEP_LEN  = 40     # GAIT_STEP_LENGTH
STEP_H    = 30     # GAIT_STEP_HEIGHT
CYCLE_MS  = 800    # GAIT_CYCLE_MS
DT        = 10     # 仿真每帧10ms

# ======== gait_offsets (TROT) ========
OFFSETS = [0, 180, 180, 0]  # TROT

# ======== 几何估算 (用于画腿, 非精确) ========
L1, L2 = 130.0, 129.0       # leg_ik.h 默认尺寸
HIP_X = [205, 205, -205, -205]
HIP_Y = [-82, 82, -82, 82]
HIP_D_SIGN = [-10.0, 10.0, -10.0, 10.0]

def geom_angle(servo, zero, dr):
    """舵机角 → 近似几何角 (rad)"""
    return np.radians(dr * (servo - zero))

def fk_approx(leg, abd_servo, th_servo, ca_servo):
    """近似正运动学: 舵机角 → 足端在机身系的位置"""
    d = HIP_D_SIGN[leg]
    q1 = geom_angle(abd_servo, STAND_POSE[leg][0], CALIB_DIR[leg][0])
    q2 = geom_angle(th_servo, STAND_POSE[leg][1], CALIB_DIR[leg][1])
    q3 = geom_angle(ca_servo, STAND_POSE[leg][2], CALIB_DIR[leg][2])
    xf = L1*np.sin(q2) + L2*np.sin(q2+q3)
    zf = L1*np.cos(q2) + L2*np.cos(q2+q3)
    c1, s1 = np.cos(q1), np.sin(q1)
    fx = HIP_X[leg] + xf
    fy = HIP_Y[leg] + d*c1 - zf*s1
    fz = d*s1 + zf*c1
    return fx, fy, fz

# ======== gait.c swing_traj / stance_traj ========
def gait_delta(local_phase):
    """返回 (thigh_delta, calf_delta)"""
    if local_phase < 180:  # 摆动相
        td = int(STEP_LEN * (90 - local_phase) / 90)
        if local_phase <= 90:
            cd = int(STEP_H * local_phase / 90)
        else:
            cd = int(STEP_H * (180 - local_phase) / 90)
        cd = -cd
    else:  # 支撑相
        p = local_phase - 180
        td = int(STEP_LEN * (p - 90) / 90)
        cd = 0
    return td, cd

# ======== 预计算全程 ========
FRAMES = int(CYCLE_MS * 2 / DT)  # 2个周期
data = {l: {'th':[], 'ca':[], 'ph':[]} for l in range(4)}
g_phase = 0

for frame in range(FRAMES):
    dt_ms = DT
    advance = CYCLE_MS / 360
    if dt_ms >= advance:
        g_phase = (g_phase + int(dt_ms / advance)) % 360
    for leg in range(4):
        lp = (g_phase + OFFSETS[leg]) % 360
        td, cd = gait_delta(lp)
        th = np.clip(int(STAND_POSE[leg][1] + CALIB_DIR[leg][1] * td), 0, 270)
        ca = np.clip(int(STAND_POSE[leg][2] + CALIB_DIR[leg][2] * cd), 0, 270)
        data[leg]['th'].append(th)
        data[leg]['ca'].append(ca)
        data[leg]['ph'].append(lp)
        data[leg]['abd'] = STAND_POSE[leg][0]  # 外展锁死

# ======== 画图 ========
fig = plt.figure(figsize=(20, 14))

# 左侧: 3D 腿杆
ax3d = fig.add_subplot(2, 4, (1, 4), projection='3d')

# 右侧: 舵机角度曲线
ax_abd = fig.add_subplot(4, 4, 5)
ax_th  = fig.add_subplot(4, 4, 9)
ax_ca  = fig.add_subplot(4, 4, 13)

# 底部: 相位状态
ax_ph = fig.add_subplot(4, 4, (14, 16))

colors = ['#d62728', '#1f77b4', '#2ca02c', '#ff7f0e']

# ===== 3D 快照: 画 t=0 时刻 =====
t_idx = 0
for leg in range(4):
    abd = STAND_POSE[leg][0]
    th  = data[leg]['th'][t_idx]
    ca  = data[leg]['ca'][t_idx]
    # 近似计算关节点
    d = HIP_D_SIGN[leg]
    q1 = geom_angle(abd, STAND_POSE[leg][0], CALIB_DIR[leg][0])
    q2 = geom_angle(th, STAND_POSE[leg][1], CALIB_DIR[leg][1])
    q3 = geom_angle(ca, STAND_POSE[leg][2], CALIB_DIR[leg][2])
    c1, s1 = np.cos(q1), np.sin(q1)
    # 髋原点
    hx, hy, hz = HIP_X[leg], HIP_Y[leg], 0.0
    # 前摆轴
    px, py, pz = hx, hy + d*c1, d*s1
    # 膝
    kx = px + L1*np.sin(q2)
    ky = py - L1*np.cos(q2)*s1
    kz = pz + L1*np.cos(q2)*c1
    # 足
    fx = kx + L2*np.sin(q2+q3)
    fy = ky - L2*np.cos(q2+q3)*s1
    fz = kz + L2*np.cos(q2+q3)*c1
    ax3d.plot([hx,px,kx,fx], [hy,py,ky,fy], [-hz,-pz,-kz,-fz],
              '-o', ms=3, color=colors[leg], lw=1.5, label=LEG_NAMES[leg])

# 机身框
bl, bw = 205, 82
bx = [bl,bl,-bl,-bl,bl]; by = [-bw,bw,bw,-bw,-bw]
ax3d.plot(bx, by, [0]*5, 'k-', lw=2)
ax3d.set_xlim(-280,280); ax3d.set_ylim(-200,200); ax3d.set_zlim(-300,100)
ax3d.set_xlabel('X'); ax3d.set_ylabel('Y'); ax3d.set_zlabel('Z')
ax3d.set_title('站立姿态 (t=0)'); ax3d.legend(fontsize=7)

# ===== 舵机角度时间曲线 =====
t = np.arange(FRAMES) * DT / 1000.0
for leg in range(4):
    ax_abd.axhline(y=STAND_POSE[leg][0], color=colors[leg], ls=':', lw=0.8)
    ax_abd.text(t[-1], STAND_POSE[leg][0], f'L{leg+1}={STAND_POSE[leg][0]}', fontsize=6, color=colors[leg])
ax_abd.set_ylabel('外展(deg)'); ax_abd.set_title('外展 — 锁死不动'); ax_abd.grid(alpha=0.3)

for leg in range(4):
    ax_th.plot(t, data[leg]['th'], color=colors[leg], lw=1, label=LEG_NAMES[leg])
    ax_th.axhline(y=STAND_POSE[leg][1], color=colors[leg], ls=':', lw=0.6)
ax_th.set_ylabel('大腿(deg)'); ax_th.set_title('大腿 — 前后摆动'); ax_th.grid(alpha=0.3)
ax_th.legend(fontsize=6, loc='upper right')

for leg in range(4):
    ax_ca.plot(t, data[leg]['ca'], color=colors[leg], lw=1)
    ax_ca.axhline(y=STAND_POSE[leg][2], color=colors[leg], ls=':', lw=0.6)
ax_ca.set_ylabel('小腿(deg)'); ax_ca.set_title('小腿 — 弯膝抬腿'); ax_ca.grid(alpha=0.3)
ax_ca.set_xlabel('时间 (s)')

# ===== 相位状态 =====
for leg in range(4):
    swing_mask = [1 if p < 180 else 0 for p in data[leg]['ph']]
    ax_ph.fill_between(t, leg*1.2, leg*1.2+np.array(swing_mask),
                       color=colors[leg], alpha=0.6, label=LEG_NAMES[leg])
ax_ph.set_xlabel('时间 (s)'); ax_ph.set_title('摆动态: 高=摆动(抬腿) 低=支撑(蹬地)')
ax_ph.set_yticks([l*1.2+0.5 for l in range(4)])
ax_ph.set_yticklabels(LEG_NAMES); ax_ph.grid(alpha=0.3)

# ===== 打印关键角度值 =====
print("="*80)
print("  TROT 步态 — 舵机角度范围")
print(f"  STEP_LEN={STEP_LEN} STEP_H={STEP_H} CYCLE={CYCLE_MS}ms")
print("="*80)
print(f"{'腿':8s} {'舵机号':>5s} {'关节':6s} {'zero':>5s} {'最小':>5s} {'最大':>5s} {'摆幅':>5s}")
print("-"*50)
for leg in range(4):
    for j, (name, z) in enumerate([('外展',STAND_POSE[leg][0]),
                                     ('大腿',STAND_POSE[leg][1]),
                                     ('小腿',STAND_POSE[leg][2])]):
        if j == 0:
            vals = [STAND_POSE[leg][0]] * FRAMES
        else:
            vals = data[leg]['th'] if j == 1 else data[leg]['ca']
        servo_n = [1,5,9,2,6,10,3,7,11,4,8,12][leg*3+j]
        print(f"{LEG_NAMES[leg]:8s} {servo_n:5d} {name:6s} {z:5d} {int(min(vals)):5d} {int(max(vals)):5d} {int(max(vals)-min(vals)):5d}")

fig.suptitle('TROT 步态仿真 (舵机角度空间直驱, gait.c 同源算法)', fontsize=14)
plt.tight_layout()
out = 'gait_sim.png'
plt.savefig(out, dpi=120)
print(f'\n已保存: {out}')
