"""步态仿真 v3 — 二维IK + 足端轨迹 (与 gait.c/ik2d.c 一致)"""
import numpy as np
import matplotlib; matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d import Axes3D

plt.rcParams['font.sans-serif']=['SimHei']; plt.rcParams['axes.unicode_minus']=False

# ===== 实测几何 (docs/硬件/2DIK测量清单.md) =====
L1 = [130.0]*4
L2 = [180.0]*4
TH_ZERO = [212.0, 65.0, 110.0, 230.0]
CA_ZERO = [ 93.0,135.0, 165.0,  90.0]
TH_DIR  = [-1.0, 1.0, -1.0, 1.0]
CA_DIR  = [ 1.0,-1.0,  1.0, -1.0]

# ===== 步态参数 (leg_config.h) =====
HIP_H = 300.0
S_LEN = 60.0
S_LIFT = 30.0
CYC = 800
OFF = [0, 180, 180, 0]     # TROT
DT = 0.01
CL = ['#d62728','#1f77b4','#2ca02c','#ff7f0e']; LN = ['FL','FR','RL','RR']
HIPX = [185, 185, -185, -185]
HIPY = [-82, 82, -82, 82]

def ik2d(leg, x, z):
    """与 ik2d.c 一致"""
    l1, l2 = L1[leg], L2[leg]
    dmax = (l1+l2)*0.999
    d = np.sqrt(x*x+z*z)
    if d > dmax:
        sc = dmax/d; x*=sc; z*=sc; d=dmax
    cq3 = (d*d-l1*l1-l2*l2)/(2*l1*l2)
    cq3 = max(-1, min(1, cq3))
    q3 = np.arccos(cq3)
    q2 = np.arctan2(x, z) - np.arctan2(l2*np.sin(q3), l1+l2*np.cos(q3))
    q2d, q3d = np.degrees(q2), np.degrees(q3)
    th = int(np.clip(TH_ZERO[leg] + TH_DIR[leg]*q2d, 0, 270))
    ca = int(np.clip(CA_ZERO[leg] + CA_DIR[leg]*q3d, 0, 270))
    return th, ca

def foot_target(lp):
    """与 gait.c 一致"""
    if lp < 180:
        p = lp
        x = -S_LEN/2 + S_LEN*p/180
        z = HIP_H - S_LIFT*np.sin(np.pi*p/180)
    else:
        p = lp - 180
        x = S_LEN/2 - S_LEN*p/180
        z = HIP_H
    return x, z

def servo_geom(leg, th, ca):
    """舵机角 → 几何角(deg), 用于3D画腿"""
    q2 = (th - TH_ZERO[leg]) / TH_DIR[leg]
    q3 = (ca - CA_ZERO[leg]) / CA_DIR[leg]
    return np.radians(q2), np.radians(q3)

def pose_servo(phase):
    th=[0]*4; ca=[0]*4
    for leg in range(4):
        lp=(phase+OFF[leg])%360
        x,z=foot_target(lp)
        th[leg],ca[leg]=ik2d(leg,x,z)
    return th,ca

def lerp(a,b,t): return [int(a[i]+(b[i]-a[i])*t) for i in range(len(a))]

# ===== 时间线 (快放: 帧数精简) =====
stand_th, stand_ca = [],[]
for leg in range(4):
    t,c = ik2d(leg, 0.0, HIP_H)
    stand_th.append(t); stand_ca.append(c)

PHASES=[0,90,180,270]
NAMES=['动作1','动作2','动作3','动作4']
frames=[]
for k in range(30): frames.append((stand_th[:],stand_ca[:],0,'G: 站姿'))
cur_th,cur_ca=stand_th[:],stand_ca[:]
for i,ph in enumerate(PHASES):
    tg_th,tg_ca=pose_servo(ph)
    for k in range(15):
        t=k/15
        frames.append((lerp(cur_th,tg_th,t),lerp(cur_ca,tg_ca,t),ph,'A%d: %s'%(i+1,NAMES[i])))
    cur_th,cur_ca=tg_th[:],tg_ca[:]
    for k in range(25): frames.append((cur_th[:],cur_ca[:],ph,'保持 %s'%NAMES[i]))
