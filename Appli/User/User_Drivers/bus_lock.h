/**
 ****************************************************************************************************
 * @file        bus_lock.h
 * @brief       PE13/PE14 总线互斥锁（ai_tea_pro：自 es8388_i2c.c 抽离）
 * @note        摄像头 SCCB(ov5640.c) 与其他位 bang 总线若共用 PE13/PE14，
 *              在 RTOS 多任务下必须用本锁仲裁，防止时序被撕裂。
 ****************************************************************************************************
 */

#ifndef __BUS_LOCK_H
#define __BUS_LOCK_H

#include "main.h"
#include "cmsis_os2.h"

extern osMutexId_t g_bus_lock_pe13_14_mutex;

void bus_lock_init(void);   /* RTOS 启动后调用一次（MX_FREERTOS_Init） */
void bus_lock_begin(void);  /* 事务开始加锁，支持嵌套 */
void bus_lock_end(void);    /* 事务结束解锁 */

#endif
