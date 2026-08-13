"""四相调试 — 每相=3D姿态+3柱状图, 4页竖排"""
import numpy as np, matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt; from mpl_toolkits.mplot3d import Axes3D
plt.rcParams['font.sans-serif']=['SimHei']; plt.rcParams['axes.unicode_minus']=False

SP=[[98,240,153],[94,35,74],[50,135,220],[150,200,30]]
DIR=[[1.0,-1.0,1.0],[-1.0,1.0,-1.0],[-1.0,-1.0,1.0],[1.0,1.0,-1.0]]
SL,SH=40,30; OFF=[0,180,180,0]; L1,L2=130,129
CL=['#d62728','#1f77b4','#2ca02c','#ff7f0e']; LN=['FL','FR','RL','RR']
HX=[205,205,-205,-205]; HY=[-82,82,-82,82]

def gait_delta(lp):
    if lp<180:
        td=SL*(90-lp)//90; cd=SH*lp//90 if lp<=90 else SH*(180-lp)//90; cd=-cd
    else: p=lp-180; td=SL*(p-90)//90; cd=0
    return td,cd

PHASES=[0,90,180,270]; NAMES=['1.抬腿前伸','2.踏地后扫','3.蹬地后推','4.复位前摆']

fig=plt.figure(figsize=(18,22))

for idx,(g_ph,title) in enumerate(zip(PHASES,NAMES)):
    # 3D (左半边)
    ax3d=fig.add_subplot(4,4,idx*4+1,projection='3d')
    bx=[205,205,-205,-205,205]; by=[-82,82,82,-82,-82]
    ax3d.plot(bx,by,[250]*5,'k-',lw=2)

    for leg in range(4):
        lp=(g_ph+OFF[leg])%360; td,cd=gait_delta(lp)
        th_sv=np.clip(int(SP[leg][1]+DIR[leg][1]*td),0,270)
        ca_sv=np.clip(int(SP[leg][2]+DIR[leg][2]*cd),0,270)
        th_deg=-25+td; ca_deg=th_deg+80+cd
        th_r=np.radians(th_deg); ca_r=np.radians(ca_deg)
        hx,hy,hz=HX[leg],HY[leg],250.0
        kx=hx+L1*np.sin(th_r); ky=hy; kz=hz-L1*np.cos(th_r)
        fx=kx+L2*np.sin(ca_r); fy=ky; fz=kz-L2*np.cos(ca_r)
        sw="S" if lp<180 else "D"
        ax3d.plot([hx,kx,fx],[hy,ky,fy],[hz,kz,fz],'-o',ms=3,color=CL[leg],lw=1.5,
                  label=f'{LN[leg]}({sw}) t={th_sv} c={ca_sv}')
    ax3d.set_xlim(-280,280); ax3d.set_ylim(-200,200); ax3d.set_zlim(0,350)
    ax3d.set_title(title,fontsize=11); ax3d.legend(fontsize=5,loc='upper right')

    # 三个柱状图 (右边3列)
    for j,(jname,zero_idx) in enumerate([('Abd外展',0),('Thigh大腿',1),('Calf小腿',2)]):
        ax=fig.add_subplot(4,4,idx*4+2+j)
        xs=[0,1,2,3]; vals=[]; sv_vals=[]
        for leg in range(4):
            lp=(g_ph+OFF[leg])%360; td,cd=gait_delta(lp)
            delta=td if j==1 else (cd if j==2 else 0)
            sv=np.clip(int(SP[leg][zero_idx]+DIR[leg][zero_idx]*delta),0,270)
            sv_vals.append(sv)
            zero=SP[leg][zero_idx]
            # 柱: 以 zero 为底, delta 为高
            bar_val=sv-zero
            ax.bar(xs[leg],bar_val,bottom=zero,width=0.6,color=CL[leg],alpha=0.6)
            ax.axhline(y=zero,color=CL[leg],ls=':',lw=0.8)
            ax.text(xs[leg],sv+1,str(sv),fontsize=8,color=CL[leg],ha='center',va='bottom')
        ax.set_xlim(-0.5,3.5); ax.set_ylim(0,270)
        ax.set_xticks(xs); ax.set_xticklabels(LN,fontsize=7)
        ax.set_title(jname,fontsize=10)

fig.suptitle('Trot 四相动作 — 3D姿态 + 舵机角度',fontsize=15,y=0.995)
plt.tight_layout()
out='trot_4phase_detail.png'
plt.savefig(out,dpi=120)
print(f'已保存: {out}')
