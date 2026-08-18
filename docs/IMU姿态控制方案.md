# 四足舵机机器人 IMU 姿态控制方案（Pitch 版）

> 目标：把当前"纯开环步态"逐步升级为"姿态闭环镇定 → 坡度自适应"。
> 适用硬件：12 路舵机 + 4 路电机（轮式），IMU 为 WT9011G4K（UART 主动上报）。
> 本方案**只使用 pitch 一个维度**；roll/yaw 暂不参与控制。

---

## 1. 背景与现状

### 1.1 当前控制链路

```
步态发生器 (walk_gait.c)
   └─ 足端轨迹 (x, y, z)
        └─ 逆运动学 (leg_ik.c → LegIK_SolveServo)
             └─ 舵机角 (平滑后经 NewServo_BatchControl 输出)
```

整个链路是**开环**的：IMU 已经解析出 roll/pitch/yaw（`imu.c`），但**完全没有参与控制**，仅以 10Hz 上报 QT 上位机做显示。

### 1.2 舵机硬件约束（本方案的核心前提）

- 舵机内部是**位置闭环**，只能发"目标角度"，到位时间约 100~300ms。
- 响应带宽低，**高频修正跟不上**，只会造成抖动甚至发烫。
- 因此本方案所有控制律都遵循：**低频、慢速、小量、平滑过渡**。

### 1.3 现有可复用设施

| 设施 | 位置 | 用途 |
|------|------|------|
| `foot_x_shift` | `walk_params_t`（默认 15mm） | 足端前移修正，站立/行走统一生效 |
| `StandFootX()` | `main.c` | 站立时按站高计算足端 x |
| `out->x += foot_x_shift` | `walk_gait.c` L66 | 行走足端 x 叠加前移修正 |
| 5ms 控制节拍 | `main.c` | IK 解算 + 指数平滑 + 批量输出 |

**关键点**：`foot_x_shift` 同时被站立（`StandFootX`）和行走（`walk_gait.c`）使用，是 Pitch 镇定最理想的注入点——一处修改，两种模式同时生效。

### 1.4 为什么只用 pitch

| 维度 | 是否使用 | 说明 |
|------|---------|------|
| **pitch** | ✅ 核心 | 承担静态镇定、行走镇定、坡度检测三层 |
| roll | ❌ 暂缓 | 横向镇定收益有限，舵机响应慢，后续需要再启用 |
| yaw | ❌ 不用 | 长期漂移，舵机机器人现阶段用不上航向锁定 |

---

## 2. 总体架构

新增一个独立模块 `attitude_control.h/.c`，把姿态控制逻辑从 `main.c` 抽出，避免业务耦合：

```
IMU 数据 (imu.c 解析, 仅取 pitch)
   │
   └─ AttCtrl_CalibTick()  零偏标定（上电一次性）
        │
        └─ AttCtrl_Update()
             ├─ 静态镇定：pitch 差值 → foot_x_shift 修正
             ├─ 行走镇定：pitch 误差低通
             └─ 坡度自适应：pitch 长期均值 → 参数切换
                  │
                  └─ 修正量注入 IK 解算前
```

### 2.1 模块接口设计

