#ifndef __CAMERA_H
#define __CAMERA_H

#include "dcmi_ov5640.h"
#include "lcd_spi_154.h"

/* ========== DCMI采集分辨率（4:3，与ISP和VGA比例一致） ========== */
#define CAM_WIDTH   480   /* 采集宽度 */
#define CAM_HEIGHT  360   /* 采集高度 */
#define CAM_BUF_ADDR  0x24010000             /* 帧缓冲(D1 SRAM) */
#define CAM_BUF_SIZE  (CAM_WIDTH * CAM_HEIGHT * 2 / 4) /* DMA传输量 */

/* ========== API ========== */
int  Camera_Init(void);
int  Camera_FrameReady(void);
void Camera_DisplayFrame(void);
uint32_t Camera_GetFrameCount(void);

#endif
