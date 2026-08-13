# CubeMX WT9011G4K IMU 配置指南

> 9 轴 IMU，UART 主动上报，**当前波特率 9600**

---

## USART2 IMU

### Connectivity

| 串口 | 模式 | 引脚 | 说明 |
|------|------|------|------|
| **USART2** | Asynchronous | TX: **PD5**, RX: **PD6** | IMU TX → PD6(RX) |

### Configuration

| 参数 | 值 |
|------|-----|
| Baud Rate | **9600** Bits/s |
| Word Length | 8 Bits |
| Parity | None |
| Stop Bits | 1 |

### NVIC

| 中断 | 状态 |
|------|------|
| USART2 global interrupt | **Enabled** |
| Preemption Priority | 5 |

---

## 引脚冲突检查

```
新增: PD5/PD6 (USART2)
现有: DCMI(D0~D7)/SPI6/舵机/电机/编码器/USART3 — 全部不冲突 ✓
```

---

## Generate Code 后验证

`Core/Inc/usart.h` 应新增：
```c
extern UART_HandleTypeDef huart2;
void MX_USART2_UART_Init(void);
```

---

## 接线

```
WT9011G4K TX  → PD6 (USART2_RX)
              RX  → PD5 (USART2_TX)（可选，配置模块用）
              VCC → 3.3V
              GND → GND
```
