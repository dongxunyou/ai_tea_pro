/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   串口打印实现（ai_tea_pro 精简版）
  *          原 USART3(RS485 即热模组) / UART7(ESP8266 WiFi) 已全部移除；
  *          _write 为空操作，print_info_* 保留以兼容既有调试调用。
  *          若将来需要串口日志，在这里把 _write 接到新串口即可。
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
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <stdio.h>
#include <stdarg.h>

/**
* @brief 串口重定向（当前为空操作，不输出到任何串口）
*/
int _write(int file, char *ptr, int len)
{
    (void)file; (void)ptr;
    return len;
}

/**
 * @brief 错误信息打印
 */
void print_info_err(const char *format, ...)
{
    va_list args;

    printf("\n==============[ERROR]=============\r\n");
    printf("[ERR]: ");

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\r\n==============[ERROR]=============\r\n");
}

/**
 * @brief 普通信息打印
 */
void print_info_normal(const char *format, ...)
{
    va_list args;

    printf("[INFO]: ");

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\r\n");
}

/**
 * @brief 警告信息打印
 */
void print_info_warning(const char *format, ...)
{
    va_list args;

    printf("\n------------[Warning]------------\r\n");
    printf("[Warn]: ");

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\r\n------------[Warning]------------\r\n");
}

/**
 * @brief 调试信息打印
 */
void print_info_debug(const char *format, ...)
{
    va_list args;

    printf("[DEBUG]: ");

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\r\n");
}
/* USER CODE END 0 */