```c
/* attitude_control.h */
typedef struct {
    /* Pitch 镇定 */
    float kp_pitch;          /* pitch 比例增益 (mm/deg) */
    float pitch_target;      /* 相对目标 pitch, 默认 0 = 保持标定平衡位 */
    float pitch_offset;      /* 零偏 (上电标定得到, 运行时只读) */
    float foot_x_min;        /* 修正后 foot_x_shift 下限 (mm) */
    float foot_x_max;        /* 修正后 foot_x_shift 上限 (mm) */

    /* 行走低通 */
    float walk_lpf_alpha;    /* 低通系数 (0~1), 越小越平滑 */

    /* 坡度自适应 */
    float slope_lpf_alpha;   /* 坡度检测低通系数 (极慢) */
    float slope_up_thr;      /* 上坡判定阈值 (deg) */
    float slope_dn_thr;      /* 下坡判定阈值 (deg) */
} att_ctrl_params_t;

extern att_ctrl_params_t g_att;

/* 零偏标定状态机: 每 5ms 调用一次, 采样均值作为平衡位 */
void    AttCtrl_CalibTick(const IMU_Angle *imu);

/* 是否已完成标定 (完成前镇定输出保持冻结) */
uint8_t AttCtrl_CalibDone(void);

/* 手动重标定 (蓝牙命令触发) */
void    AttCtrl_Recalibrate(void);

/* 计算修正量, 每 5ms 调用一次 */
void AttCtrl_Update(const IMU_Angle *imu,
                    float *out_x_shift,    /* 叠加到 foot_x_shift */
                    int   *slope_state);   /* -1 下坡 0 平 1 上坡 */
```

---

## 3. 零偏标定与差值计算（一切 pitch 控制的前提）

### 3.1 为什么不能用绝对 0°

机器人"自然站立平衡"时的 IMU 读数几乎不可能是 0°，原因：

- 机械装配误差、腿长不一致、脚垫/轮子高度差异
- 重心偏前（`foot_x_shift=15mm` 本身就是为重心偏前设的，说明静止平衡时 pitch 不为 0）
- 放置地面本就不平

若把目标角写死为 0°，机器人在正确平衡位（IMU 读到 +2° 等）时会误判自己前倾，
持续错误修正，结果反而站歪、越推越偏。

### 3.2 做法：标定零偏 + 差值计算

误差统一用"当前值 − 零偏 − 相对目标"：

```c
float pitch_err = (imu->pitch - g_att.pitch_offset) - g_att.pitch_target;
```

- `pitch_offset` = 标定时采样得到的平衡位 pitch
- `pitch_target` = 相对目标（默认 0，即"回到标定平衡位"）
- 想让机器人主动前倾/后倾时，只改这个相对目标即可，物理意义清晰

### 3.3 标定流程

上电进入站立模式、机器人四足着地静止后，采样 2 秒取均值：

```c
/* 状态机式采样, 放 5ms 循环, 不阻塞 */
static uint32_t s_calib_n = 0;
static float    s_calib_sum = 0;
static uint8_t  s_calibrating = 1;

void AttCtrl_CalibTick(const IMU_Angle *imu)
{
    if (!s_calibrating) return;
    s_calib_sum += imu->pitch;
    if (++s_calib_n >= 400) {              /* 400 × 5ms = 2 秒 */
        g_att.pitch_offset = s_calib_sum / (float)s_calib_n;
        s_calibrating = 0;
    }
}
```

### 3.4 注意点

1. **标定时必须四足着地、静止、无人扶**，否则扰动被平均进去，零偏就不准。
2. **每次上电重新标定**即可，无需断电保存（开机姿态会因地形/负载变化而变，重标更准）。
3. **标定期间冻结镇定输出**（`foot_x_shift` 保持默认 15mm），标定完成再启用。
4. **留一个蓝牙命令**（如 `Z:`）触发 `AttCtrl_Recalibrate()`，换地面或感觉零偏漂了可手动重标。

---

## 4. 第一层：静态镇定（站立模式）

### 4.1 目标

站立时推一下机器人，它能自动把脚往前/后挪，把身体撑回水平。

### 4.2 原理

Pitch 差值 → 动态修正 `foot_x_shift`。当前 `foot_x_shift` 是常量 15mm，改为：

```
foot_x_shift = base_x_shift + Kp_pitch * pitch_err
             = 15.0f + Kp_pitch * ((pitch - pitch_offset) - pitch_target)
```

- 身体前倾（pitch 增大）→ 足端往前挪更多 → 把身体"推"回来
- 身体后仰（pitch 减小）→ 足端往后收 → 把身体"拉"回来

### 4.3 注入点

