# IMU 串口接收故障排查记

> 一次耗时一整下午的调试，**最终根因：上电时序**。
> 本文以该问题为核心，完整讲解原理、排查链路和修复方法。

---

# 核心问题：上电时序导致的接收链永久断裂

## 一、故障现象

IMU（WT9011G4K）通过 UART 接入 STM32H743：

| 操作方式 | 结果 |
|---------|------|
| **上电时** TX 线接着 PD6 | 数据全 0，永远收不到 |
| **上电后**再插线，按 I 开启 | 完全正常 |

同一个硬件、同一份代码，只差"接线时机"——这是典型的**时序问题**，不是配置问题。

## 二、根因：完整的故障链路

```
时间轴 →
──────────────────────────────────────────────────────────

MCU（上电快）                        IMU 模块（上电慢）
    │                                    │
    ├─ USART2 初始化完成                 │ 仍在复位中
    ├─ Receive_IT 武装接收中断            │ TX 引脚输出低电平
    │   （CR1.RXNEIE=1，准备收数）        │
    │                                    │
    │ ←── 低电平被误判为"起始位" ────────┤
    │ ←── 采样 8 位全 0 + 停止位 0 ──────┤
    │                                    │
    │   ★ USART 置位 FE（帧错误）★        │
    │   ★ 同时置位 RXNE（数据就绪）★      │
    │                                    │
    ├─ 中断触发 → HAL_UART_IRQHandler    │
    ├─ HAL 读到 0x00 + 检测到 FE         │
    │                                    │
    │   ★ HAL 走【错误路径】★             │
    │   1. 设置 huart->ErrorCode         │
    │   2. 调用 HAL_UART_ErrorCallback   │
    │      （弱函数，默认是空的！）        │
    │   3. RxState 置为 READY            │
    │   4. RXNEIE 中断被清除             │
    │                                    │
    ├─ 你的代码没有实现 ErrorCallback    │
    ├─ 没有人重新调用 Receive_IT          │
    │                                    │
    └─ 接收链永久断裂                    └─ 模块复位完成，开始正常发数据
       raw=0，什么都收不到                   但 MCU 这边已经"聋"了
```

### 一句话总结

**模块上电复位时 TX 输出低电平 → USART 采到一个全 0 的非法字节 → 帧错误（FE）→ HAL 库默认的错误处理会停掉接收链 → 没有 ErrorCallback 重装接收 → 接收永久死亡。**

### 为什么"后插线"就正常？

后插线时模块早已稳定运行，MCU 收到第一个字节就是正常数据帧，**永远不会触发错误路径**，所以接收链完好。

### 为什么"成功过一次"？

复位噪声是概率性的。某次上电时模块复位恰好没产生完整错误字节（或者噪声恰好没触发 FE），接收链存活，之后一直正常——直到下次重启再次中招。

## 三、HAL 库为什么这样设计？

`HAL_UART_Receive_IT` 的错误处理逻辑（STM32 HAL 通用行为）：

```
正常字节: RXNE → 读 RDR → RxCpltCallback（用户重装接收）→ 继续
错误字节: RXNE + FE/ORE → 读 RDR → ErrorCallback（默认空）→ 停止
```

设计意图：出错时把控制权交给用户，由用户决定怎么恢复。但对初学者来说，**默认行为 = 静默死亡**，没有任何报错提示。

## 四、修复：双保险

```c
/* ── 保险 1：错误回调里自愈 ─────────────────────────────── */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        /* 清掉所有错误标志（FE/ORE/PE/NE） */
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        /* 强制复位状态机（HAL 错误路径不会自己复位 RxState） */
        huart->RxState = HAL_UART_STATE_READY;
        /* 重新武装接收 */
        HAL_UART_Receive_IT(huart, &imu_rx_byte, 1);
    }
}

/* ── 保险 2：主循环看门狗，每 1s 检查断链自动重装 ────────── */
{
    static uint32_t rx_wd_last = 0;
    if ((uint32_t)(uwTick - rx_wd_last) >= 1000u) {
        rx_wd_last = uwTick;
        if (huart2.RxState != HAL_UART_STATE_BUSY_RX) {
            huart2.RxState = HAL_UART_STATE_READY;
            HAL_UART_Receive_IT(&huart2, &imu_rx_byte, 1);
        }
    }
}
```

**注意**：保险 1 是根治，保险 2 是兜底（防其他未知错误路径）。两条都要有。

---

# 附录：这次调试踩过的其他坑

| # | 坑 | 现象 | 一句话原理 |
|---|-----|------|-----------|
| 1 | newlib-nano 浮点 printf | `R: P: Y:` 值为空 | nano 库默认砍掉 `%f` 支持，链接加 `-u _printf_float` |
| 2 | DMA + 栈缓冲区 | 蓝牙消息时好时坏 | 函数返回栈被回收，DMA 还在读 → 必须 `static` |
| 3 | H7 D-Cache | DMA 偶尔发旧数据 | CPU 写缓存 DMA 读内存，发送前刷 `SCB_CleanDCache_by_Addr` |
| 4 | 中断里连发 DMA 发送 | 按某命令后 MCU 卡死 | `while(BUSY_TX)` 在 ISR 死等，DMA 完成中断永远进不来 |
| 5 | 波特率不一致 | 换串口后全 0 | 模块改了 115200 而 MCU 还是 460800，两端必须同步 |

---

# 排查方法论：逐层隔离 + 计数器说话

| 层 | 验证手段 | 判断 |
|----|---------|------|
| 传输层 | 手机日志显示原始行 | 数据有没有到 APP |
| 解析层 | 原始行内容对不对 | 格式/值是否正常 |
| MCU 接收层 | **raw 计数器**（每收一字节 +1） | 中断链路是否活着 |
| MCU 硬件层 | 环回自测（TX 短接 RX） | 收发通路是否完好 |
| 物理层 | 万用表量电压/通断 | 信号是否到引脚 |

**黄金原则**：
1. 加计数器，用数据说话，不靠猜
2. 诊断消息要在**能收到的时机**发（连接后再发，别在上电时发——上电时 APP 还没连上，全丢了）
3. **现象差异直接指向根因**："后插线就行" = 时序问题，别在配置上死磕

---

# 串口收不到数据的完整自查清单

- [ ] 两边波特率一致（用 TTL 验证模块真实波特率）
- [ ] 引脚映射正确（RX 引脚 + AF 复用号）
- [ ] 外设时钟开启（`__HAL_RCC_XXX_CLK_ENABLE`）
- [ ] NVIC 中断使能 + IRQHandler 存在
- [ ] `HAL_UART_Receive_IT` 已武装（检查返回值不是 HAL_BUSY）
- [ ] ★ **实现了 `HAL_UART_ErrorCallback`**（上电噪声必走这条路径！）
- [ ] ★ 接收链有看门狗兜底
- [ ] 共地、线序、接触
- [ ] DMA 缓冲区是 static
- [ ] D-Cache 已刷新（H7 专属）
- [ ] 中断里没有阻塞调用
