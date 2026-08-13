/*
 * bluetooth_control.h
 * 蓝牙通信 — A5/5A 帧协议
 *
 * 帧格式: 0xA5 + CMD + 校验和(==CMD) + 0x5A
 * 上位机发送 4 字节控制帧，下位机回调 bt_on_command(cmd)
 */
#ifndef INC_BLUETOOTH_CONTROL_H_
#define INC_BLUETOOTH_CONTROL_H_

#include "main.h"

/* 发送字符串（阻塞，9600bps 下短消息 <50ms） */
void bluetooth_send(const char *message);

/* 字节级解析，放入 HAL_UART_RxCpltCallback 中调用 */
void bluetooth_parse_byte(uint8_t byte);

/* 收到完整帧时回调 —— 用户自行实现（__weak，可重写） */
void bt_on_command(uint8_t cmd);

#endif