# 连续 Trot: 真实速度 (GAIT_CYCLE_MS=800 → 每帧 4.5°), 10ms/帧 实时播放
g_ph=0.0
for k in range(800):
    g_ph=(g_ph+360.0*DT/(CYC/1000.0))%360
    th,ca=pose_servo(int(g_ph))
    frames.append((th,ca,int(g_ph),'T: 连续 Trot (实时 800ms/周期)'))

def update(f):
    th,ca,ph,label=frames[f]
    ax3d.cla(); ax_th.cla(); ax_ca.cla(); ax_ph.cla()
    for leg in range(4):
        q2,q3=servo_geom(leg,th[leg],ca[leg])
        hx,hy,hz=HIPX[leg],HIPY[leg],0.0
        kx=hx+L1[leg]*np.sin(q2); kz=-L1[leg]*np.cos(q2)
        fx=kx+L2[leg]*np.sin(q2+q3); fz=kz-L2[leg]*np.cos(q2+q3)
        # 3D: z向上为正, 身体在z=0, 地面在z=-HIP_H
        ax3d.plot([hx,kx,fx],[hy,hy,hy],[hz,kz,fz],'-o',ms=3,color=CL[leg],lw=1.5,label=LN[leg])
        ax_th.bar(leg*2,th[leg]-TH_ZERO[leg],bottom=TH_ZERO[leg],width=1.5,color=CL[leg],alpha=0.5)
        ax_th.axhline(y=TH_ZERO[leg],color=CL[leg],ls=':',lw=0.8)
        ax_th.text(leg*2+0.7,th[leg]+2,str(th[leg]),fontsize=9,color=CL[leg],ha='center')
        ax_ca.bar(leg*2,ca[leg]-CA_ZERO[leg],bottom=CA_ZERO[leg],width=1.5,color=CL[leg],alpha=0.5)
        ax_ca.axhline(y=CA_ZERO[leg],color=CL[leg],ls=':',lw=0.8)
        ax_ca.text(leg*2+0.7,ca[leg]+2,str(ca[leg]),fontsize=9,color=CL[leg],ha='center')
        lp=(ph+OFF[leg])%360
        sw = 0 if label.startswith('G') else (1 if lp<180 else 0)
        ax_ph.bar(leg,sw,width=0.8,color=CL[leg],alpha=0.7)
    bx=[185,185,-185,-185,185]; by=[-82,82,82,-82,-82]
    ax3d.plot(bx,by,[0]*5,'k-',lw=2)
    # 地面线
    ax3d.plot([-250,250],[0,0],[-HIP_H,-HIP_H],'g--',lw=0.8,alpha=0.5)
    ax3d.set_xlim(-300,300); ax3d.set_ylim(-150,150); ax3d.set_zlim(-HIP_H-60,60)
    ax3d.set_title("%s  t=%.1fs"%(label,f*DT)); ax3d.legend(fontsize=7)
    ax_th.set_xlim(-1,8); ax_th.set_ylim(0,270); ax_th.set_title('Thigh servo'); ax_th.set_xticks([0.7,2.7,4.7,6.7]); ax_th.set_xticklabels(LN)
    ax_ca.set_xlim(-1,8); ax_ca.set_ylim(0,270); ax_ca.set_title('Calf  servo'); ax_ca.set_xticks([0.7,2.7,4.7,6.7]); ax_ca.set_xticklabels(LN)
    ax_ph.set_xlim(-0.5,3.5); ax_ph.set_ylim(-0.1,1.2); ax_ph.set_title('Swing(high) Stance(low)'); ax_ph.set_xticks([0,1,2,3]); ax_ph.set_xticklabels(LN)

fig=plt.figure(figsize=(14,10))
ax3d=fig.add_subplot(2,2,1,projection='3d')
ax_th=fig.add_subplot(2,2,2); ax_ca=fig.add_subplot(2,2,3); ax_ph=fig.add_subplot(2,2,4)
ani=FuncAnimation(fig,update,frames=len(frames),interval=DT*1000,repeat=True)
plt.show()