`main.c` 的 5ms 循环中，IK 解算前，把 `StandFootX()` 里用到的
`g_walk_params.foot_x_shift` 替换为动态值：

```c
/* 在 for(leg) 循环前计算一次 */
float att_x_shift = 0;
int   slope_state = 0;
AttCtrl_Update(IMU_GetAngle(), &att_x_shift, &slope_state);
g_walk_params.foot_x_shift = 15.0f + att_x_shift;   /* 注入 */
```

> `StandFootX()` 和 `walk_gait.c` 都通过 `g_walk_params.foot_x_shift` 取修正值，
> 因此这里一处赋值，站立与行走统一生效。

### 4.4 限幅

防止足端修正跑出可达范围：

```
foot_x_min = -10mm    （不能收得太靠里）
foot_x_max = +60mm    （不能伸得太远，避免 IK 钳位）
```

`kp_pitch` 初值 **0.5 mm/deg**，之后逐步加大到 1.0~2.0。

---

## 5. 第二层：行走镇定（Trot 中启用）

### 5.1 与静态镇定的关系

实际上**代码零改动即可继承**：行走足端在 `walk_gait.c` 里已经
`out->x += g_walk_params.foot_x_shift`，而第一层已对 `foot_x_shift` 做动态赋值。
因此 trot 行走时自然带上镇定。

### 5.2 新增问题：步态周期性点头

行走时 pitch 会随支撑相/摆动相切换**周期性波动**（正常节拍，非倾覆）。若直接
用原始 pitch 做修正，会被每一步的点头带着走，导致足端周期抖动，浪费舵机且恶化步态。

### 5.3 解决：对 pitch_error 做一阶低通

```c
/* 状态量: 低通后的 pitch_error */
static float s_pitch_err_f = 0;
float pitch_err = (imu->pitch - g_att.pitch_offset) - g_att.pitch_target;
s_pitch_err_f += g_att.walk_lpf_alpha * (pitch_err - s_pitch_err_f);
```

- 站立模式：`walk_lpf_alpha` 取较大值（如 0.3），响应相对快（站立没有步态波动）
- 行走模式：`walk_lpf_alpha` 取较小值（如 0.05~0.1），只响应持续性倾斜

> 可通过 `g_mode`（`MODE_STAND` / `MODE_TROT`）在 `AttCtrl_Update` 内动态切换系数，
> 或直接统一用一个较慢的低通 + 相对略大的 Kp 折中。

---

## 6. 第三层：坡度自适应

### 6.1 目标

识别上坡/下坡，自动切换步态参数。坡度是**最低频**信号，最舵机友好。

### 6.2 坡度判断

对 **pitch 差值**（`pitch - pitch_offset`）做**长周期平均**，剔除步态点头，与阈值比较：

```c
/* 极慢低通, ~2 秒时间常数 */
static float s_slope = 0;
float flat = imu->pitch - g_att.pitch_offset;   /* 相对平衡位的 pitch */
s_slope += g_att.slope_lpf_alpha * (flat - s_slope);   /* alpha ~0.002 */

int slope = 0;
if (s_slope > g_att.slope_up_thr)    slope =  1;   /* 上坡 */
else if (s_slope < g_att.slope_dn_thr) slope = -1; /* 下坡 */
```

阈值建议：`slope_up_thr = +5°`，`slope_dn_thr = -5°`，并加**迟滞**（跨过阈值后
要回落 1~2° 才切换，避免在边界反复横跳）。

> 注意：坡度判断用**相对零位的 pitch**（减去 `pitch_offset`），否则零偏会污染坡度读数。

### 6.3 参数映射

| 场景 | `step_h`（抬腿） | `step_len`（步幅） | `stand_h`（站高） | `duty`（支撑比） |
|------|------------------|--------------------|--------------------|------------------|
| 上坡 | 增大 (+10mm) | 减小 (-15mm) | 不变 | 增大 (+0.05) |
| 下坡 | 不变 | 减小 (-10mm) | 降低 (-15mm) | 不变 |
| 平地 | 恢复默认 | 恢复默认 | 恢复默认 | 恢复默认 |

