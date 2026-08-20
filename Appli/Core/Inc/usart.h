/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   串口打印函数声明（ai_tea_pro 精简版：WiFi/RS485 串口已移除，
  *          printf 重定向为空操作，print_info_* 仅保留接口兼容）
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

extern UART_HandleTypeDef huart1;

void MX_USART1_UART_Init(void);

/* 引导阶段标记：每个埋点写一次，CubeProgrammer 读此变量地址即可定位卡死位置 */
extern volatile uint32_t g_boot_marker;
#define LOG_MARK(code, ...) do { g_boot_marker = (uint32_t)(code); printf(__VA_ARGS__); } while (0)

/* USER CODE BEGIN Prototypes */
void print_info_warning(const char *format, ...);
void print_info_err(const char *format, ...);
void print_info_normal(const char *format, ...);
void print_info_debug(const char *format, ...);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */
