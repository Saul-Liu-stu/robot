# OV5640 摄像头模块使用手册

## 文件结构

```
Hardware/
├── camera.h            # 摄像头封装层（主接口）
├── camera.c            # 摄像头封装层实现
├── dcmi_ov5640.h       # OV5640 底层驱动头文件
├── dcmi_ov5640.c       # OV5640 底层驱动（SCCB通信、寄存器配置）
├── dcmi_ov5640_cfg.h   # OV5640 寄存器配置表
├── sccb.h              # SCCB（I2C）软件模拟头文件
├── sccb.c              # SCCB 实现
├── lcd_spi_154.h       # ST7789 LCD 驱动头文件
└── lcd_spi_154.c       # ST7789 LCD 驱动
```

## 数据流架构

```
OV5640 传感器（2592×1944 最大）
      │
      ▼
   ISP 窗口 1280×960 (4:3)
      │
      ▼
   Set_Framesize 640×480 (VGA, 4:3)
      │
      ▼
   DCMI 硬件裁剪 ──→ 捕获 480×360 中心区域（4:3, RGB565）
      │
      ▼
   DMA 连续传输 ──→ 帧缓冲 0x24010000（D1 SRAM, 非缓存）
      │
      ├──→ LCD 预览：软件裁中心 240×240（逐行 SPI）
      │
      └──→ 视觉算法：直接读 0x24010000 获取完整 480×360 图像
```

## 采集参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 采集分辨率 | 480×360 | 4:3，camera.h 中 `CAM_WIDTH`/`CAM_HEIGHT` |
| 像素格式 | RGB565 | 每像素 2 字节 |
| 帧缓冲地址 | 0x24010000 | D1 AXI SRAM，MPU 设为非缓存 |
| 缓冲大小 | ~337KB | 480×360×2 |
| 通讯接口 | DCMI（8位并行） | 硬件同步模式 |
| 控制接口 | SCCB（软件 I2C） | SCL=PF14, SDA=PF15 |
| 掉电控制 | PWDN=PF13 | 低电平=摄像头工作 |

## 硬件连接

| 信号 | 引脚 | 说明 |
|------|------|------|
| DCMI_D0 | PC6 | 数据线 bit0 |
| DCMI_D1 | PC7 | 数据线 bit1 |
| DCMI_D2 | PG10 | 数据线 bit2 |
| DCMI_D3 | PG11 | 数据线 bit3 |
| DCMI_D4 | PE4 | 数据线 bit4 |
| DCMI_D5 | PD3 | 数据线 bit5 |
| DCMI_D6 | PE5 | 数据线 bit6 |
| DCMI_D7 | PE6 | 数据线 bit7 |
| DCMI_VSYNC | PG9 | 垂直同步 |
| DCMI_HSYNC | PA4 | 水平同步 |
| DCMI_PIXCLK | PA6 | 像素时钟 |
| SCCB_SCL | PF14 | I2C 时钟（OD输出） |
| SCCB_SDA | PF15 | I2C 数据（OD输出） |
| PWDN | PF13 | 掉电控制（低=工作） |
| LCD_SPI | SPI6 | PG8(NSS)/PG13(SCK)/PG14(MOSI) |
| LCD_BL | PG12 | 背光 |
| LCD_DC | PG15 | 数据/指令选择 |

> **重要：摄像头模块功耗较高（200-300mA），必须接两根供电线，否则会反复复位！**

## API 参考

### `int Camera_Init(void)`

初始化整个摄像头系统（LCD + OV5640 + 自动对焦 + DCMI 连续采集）。

```c
if (Camera_Init() != 0) {
    // 初始化失败，OV5640 未检测到
    // LCD 已清屏为黑色，可显示错误信息
}
```

**返回：** `0` 成功，`-1` OV5640 未检测到。

### `int Camera_FrameReady(void)`

检查是否有新帧数据就绪。

```c
if (Camera_FrameReady()) {
    // 一帧新数据已采集完毕，可读取
}
```

**返回：** `1` 有新帧，`0` 无。

### `void Camera_DisplayFrame(void)`

将当前帧的中心 240×240 区域显示到 LCD。调用前需确认 `Camera_FrameReady()` 返回 `1`。

```c
Camera_DisplayFrame();  // 刷新 LCD 显示
```

### `uint32_t Camera_GetFrameCount(void)`

获取自初始化以来已显示的帧总数。

```c
uint32_t fps = Camera_GetFrameCount();
```

