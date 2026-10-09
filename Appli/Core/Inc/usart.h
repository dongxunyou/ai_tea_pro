/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart7;

/* USER CODE BEGIN Private defines */
#define RX_DMA_BUF_SIZE   256u
extern DMA_HandleTypeDef hdma_uart7_rx;
extern uint8_t rx_dma_buf[RX_DMA_BUF_SIZE];
extern volatile uint32_t g_rx_isr_cnt;
extern volatile uint32_t g_rx_bytes;
extern volatile int      g_rx_arm_rc;
extern volatile uint32_t g_rx_err_cnt;
extern volatile uint32_t g_rx_wdog_cnt;
void uart7_rx_health_check(void);
/* USER CODE END Private defines */

void MX_USART3_UART_Init(void);

/* USER CODE BEGIN Prototypes */
void print_info_warning(const char *format, ...);
void print_info_err(const char *format, ...);
void print_info_normal(const char *format, ...);
void print_info_debug(const char *format, ...);
void MX_UART7_UART_Init(void);
void MX_UART7_RX_DMA_Init(void);
void uart7_rx_start(void);
int  uart7_rx_pop(uint8_t *out);

/* ══════════════════════════════════════════════════════════════
 * 原始数据导出模式（干茶叶数据集采集用，2026-09-13 添加）
 * --------------------------------------------------------------
 * 图像导出期间 UART7 必須被独占：printf 的任何日志字节混入图像流都会
 * 毁掉整帧。置 1 后 _write() 全部静默丢弃。默认 0 —— 完全不改变原有
 * 调试行为（含 NPU 插桩诊断输出）。
 * ══════════════════════════════════════════════════════════════ */
void    uart7_set_raw_mode(uint8_t on);
uint8_t uart7_get_raw_mode(void);

/* 裸发送，绕过 printf 通路，供图像数据分包发送 */
int     uart7_raw_write(const uint8_t *buf, uint32_t len, uint32_t timeout_ms);

/* 运行时切换波特率（波特率自检用）。
 * ★ 调用后 TX 立刻变波特率，调用者必须保证 RX DMA 已重新 arm。
 *   返回 0=成功，<0=失败（此时 UART 仍停在原波特率）。 */
int     uart7_set_baud(uint32_t baud);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

