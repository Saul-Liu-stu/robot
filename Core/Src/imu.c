/*
 * imu.c
 * WT9011G4K 协议解析 — 参考维特 JY901 标准实现
 *
 * 所有数据包固定11字节:
 *   [0] 0x55  帧头
 *   [1] 类型  (0x51加速度 0x52角速度 0x53角度 0x59四元数)
 *   [2..9] 8字节数据
 *   [10]    校验和 (byte0~9累加取低8位)
 *
 * 数据为大端 int16: hi<<8 | lo
 */
#include "imu.h"

extern volatile uint32_t uwTick;

static IMU_Angle g_angle = {0};
static uint32_t  imu_last_tick = 0;
static uint32_t  g_raw_count = 0;    /* 诊断: 原始字节数 */
static uint32_t  g_frame_count = 0;  /* 诊断: 有效角度帧数 */

static uint8_t  imu_buf[11];
static uint8_t  imu_idx = 0;

const IMU_Angle* IMU_GetAngle(void)
{
    return &g_angle;
}

uint32_t IMU_GetRawCount(void)  { return g_raw_count; }
uint32_t IMU_GetFrameCount(void){ return g_frame_count; }

int IMU_Timeout(uint32_t timeout_ms)
{
    return (uwTick - imu_last_tick > timeout_ms) ? 1 : 0;
}

void IMU_ParseByte(uint8_t byte)
{
    g_raw_count++;

    /* 等帧头 */
    if (imu_idx == 0 && byte != 0x55)
        return;

    imu_buf[imu_idx++] = byte;

    if (imu_idx < 11) return;   /* 未收满 */

    imu_idx = 0;

    /* 校验: byte0~9 累加 == byte10 */
    uint8_t sum = 0;
    for (int i = 0; i < 10; i++)
        sum += imu_buf[i];
    if (sum != imu_buf[10])
        return;  /* 校验失败，丢弃 */

    uint8_t type = imu_buf[1];

    if (type == 0x53) {  /* 姿态角 */
        int16_t roll  = (int16_t)((imu_buf[3] << 8) | imu_buf[2]);
        int16_t pitch = (int16_t)((imu_buf[5] << 8) | imu_buf[4]);
        int16_t yaw   = (int16_t)((imu_buf[7] << 8) | imu_buf[6]);

        g_angle.roll  = roll  * 180.0f / 32768.0f;
        g_angle.pitch = pitch * 180.0f / 32768.0f;
        g_angle.yaw   = yaw   * 180.0f / 32768.0f;

        imu_last_tick = uwTick;
        g_frame_count++;
    }
    /* 其他类型(0x51加速度/0x52角速度/0x59四元数)可在此扩展 */
}
