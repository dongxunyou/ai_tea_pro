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
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

