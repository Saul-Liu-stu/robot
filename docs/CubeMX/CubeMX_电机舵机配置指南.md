# CubeMX 电机与舵机配置指南

> 在现有工程（含 DCMI + SPI LCD + OV5640）基础上追加  
> 芯片：STM32H743ZIT6 (LQFP144)

---

## 一、定时器配置

### 1.1 舵机 PWM（50Hz）

| 定时器 | 通道 | 引脚 | 舵机 | Prescaler | Counter Period | Auto-reload |
|--------|------|------|------|-----------|--------|--------------|
| **TIM2** | CH1 | PA0 | 1 | 23999 | 199 | Enable |
| | CH2 | PA1 | 9 | | | |
| | CH3 | PA2 | 10 | | | |
| | CH4 | PA3 | 11 | | | |
| **TIM3** | CH2 | PA7 | 2 | 23999 | 199 | Enable |
| | CH3 | PB0 | 5 | | | |
| | CH4 | PB1 | 6 | | | |
| **TIM4** | CH1 | PD12 | 8 | 23999 | 199 | Enable |
| | CH2 | PD13 | 7 | | | |
| | CH3 | PD14 | 4 | | | |
| | CH4 | PD15 | 3 | | | |
| **TIM12** | CH1 | PB14 | 12 | 23999 | 199 | Enable |

> **重要：** `Prescaler = 23999` 基于 **APB1 定时器时钟 = 240MHz**。  
> 如果 SystemClock_Config 中 APB1 Prescaler 不是 `/2`（即 PCLK1 ≠ 120MHz），  
> 定时器时钟会变，Prescaler 需重新计算：`PSC = (TIM_CLK / 10000) - 1`，使计数器频率 = 10kHz。

> **TIM3 CH1 不要开！** PA6 被 DCMI_PIXCLK 占用。只开 CH2/CH3/CH4。

**计算验证：**
```
TIM_CLK = 240MHz (APB1 × 2, 因为 APB1 prescaler ≠ 1)
PSC = 23999 → 计数器 = 240MHz / 24000 = 10kHz
ARR = 199 → PWM 周期 = (199+1) / 10kHz = 20ms = 50Hz ✓
```

### 1.2 电机 PWM（10kHz）

| 定时器 | 通道 | 引脚 | 电机 | Prescaler | Counter Period | 额外设置 |
|--------|------|------|------|-----------|--------|----------|
| **TIM1** | CH1 | PA8 | A | 239 | 99 | BreakDeadTime 全部 Disable |
| | CH2 | PA9 | B | | | |
| **TIM16** | CH1 | PB8 | C | 239 | 99 | — |
| **TIM17** | CH1 | PB9 | D | 239 | 99 | — |

**计算验证：**
```
TIM_CLK = 240MHz
PSC = 239 → 计数器 = 240MHz / 240 = 1MHz
ARR = 99 → PWM 周期 = 100 / 1MHz = 0.1ms = 10kHz ✓
speed 0~100 直接对应 CCR (0% ~ 100% 占空比)
```

---

## 二、GPIO 方向控制引脚

| 引脚 | 标签（User Label） | 用途 |
|------|-------------------|------|
| PE0 | **AIN1** | 电机A 方向 |
| PE1 | **AIN2** | 电机A 方向 |
| PE2 | **BIN1** | 电机B 方向 |
| PE3 | **BIN2** | 电机B 方向 |
| PB5 | **CIN1** | 电机C 方向 |
| PB6 | **CIN2** | 电机C 方向 |
| PB7 | **DIN1** | 电机D 方向 |
| PB12 | **DIN2** | 电机D 方向 |
| PA10 | **STBY** | TB6612 使能 |

> **全部设为：** `GPIO_Output`，推挽输出(Push-Pull)，无上下拉(No pull)，低速(Low)

---

## 三、CubeMX 操作步骤

### 步骤 1：打开 Timers 面板

#### TIM1
```
1. 勾选 Internal Clock
2. Channel1 → PWM Generation CH1
3. Channel2 → PWM Generation CH2
4. Configuration:
   - Prescaler: 239
   - Counter Mode: Up
   - Counter Period: 99
   - Auto-reload preload: Enable
5. Break and Dead-Time settings → 全部设为 Disable
```

#### TIM2
```
1. 勾选 Internal Clock
2. Channel1~4 → PWM Generation CH1~4
3. Prescaler: 23999, Counter Period: 199
4. Auto-reload preload: Enable
```

#### TIM3
```
1. 勾选 Internal Clock
2. Channel2~4 → PWM Generation CH2~4
   ⚠️ 不要开 Channel1！PA6 已分配给 DCMI_PIXCLK
3. Prescaler: 23999, Counter Period: 199
```

#### TIM4
```
1. 勾选 Internal Clock
2. Channel1~4 → PWM Generation CH1~4
3. Prescaler: 23999, Counter Period: 199
```

#### TIM12
```
1. 勾选 Internal Clock
2. Channel1 → PWM Generation CH1
3. Prescaler: 23999, Counter Period: 199
```

#### TIM16
```
1. 勾选 Internal Clock
2. Channel1 → PWM Generation CH1
3. Prescaler: 239, Counter Period: 99
```

#### TIM17
```
1. 勾选 Internal Clock
2. Channel1 → PWM Generation CH1
3. Prescaler: 239, Counter Period: 99
```

### 步骤 2：配置 GPIO Output 引脚

在 Pinout 视图依次搜索并配置：

```
PE0 → GPIO_Output → User Label: AIN1
PE1 → GPIO_Output → User Label: AIN2
PE2 → GPIO_Output → User Label: BIN1
PE3 → GPIO_Output → User Label: BIN2
PB5 → GPIO_Output → User Label: CIN1
PB6 → GPIO_Output → User Label: CIN2
PB7 → GPIO_Output → User Label: DIN1
PB12 → GPIO_Output → User Label: DIN2
PA10 → GPIO_Output → User Label: STBY
```

全部设：推挽输出 / 无上下拉 / 低速 / 初始 Low。

### 步骤 3：确认现有配置没被覆盖

重新 Generate Code 前，确认 DCMI/SPI6/LCD 相关引脚配置仍在。如果 CubeMX 自动修改了，手动恢复。

---

## 四、引脚冲突检查清单

| 资源 | 引脚 | 状态 |
|------|------|------|
| DCMI_D0~D7 | PC6/PC7/PG10/PG11/PE4/PD3/PE5/PE6 | ✓ 不冲突 |
| DCMI_VSYNC | PG9 | ✓ |
| DCMI_HSYNC | PA4 | ✓ |
| DCMI_PIXCLK | PA6 | ✓ TIM3_CH1 已禁用 |
| SCCB_SCL/SDA | PF14/PF15 | ✓ |
| PWDN | PF13 | ✓ |
| SPI6 | PG8/PG13/PG14 | ✓ |
| LCD_BL/DC | PG12/PG15 | ✓ |
| TIM6 (tick) | — (内部) | ✓ |

---

## 五、Generate Code 后验证

生成后检查 `Core/Inc/tim.h` 是否包含：

```c
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim12;
extern TIM_HandleTypeDef htim16;
extern TIM_HandleTypeDef htim17;
```

检查 `Core/Inc/main.h` 是否包含 GPIO 标签：
```c
#define STBY_Pin         GPIO_PIN_10
#define STBY_GPIO_Port   GPIOA
#define AIN1_Pin         GPIO_PIN_0
#define AIN1_GPIO_Port   GPIOE
// ... (共9个GPIO标签)
```
