/**
 ****************************************************************************************************
 * @file        bus_lock.c
 * @brief       PE13/PE14 总线互斥锁实现
 ****************************************************************************************************
 */

#include "bus_lock.h"

osMutexId_t g_bus_lock_pe13_14_mutex = NULL;
static volatile int g_bus_lock_cnt = 0;

/**
 * @brief       总线互斥锁初始化
 * @note        由 MX_FREERTOS_Init 调用；懒初始化兜底见 bus_lock_begin
 */
void bus_lock_init(void)
{
    if (g_bus_lock_pe13_14_mutex == NULL)
    {
        g_bus_lock_pe13_14_mutex = osMutexNew(NULL);
    }
}

/**
 * @brief       总线加锁（支持嵌套调用，SCCB 读时序含两个 START + 一个 STOP）
 */
void bus_lock_begin(void)
{
    if (g_bus_lock_cnt++ == 0)
    {
        if (g_bus_lock_pe13_14_mutex == NULL)
        {
            bus_lock_init();
        }
        osMutexAcquire(g_bus_lock_pe13_14_mutex, osWaitForever);
    }
}

/**
 * @brief       总线解锁
 */
void bus_lock_end(void)
{
    if (--g_bus_lock_cnt == 0)
    {
        osMutexRelease(g_bus_lock_pe13_14_mutex);
    }
}
