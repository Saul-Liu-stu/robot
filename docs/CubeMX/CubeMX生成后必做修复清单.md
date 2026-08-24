# CubeMX Generate Code 后 —— 必做修复清单

> 每次 CubeMX 点 Generate Code 后按此清单逐项修复，5 分钟搞定。

---

## 一、CubeMX 内已固化（无需操作）

> ⚠️ 2026-08-22 摄像头与 LCD 已移除。本节的 DCMI 固化项**全部废弃**——
> 前提是已在 .ioc 里禁用 DCMI、SPI6(LCD)、SCCB 相关引脚并重新 Generate
> （Generate 后 dcmi.c/dcmi.h/spi.c/spi.h 消失，MX_DCMI_Init/MX_SPI6_Init 调用消失）。
> 若尚未禁用，旧 DCMI 配置仍在但无摄像头硬件，无实际影响。

---

## 二、手动修复（CubeMX 改不了的）

### 修复 1：~~`Core/Inc/dcmi.h` 加 DMA 句柄 extern~~（2026-08-22 废弃）

摄像头移除 + .ioc 禁用 DCMI 后，dcmi.c/h 整个消失，本项**不再需要**。
若 DCMI 仍未从 .ioc 移除，则 Generate 后仍需补 `extern DMA_HandleTypeDef hdma_dcmi;`。

### 修复 2：`Core/Src/main.c` MPU 配置（仍然必做）

> 摄像头移除**不影响**本项：蓝牙 TX 仍走 DMA，D-Cache/DMA 一致性依赖此配置。

CubeMX 生成后 MPU_Config 只有 NUMBER0 且是 cacheable 的。**替换整个函数体：**

```c
void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  HAL_MPU_Disable();

  /* D2 SRAM 区: 非缓存 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* 整个 D1 RAM 0x24000000: 非缓存 (含摄像头帧缓冲 0x24010000)
   * ⚠ 基地址必须按区域大小对齐, 0x24010000 不是512KB对齐会静默失败! */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x24000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 修复 3：工程目录重构后的源文件清单（2026-08-21 起必查）

应用层代码已全部移入 `Hardware/`（leg_ik / walk_gait / motor_control / new_servo / leg_control / encoder / imu / bluetooth_control / drive_ctrl / attitude_control + leg_config.h），`Core/` 只剩 CubeMX 生成文件。

CubeMX Generate Code 后**检查**（否则用户模块全丢，链接报 `undefined reference to 'WalkGait_FootTarget'` 等）：

1. `Debug/Hardware/subdir.mk` 的 C_SRCS / OBJS / C_DEPS / clean 四处应包含上述 10 个模块——缺了手动加回
2. 编译若提示 Hardware 文件未参与构建 → 确认 `.cproject` 里 Hardware 仍是源目录

```bash
# 快速验证 (期望 ≥10)
grep -c "walk_gait\|leg_ik\|attitude_control\|motor_control" Debug/Hardware/subdir.mk
```

### 修复 4：`Debug/Core/Src/subdir.mk` 加 iwdg.c（2026-08-23 起必查）

IWDG 启用后实测 CubeMX **不会**把 iwdg.c 加进 Core 的 subdir.mk → 链接报 `undefined reference to 'MX_IWDG1_Init'`。Generate 后检查四处：

```bash
grep -n "iwdg" Debug/Core/Src/subdir.mk
# 期望: C_SRCS / OBJS / C_DEPS / clean 四处都含 iwdg.c 对应条目
```

IWDG 配置本身（iwdg.c 内 2047 reload ≈2.05s 超时、调试冻结）与 main.c 里 `HAL_IWDG_Refresh`（USER CODE 3 区，Generate 不丢）无需处理。

---

## 三、快速验证命令

在工程根目录执行：

```bash
# 验证 dcmi.h
grep -n "hdma_dcmi" Core/Inc/dcmi.h
# 期望: 有 extern DMA_HandleTypeDef hdma_dcmi;

# 验证 dcmi.c
grep -n "RISING\|PULLUP\|PRIORITY_HIGH\|HALFFULL" Core/Src/dcmi.c
# 期望: 各对应行正确（具体行号见上文"验证"列）

# 验证 MPU
grep -c "24000000\|NOT_CACHEABLE\|NUMBER1" Core/Src/main.c
# 期望: 3（三个关键字都命中）
```

---

## 四、如果忘了修会有什么症状

| 漏修项 | 症状 |
|--------|------|
| ~~dcmi.h 缺 hdma_dcmi~~ | ~~编译失败~~（2026-08-22 DCMI 移除后废弃） |
| MPU 非缓存 | **蓝牙回复乱码**：D1/D2 RAM 可缓存，DMA 读旧数据 |
