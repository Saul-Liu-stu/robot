# CubeMX 蓝牙模块 + 编码器 配置指南

> 在现有工程（DCMI+LCD+OV5640+12舵机+4电机）上追加

---

## 一、USART3 蓝牙 (HC-05, 9600bps)

### 1.1 Connectivity 面板

| 串口 | 模式 | 引脚 | 说明 |
|------|------|------|------|
| **USART3** | Asynchronous | TX: **PB10**, RX: **PB11** | HC-05 交叉接线 |

### 1.2 Configuration

| 参数 | 值 |
|------|-----|
| Baud Rate | **9600** Bits/s |
| Word Length | 8 Bits (including Parity) |
| Parity | None |
| Stop Bits | 1 |
| Data Direction | Receive and Transmit |

### 1.3 NVIC Settings

| 中断 | 状态 |
|------|------|
| USART3 global interrupt | **Enabled** |
| Preemption Priority | 5 |
| Sub Priority | 0 |

---

## 二、编码器引脚 → GPIO_EXTI 中断计数

> LQFP144 封装无 PH10/PH11，改用 EXTI 软件计数。500PPR × 300RPM = 2500脉冲/秒，EXTI 完全够用。

### 2.1 GPIO 配置

| 引脚 | 模式 | 上下拉 | 标签 |
|------|------|--------|------|
| **PE7** | GPIO_EXTI (上升沿+下降沿) | Pull-up | ENC1_A |
| **PE8** | GPIO_Input | Pull-up | ENC1_B |

### 2.2 NVIC Settings

| 中断 | 状态 |
|------|------|
| EXTI line[9:5] interrupts | **Enabled** |
| Preemption Priority | 6 |

> ENC1_A 用双沿中断计数，ENC1_B 在中断里读电平判断方向。

---

## 三、Generate Code 后验证

检查 `Core/Inc/usart.h` 包含：
```c
extern UART_HandleTypeDef huart3;
```

检查 `Core/Inc/gpio.h` 或 `main.h` 包含 PH10/PH11 标签（如果设了 User Label）。

> TIM5 不在 CubeMX 输出中，代码会手动初始化。
