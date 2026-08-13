#include "camera.h"

static uint32_t g_frame_count = 0;

/* LCD中心裁剪偏移：从CAM帧中取240x240 */
#define CROP_X  ((CAM_WIDTH  - LCD_Width)  / 2)  /* 120 */
#define CROP_Y  ((CAM_HEIGHT - LCD_Height) / 2)  /* 60  */

/**
 * @brief  摄像头初始化（480×360，4:3）
 *
 *  流程：
 *    OV5640 ISP 1280×960 → Set_Framesize 640×480 (VGA)
 *          → DCMI 硬件裁剪 480×360 中心 → DMA → 帧缓冲(0x24010000)
 *    LCD  → 逐行显示帧中心 240×240
 *
 *  缓冲: 480×360×2 = 337KB
 *
 * @retval 0=成功, -1=OV5640未检测到
 */
int Camera_Init(void)
{
    /* ----- LCD ----- */
    SPI_LCD_Init();
    LCD_Backlight_ON;
    LCD_SetDirection(Direction_V);
    LCD_SetBackColor(LCD_BLACK);
    LCD_Clear();

    /* ----- OV5640（640×480 VGA）----- */
    if (DCMI_OV5640_Init() != 0)
        return -1;

    /* 覆盖DCMI crop: 从640×480中取480×360中心 */
    /* 水平单位=PCLK周期，RGB565每像素=2 PCLK */
    uint32_t crop_x0 = (640U - CAM_WIDTH);           /* HOFFCNT=160 PCLK */
    uint32_t crop_y0 = (480U - CAM_HEIGHT) / 2;      /* VST=60行 */
    uint32_t crop_xs = CAM_WIDTH  * 2 - 1;           /* CAPCNT=959 PCLK */
    uint32_t crop_ys = CAM_HEIGHT - 1;               /* VLINE=359行 */

    HAL_DCMI_ConfigCrop(&hdcmi, crop_x0, crop_y0, crop_xs, crop_ys);
    HAL_DCMI_EnableCrop(&hdcmi);

    OV5640_AF_Download_Firmware();
    OV5640_AF_Trigger_Constant();
    OV5640_Set_Vertical_Flip(OV5640_Disable);
    OV5640_Set_Horizontal_Mirror(OV5640_Enable);
    OV5640_DMA_Transmit_Continuous((uint32_t)CAM_BUF_ADDR, CAM_BUF_SIZE);

    return 0;
}

int Camera_FrameReady(void)
{
    return (OV5640_FrameState == 1) ? 1 : 0;
}

/**
 * @brief  从480×360帧缓冲中提取中心240×240显示到LCD
 */
void Camera_DisplayFrame(void)
{
    OV5640_FrameState = 0;

    uint16_t *buf = (uint16_t *)CAM_BUF_ADDR;

    for (uint16_t y = 0; y < LCD_Height; y++)
    {
        uint16_t *line = &buf[(y + CROP_Y) * CAM_WIDTH + CROP_X];
        LCD_CopyBuffer(0, y, LCD_Width, 1, line);
    }

    g_frame_count++;
}

uint32_t Camera_GetFrameCount(void)
{
    return g_frame_count;
}