## 主程序最小示例

```c
#include "camera.h"

int main(void)
{
    // ... 系统初始化（HAL、时钟、MPU、MX_GPIO、MX_DMA、MX_SPI6、MX_DCMI）...

    if (Camera_Init() != 0) {
        Error_Handler();  // 摄像头故障
    }

    while (1) {
        if (Camera_FrameReady()) {
            Camera_DisplayFrame();  // LCD 预览
        }

        // ===== 在这里加视觉处理 =====
        // uint16_t *frame = (uint16_t *)0x24010000;
        // frame[y * 480 + x]  拿像素，RGB565 格式
        // ==========================
    }
}
```

## 视觉处理指南

### 读取像素数据

```c
uint16_t *frame = (uint16_t *)CAM_BUF_ADDR;  // 0x24010000

// 读取坐标 (x, y) 的像素（RGB565）
uint16_t pixel = frame[y * CAM_WIDTH + x];  // y * 480 + x

// RGB565 拆分为 R、G、B 分量
uint8_t r = (pixel >> 11) & 0x1F;   // 5-bit red
uint8_t g = (pixel >> 5)  & 0x3F;   // 6-bit green
uint8_t b =  pixel        & 0x1F;   // 5-bit blue

// 转灰度（加权）
uint8_t gray = (r * 77 + g * 150 + b * 29) >> 8;
```

### 重要：不要在 DMA 写入期间读帧

DMA 持续向帧缓冲写入数据。如果在 `Camera_FrameReady()` 返回 `1` 的**同一帧周期内**完成处理，不会有数据竞争。如果处理时间超过一帧，应拷贝一份到处理缓冲区：

```c
static uint16_t g_process_buf[480 * 360];  // 需足够 RAM

if (Camera_FrameReady()) {
    memcpy(g_process_buf, (void *)CAM_BUF_ADDR, sizeof(g_process_buf));
    Camera_DisplayFrame();
    // 然后在 g_process_buf 上做视觉处理
}
```

## 关键硬件适配记录

| 文件 | 修改 | 原因 |
|------|------|------|
| dcmi.c | PCLK FALLING→RISING | OV5640 上升沿输出数据 |
| dcmi.c | 引脚 NOPULL→PULLUP | 防止浮空 |
| dcmi.c | DMA FIFO FULL→HALFFULL | 流数据效率 |
| dcmi.c | DMA Priority LOW→HIGH | 视频流优先级 |
| dcmi.h | 添加 `extern hdma_dcmi` | 驱动需访问 DMA 句柄 |
| lcd_spi_154.c | hspi4→hspi6 | SPI 外设匹配 |
| lcd_spi_154.h | GPIO 时钟端口修正 | PD/PE→PG |
| dcmi_ov5640.h | 删除 usart.h | 工程无 UART |
| dcmi_ov5640.c | 删除 fputc() | 无 UART，用 semihosting |
| main.c | `__io_putchar` 弱实现 | 防止无调试器时 printf 崩溃 |
| main.c | MPU 0x30000000→非缓存 | DMA/CPU 缓存一致性问题 |
| main.c | MPU 0x24010000→非缓存 | 帧缓冲避开 D-Cache |

## 分辨率配置

如需修改，在 [camera.h](Hardware/camera.h) 中调整：

```c
#define CAM_WIDTH   480    // ← 改为需要的宽度（需被4整除，保持4:3比例）
#define CAM_HEIGHT  360    // ← 改为需要的高度
```

同时修改 [dcmi_ov5640.h](Hardware/dcmi_ov5640.h) 中传感器输出（必须≥采集尺寸）：

```c
#define OV5640_Width   640   // 传感器输出宽度（4:3）
#define OV5640_Height  480   // 传感器输出高度（4:3）
```

> **约束：** 传感器输出和 DCMI 采集宽高比必须一致（都是 4:3），否则 OV5640 缩放器拒收。

## 故障排查

| 现象 | 原因 | 解决 |
|------|------|------|
| 反复复位 | 供电不足 | 接两根供电线 |
| 黑屏 | OV5640 未检测到 | 检查 SCCB 接线 PF14/PF15 |
| 花屏/四格 | 宽高比不匹配 | 确保都是 4:3 |
| 黑线/卡死 | D-Cache 问题 | 确认 MPU 已设非缓存 |
| 烧录后无反应 | printf 崩溃 | `__io_putchar` 弱实现已修复 |