### 6.4 实现要点

- 用 `MODE_STAND`/`MODE_TROT` 切换时**不触发坡度调整**，只在行走中生效。
- 参数切换也要**平滑过渡**（`g_walk_params` 已是运行时变量，可逐步逼近目标值），
  避免 `stand_h` 突变导致腿部跳变。
- 坡度状态通过 `slope_state` 返回，供上位机显示、日志或后续扩展。

---

## 7. 参数汇总表

| 参数 | 初值 | 范围 | 说明 |
|------|------|------|------|
| `kp_pitch` | 0.5 mm/deg | 0~3 | pitch 比例增益 |
| `pitch_target` | 0°（相对） | -15~+15 | 相对平衡位的目标俯仰 |
| `pitch_offset` | 标定值 | 自动 | 上电标定得到的平衡位 pitch |
| `foot_x_min` | -10 mm | — | foot_x_shift 下限 |
| `foot_x_max` | +60 mm | — | foot_x_shift 上限 |
| `walk_lpf_alpha` | 站立 0.3 / 行走 0.08 | 0~1 | 行走/站立低通系数 |
| `slope_lpf_alpha` | 0.002 | — | 坡度低通系数（约 2s） |
| `slope_up_thr` | +5° | — | 上坡阈值 |
| `slope_dn_thr` | -5° | — | 下坡阈值 |

---

## 8. 调优流程（按序执行）

1. **零偏标定**：上电站稳后确认 `pitch_offset` 已正确写入（可用上位机观察回传的
   pitch 是否趋近 0）。

2. **静态镇定**：站立模式，用手轻推机器人。从小 `kp_pitch=0.3` 开始，逐步加大，
   出现来回晃动即回退到晃动增益的 60%~70%。验证"推一下能自己撑住"。

3. **行走镇定**：进入 trot，观察身体点头是否被抑制。若足端周期抖动，调小
   `walk_lpf_alpha`（更平滑）。若对持续倾斜反应太慢，调大 `kp_pitch` 或 `alpha`。

4. **坡度自适应**：在斜坡（或垫高木板）上实测，观察 `slope_state` 是否正确翻转、
   步态参数是否平滑切换、站高切换是否引起腿部跳变。

---

## 9. 实施步骤 Checklist

- [ ] 新建 `attitude_control.h` / `attitude_control.c`
- [ ] 实现零偏标定（`AttCtrl_CalibTick` / `AttCtrl_CalibDone` / `AttCtrl_Recalibrate`）
- [ ] `main.c` 5ms 循环：IK 前调用 `AttCtrl_Update`，注入 `foot_x_shift`
- [ ] 标定期间冻结镇定输出，完成后启用
- [ ] 验证第一层：站立推推自稳
- [ ] 验证第二层：trot 行走镇定 + 低通
- [ ] 实现第三层：坡度低通 + 迟滞 + 参数平滑切换
- [ ] 坡面实测，记录并微调阈值
- [ ] （可选）新增蓝牙命令 `Z:` 触发手动重标定，及 `kp_pitch` 在线调参

---

## 10. 备注

- 本方案**刻意不引入**角速度 D 项、互补滤波/四元数融合等高频手段，因为舵机响应
  带宽支撑不了，收益为负。
- **roll 暂缓**：横向镇定收益有限且舵机响应慢，等 pitch 三个层次跑通并满意后，
  若确有侧倾问题，再按"外展偏置 + slew + 限幅"的思路单独启用（可另起文档）。
- 本方案只需 pitch，现有 `imu.c` 已解析 `IMU_Angle.pitch`，**无需扩展 IMU 解析**。
- 所有增益、限幅、阈值集中到 `g_att`（`att_ctrl_params_t`），便于在线调参与后续
  蓝牙命令接入（参照现有 `X:` 命令的模式新增对应调参命令）。