/*
 * encoder.c
 * GMR 编码器 — EXTI 双沿软件四倍频计数
 *
 * ENC1_A=PE7 (EXTI 上升+下降沿)
 * ENC1_B=PE8 (读电平判方向)
 *
 * 计数逻辑:
 *   A 上升沿: B=0 → ++ , B=1 → --
 *   A 下降沿: B=1 → ++ , B=0 → --
 *   即 A xor B: 1→++, 0→--
 *   每周期 2 个计数, 500PPR → 1000/圈
 */
#include "encoder.h"

/* 时钟基准 ms (由 systick 或 TIM6 驱动) */
extern volatile uint32_t uwTick;

static volatile int32_t g_enc_count[4];

/* ---------- 公开函数 ---------- */

void Encoder_Init(void)
{
    for (int i = 0; i < 4; i++)
        g_enc_count[i] = 0;
}

int32_t Encoder_GetCount(uint8_t id)
{
    if (id > ENCODER_4) return 0;
    return g_enc_count[id];
}

float Encoder_GetRPM(uint8_t id)
{
    if (id > ENCODER_4) return 0.0f;

    static int32_t  last_count[4];
    static uint32_t last_tick[4];
    static uint8_t  first_call[4] = {1,1,1,1};

    if (first_call[id]) {
        last_count[id] = g_enc_count[id];
        last_tick[id]  = uwTick;
        first_call[id] = 0;
        return 0.0f;
    }

    uint32_t dt = uwTick - last_tick[id];
    if (dt < 10) return 0.0f;  /* 不足10ms不计算 */

    int32_t  dc = g_enc_count[id] - last_count[id];
    /* 输出轴转速 = 计数差 ÷ (PPR×2) ÷ 减速比 × 60s/min ÷ dt_ms×1000 */
    float rpm = (float)dc * 60000.0f
              / (float)(ENC_PPR * 2 * GEAR_RATIO)
              / (float)dt;

    last_count[id] = g_enc_count[id];
    last_tick[id]  = uwTick;

    return rpm;
}

void Encoder_Reset(uint8_t id)
{
    if (id > ENCODER_4) return;
    g_enc_count[id] = 0;
}

/* ---------- EXTI 回调 (四路编码器) ---------- */

/* 编码器引脚映射表：{A_pin, B_pin, encoder_id} */
static const struct {
    uint16_t a_pin;
    uint16_t b_pin;
    uint8_t  id;
} g_enc_map[4] = {
    {ENC1_A_Pin, ENC1_B_Pin, ENCODER_1},
    {ENC2_A_Pin, ENC2_B_Pin, ENCODER_2},
    {ENC3_A_Pin, ENC3_B_Pin, ENCODER_3},
    {ENC4_A_Pin, ENC4_B_Pin, ENCODER_4},
};

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* 查找触发脚对应的编码器 */
    uint8_t enc_id = 0xFF;
    uint16_t b_pin = 0;
    for (int i = 0; i < 4; i++) {
        if (GPIO_Pin == g_enc_map[i].a_pin) {
            enc_id = g_enc_map[i].id;
            b_pin  = g_enc_map[i].b_pin;
            break;
        }
    }
    if (enc_id == 0xFF) return;  /* 非编码器中断 */

    /* 读A/B状态（所有编码器都在GPIOE） */
    uint8_t a = (GPIOE->IDR & GPIO_Pin) ? 1 : 0;
    uint8_t b = (GPIOE->IDR & b_pin)     ? 1 : 0;

    if (a ^ b)
        g_enc_count[enc_id]++;
    else
        g_enc_count[enc_id]--;
}
