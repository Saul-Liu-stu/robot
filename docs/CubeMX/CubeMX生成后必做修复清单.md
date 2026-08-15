# CubeMX Generate Code 后 —— 必做修复清单

> 每次 CubeMX 点 Generate Code 后按此清单逐项修复，5 分钟搞定。

---

## 一、CubeMX 内已固化（无需操作）

| 项 | CubeMX 配置位置 | 验证 |
|----|---------------|------|
| DCMI PCLK Rising | DCMI → Pixel Clock Polarity: Rising Edge | dcmi.c 第43行 |
| DCMI 引脚 Pull-up | 每个 DCMI 脚 → GPIO Pull-up | dcmi.c 6 处 `GPIO_PULLUP` |
| DCMI DMA Priority High | DCMI → DMA Settings → Priority: High | dcmi.c 第145行 |
| DCMI DMA FIFO HalfFull | DCMI → DMA Settings → FIFO Threshold: Half Full | dcmi.c 第147行 |

> ✅ 以上四项已在 CubeMX 文件中配好，Generate Code 后自动正确，不用再修。

---

## 二、手动修复（CubeMX 改不了的）

### 修复 1：`Core/Inc/dcmi.h` 加 DMA 句柄 extern

CubeMX 生成后，dcmi.h 只有这一行：
```c
extern DCMI_HandleTypeDef hdcmi;
```

**补一行：**
```c
extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef hdma_dcmi;  /* ← 手动加这行 */
```

### 修复 2：`Core/Src/main.c` MPU 配置

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
| dcmi.h 缺 hdma_dcmi | **编译失败**：`hdma_dcmi undeclared` |
| MPU 非缓存 | **画面黑线/卡死/渐变黑**，D-Cache 一致性问题 |
| MPU 缺 0x24000000 区域 | **黑屏** 或 **蓝牙回复乱码**：D1 RAM 可缓存，DMA 读旧数据 |
