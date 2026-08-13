/*
 * imu.h
 * WT9011G4K 九轴IMU — UART主动上报解析
 *
 * 接线: IMU TX → PD6(USART2_RX), 9600bps
 * 协议: 0x55 + type + data + checksum
 */
#ifndef INC_IMU_H_
#define INC_IMU_H_

#include "main.h"

/* 姿态角（度）, 解算后实时更新 */
typedef struct {
    float roll;   /* 横滚  ±180° */
    float pitch;  /* 俯仰  ±90°  (物理限制) */
    float yaw;    /* 偏航  ±180° (Z轴漂移，不可长期参考) */
} IMU_Angle;

/* 获取最新姿态角 */
const IMU_Angle* IMU_GetAngle(void);

/* 喂字节（HAL_UART_RxCpltCallback中调用） */
void IMU_ParseByte(uint8_t byte);

/* IMU超时检测: 超过timeout_ms未收到数据返回1 */
int IMU_Timeout(uint32_t timeout_ms);

#endif
