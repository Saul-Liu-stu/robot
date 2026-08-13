# 四足机器人 robot_four_leg

基于 STM32H743 的四足机器人项目，包含固件、Qt 蓝牙控制端、Python 步态仿真工具与完整硬件文档。

## 硬件平台

| 模块 | 型号 / 数量 | 接口 |
|------|------------|------|
| 主控 | STM32H743ZIT6 (LQFP144) @ 480MHz (HSI→PLL) | — |
| 舵机 | 12× 270°（4 腿 × 外展/大腿/小腿） | TIM2/3/4/12 PWM 50Hz |
| 直流电机 | 4× TB6612（腿部推进） | TIM1/16/17 PWM 10kHz |
| 编码器 | 4× GMR 500PPR（1:28 减速） | EXTI 双沿计数 |
| IMU | WT9011G4K | USART2 460800 |
| 蓝牙 | HC-05（SPP） | USART3 115200 |
| 摄像头 | OV5640（RGB565） | DCMI 8-bit + DMA |
| 显示屏 | 1.54" ST7789V 240×240 | SPI6 |

完整引脚分配见 `docs/硬件/控制板引脚分配表.md`。

## 项目结构

```
robot_four_leg/
├── Core/           # STM32 用户代码（步态、IK、电机、舵机、IMU、蓝牙、摄像头）
├── Drivers/        # CMSIS + STM32H7xx HAL 库
├── Hardware/       # 外设驱动（LCD、OV5640、SCCB）
├── QT_APP/         # Qt 蓝牙控制端（Windows/Android）
├── tools/          # Python 步态仿真与调参脚本
├── docs/           # 项目文档
└── robot_four_leg.ioc   # CubeMX 工程配置
```

## 软件环境

| 部分 | 工具 | 说明 |
|------|------|------|
| 固件 | STM32CubeIDE | Import 工程后编译烧录 |
| 控制端 | Qt 5.x + Android SDK | Qt Creator 打开 `QT_APP/robot_ble_app.pro` |
| 仿真 | Python 3 | `tools/` 下脚本，依赖 numpy/matplotlib |

## 快速开始

1. **固件**：STM32CubeIDE → `File → Import → Existing Projects into Workspace` → 编译烧录
2. **控制端**：Qt Creator 打开 `QT_APP/robot_ble_app.pro`；Android 构建产物生成于 `QT_APP/build_apk/`（不提交仓库）
3. **步态仿真**：
   ```bash
   pip install numpy matplotlib
   python tools/gait_sim.py
   ```

## 通信协议

蓝牙 SPP（HC-05，115200 bps）v3.0：

| 命令 | 功能 |
|------|------|
| `G` | 站立（12 舵机回到标准站姿） |
| `T` | Trot 对角小跑 |
| `A` | 单步前进（调试用，按相位推进 90°） |
| `1`~`12` | 舵机校准模式 |

完整协议见 `docs/协议/蓝牙通信协议.md`。

## 文档索引

| 目录 | 内容 |
|------|------|
| `docs/CubeMX/` | CubeMX 配置指南与生成后修复清单 |
| `docs/协议/` | 蓝牙通信协议 |
| `docs/硬件/` | 引脚分配、舵机标定、移植文档 |
| `docs/算法/` | 步态实现、IK、PID 调试 |
| `docs/协作开发指南.md` | **协作开发规范（Fork + PR 工作流），开工前必读** |

## 协作开发

本项目采用一人一仓库（Fork + Pull Request）模式，具体流程见 `docs/协作开发指南.md`。

- 开发前先同步主仓库，禁止直接 push 主仓库
- 提交信息格式：`类型: 描述`（feat/fix/docs/refactor/test）
- 编译产物（`Debug/`、`build_apk/`、APK 等）已被 `.gitignore` 排除，不要提交
