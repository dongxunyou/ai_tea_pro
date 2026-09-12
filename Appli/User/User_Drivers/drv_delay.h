#ifndef __DRV_DELAY_H
#define __DRV_DELAY_H

/**
 * @file    drv_delay.h
 * @brief   基于DWT的微秒延时工具（适配STM32N647，Cortex-M55）
 * @note    调用 drv_delay_init() 一次后，可全局使用 drv_delay_us()
 */

#include "stm32n6xx_hal.h"

/* ----------------------------------------------------------------
 * DWT Lock Access Register 地址（固定地址，不依赖结构体成员名）
 * ARMv8-M架构：DWT基地址 0xE0001000，LAR偏移 0xFB0
 * ---------------------------------------------------------------- */
#define DWT_LAR_ADDR   (*((volatile uint32_t *)0xE0001FB0UL))
#define DWT_LAR_UNLOCK  0xC5ACCE55UL

/* ----------------------------------------------------------------
 * DWT 初始化
 * ---------------------------------------------------------------- */
static inline void drv_delay_init(void)
{
    /* 步骤1：使能 DWT 追踪单元
     * Cortex-M55 使用 DCB->DEMCR，兜底用裸地址 */
#if defined(DCB) && defined(DCB_DEMCR_TRCENA_Msk)
    DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
#elif defined(CoreDebug) && defined(CoreDebug_DEMCR_TRCENA_Msk)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#else
    /* 通用兜底：直接写 DEMCR 寄存器地址 */
    *((volatile uint32_t *)0xE000EDFC UL) |= (1UL << 24);
#endif

    /* 步骤2：解锁 DWT 写访问（Cortex-M33/M55 必须，否则 CYCCNT 永远是0） */
    DWT_LAR_ADDR = DWT_LAR_UNLOCK;

    /* 步骤3：清零并使能 CYCCNT */
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ----------------------------------------------------------------
 * 微秒延时（基于DWT周期计数，不依赖中断，精度高）
 * ---------------------------------------------------------------- */
static inline void drv_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    /* 每微秒所需CPU周期数 = SystemCoreClock / 1000000 */
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks)
    {
        __NOP();
    }
}

#endif /* __DRV_DELAY_H */