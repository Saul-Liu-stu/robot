# -*- coding: utf-8 -*-
"""
PC 步态仿真可视化: 在屏幕上看机器人走路
用法:
    python visualize.py            # 保存 4 帧快照 sim_snapshot.png + 自检
    python visualize.py --show     # 弹窗播放行走动画
自检: 对全程所有足端目标做 IK->FK 回代, 打印最大误差 (应 < 0.01mm)
"""
import sys
import math
import numpy as np
import matplotlib
if '--show' not in sys.argv:
    matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # py3.7 需要显式 import
from matplotlib import font_manager
from matplotlib.animation import FuncAnimation

for fp in [r'C:\Windows\Fonts\msyh.ttc', r'C:\Windows\Fonts\simhei.ttf']:
    try:
        font_manager.fontManager.addfont(fp)
    except Exception:
        pass
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei']
plt.rcParams['axes.unicode_minus'] = False

import robot_config as RC
from leg_ik import ik, leg_segments
from gait import foot_target

DT = 0.005                      # 5ms 一拍, 与固件控制周期一致
TS = np.arange(0, RC.PERIOD * 2 + 1e-9, DT)
GROUND = RC.STAND_H             # 地面 z (FRD 向下为正)


def solve_all(t):
    """某时刻: 四条腿的目标点/关节角/腿段(机身系坐标)"""
    legs = []
    for i in range(4):
        d = RC.D_SIGN[i] * RC.LEG_HIP_D
        tgt = foot_target(i, t, d)
        q1, q2, q3, st = ik(*tgt, d)
        segs = leg_segments(q1, q2, q3, d)      # 腿系
        # 腿系 -> 机身系: 平移髋安装位置
        segs = [(p[0] + RC.HIP_X[i], p[1] + RC.HIP_Y[i], p[2]) for p in segs]
        tgt_b = (tgt[0] + RC.HIP_X[i], tgt[1] + RC.HIP_Y[i], tgt[2])
        legs.append((tgt_b, segs, st))
    return legs


def self_check():
    """全程 IK->FK 回代误差检查"""
    from leg_ik import fk
    max_err = 0.0
    n_warn = 0
    for t in TS:
        for i in range(4):
            d = RC.D_SIGN[i] * RC.LEG_HIP_D
            tgt = foot_target(i, t, d)
            q1, q2, q3, st = ik(*tgt, d)
            f = fk(q1, q2, q3, d)
            err = math.sqrt((tgt[0]-f[0])**2 + (tgt[1]-f[1])**2 + (tgt[2]-f[2])**2)
            max_err = max(max_err, err)
            if st != 0:
                n_warn += 1
    print(f'[自检] 全程 {len(TS) * 4} 个足端目标: IK->FK 最大误差 {max_err:.6f} mm, '
          f'不可达/超限 {n_warn} 次')
    ok = max_err < 0.01 and n_warn == 0
    print('[自检] ' + ('通过 [OK]' if ok else '未通过 [FAIL] —— 检查参数或步态范围'))
    return ok


def draw(ax, t):
    ax.cla()
    ax.set_title(f'轮足机器人步态仿真 (trot)   t = {t:.2f} s')
    ax.set_xlabel('x 前 (mm)')
    ax.set_ylabel('y 右 (mm)')
    ax.set_zlabel('z 上 (mm)')

    # 地面网格 (显示时 z 取负, 让"上"朝上)
    gx, gy = np.meshgrid(np.linspace(-220, 220, 9), np.linspace(-180, 180, 7))
    ax.plot_wireframe(gx, gy, np.full_like(gx, -GROUND), color='#dddddd', lw=0.6)

    # 机身矩形
    bl, bw = RC.BODY_BL / 2, RC.BODY_BW / 2
    bx = [bl, bl, -bl, -bl, bl]
    by = [-bw, bw, bw, -bw, -bw]
    ax.plot(bx, by, [0] * 5, 'k-', lw=2)
    ax.plot([bl + 25], [0], [0], marker='>', color='k')   # 机头方向

    colors = ['#d62728', '#1f77b4', '#2ca02c', '#ff7f0e']
    for i, (tgt_b, segs, st) in enumerate(solve_all(t)):
        xs = [p[0] for p in segs]
        ys = [p[1] for p in segs]
        zs = [-p[2] for p in segs]      # z 取负显示
        ax.plot(xs, ys, zs, '-o', ms=4, color=colors[i], lw=2, label=RC.LEG_NAMES[i])
        ax.plot([tgt_b[0]], [tgt_b[1]], [-tgt_b[2]], 'x', color=colors[i], ms=6)

    ax.set_xlim(-240, 240)
    ax.set_ylim(-200, 200)
    ax.set_zlim(-GROUND - 30, 60)
    ax.legend(loc='upper right', fontsize=8)
    ax.view_init(elev=18, azim=-60)


def save_snapshot():
    fig = plt.figure(figsize=(11, 8))
    phases = [0.0, 0.25, 0.5, 0.75]
    for k, ph in enumerate(phases):
        ax = fig.add_subplot(2, 2, k + 1, projection='3d')
        draw(ax, RC.PERIOD + ph * RC.PERIOD)    # 取第二个周期, 相位干净
        ax.set_title(f'相位 {ph:.2f} (t={RC.PERIOD * (1 + ph):.2f}s)', fontsize=10)
    plt.tight_layout()
    out = 'sim_snapshot.png'
    plt.savefig(out, dpi=100)
    print(f'快照已保存: {out}')


def play():
    fig = plt.figure(figsize=(9, 7))
    ax = fig.add_subplot(111, projection='3d')
    anim = FuncAnimation(fig, lambda k: draw(ax, TS[k]),
                         frames=len(TS), interval=DT * 1000, repeat=True)
    plt.show()


if __name__ == '__main__':
    ok = self_check()
    save_snapshot()
    if '--show' in sys.argv:
        play()
    sys.exit(0 if ok else 1)
