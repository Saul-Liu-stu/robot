/*
 * bluetooth_control.c
 * 蓝牙帧协议 + 收发
 *
 * 帧: 0xA5 | CMD | CMD(校验) | 0x5A
 * 校验失败自动丢帧，不回复错误信息（减少串口流量）
 */
#include "bluetooth_control.h"
#include <string.h>

extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart3_tx;

/* ---- 帧协议 ---- */
#define BT_HEAD  0xA5
#define BT_TAIL  0x5A

typedef enum { WAIT_HEAD, WAIT_CMD, WAIT_SUM, WAIT_TAIL } BT_State;

static BT_State bt_state = WAIT_HEAD;
static uint8_t  bt_cmd   = 0;

/* ---- DMA 发送 ---- */

void bluetooth_send(const char *message)
{
    uint16_t len = (uint16_t)strlen(message);
    if (len == 0) return;

    /* 等待上次DMA完成（115200下~30字节≈2.6ms，100ms间隔不会阻塞） */
    while (huart3.gState == HAL_UART_STATE_BUSY_TX);
    HAL_UART_Transmit_DMA(&huart3, (uint8_t *)message, len);
}

/* ---- 接收 ---- */

void bluetooth_parse_byte(uint8_t byte)
{
    switch (bt_state) {
    case WAIT_HEAD:
        if (byte == BT_HEAD) bt_state = WAIT_CMD;
        break;
    case WAIT_CMD:
        bt_cmd   = byte;
        bt_state = WAIT_SUM;
        break;
    case WAIT_SUM:
        bt_state = (byte == bt_cmd) ? WAIT_TAIL : WAIT_HEAD;  /* 校验不过→丢帧 */
        break;
    case WAIT_TAIL:
        if (byte == BT_TAIL) bt_on_command(bt_cmd);
        bt_state = WAIT_HEAD;
        break;
    default:
        bt_state = WAIT_HEAD;
        break;
    }
}

/* ---- 弱回调：用户重写 ---- */

__weak void bt_on_command(uint8_t cmd)
{
    (void)cmd;
    bluetooth_send("OK\r\n");
}
