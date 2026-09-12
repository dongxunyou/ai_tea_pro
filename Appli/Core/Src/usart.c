/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
// todo  此处
#include <stdio.h>
#include <stdarg.h>

/* ══════════════════════════════════════════════════════════════
 * ★ printf 调试输出重定向（2026-09-11 添加：NPU 插桩诊断）
 * --------------------------------------------------------------
 * printf 打到 UART7(PH3=TX, PE7=RX)。
 * （历史：USART3 曾给 RS485 即热模组用、UART7 曾给 WiFi/ESP8266 遥测用，
 *   这两个模块已于 2026-09-12 随工程精简一并删除，UART7 现专供 printf。）
 *
 * 波特率：原来是给 ESP8266 的 250000，调试改 115200（PC 端更通用）。
 *         要改回只改下面这个宏。
 * ══════════════════════════════════════════════════════════════ */
#ifndef DEBUG_UART_ENABLE
#define DEBUG_UART_ENABLE 1
#endif
#ifndef DEBUG_UART_BAUD
#define DEBUG_UART_BAUD   115200U
#endif

extern UART_HandleTypeDef huart7;

/**
* @brief 串口重定向
*/
int _write(int file, char *ptr, int len)
{
    (void)file;
#if DEBUG_UART_ENABLE
    /* 用有限超时而非 HAL_MAX_DELAY：串口没初始化/被占也不会把调用者永久卡死
     * （printf 也会在 IAC_IRQHandler 里被调用，不能用无限等待）。*/
    if ((ptr != NULL) && (len > 0)) {
        (void)HAL_UART_Transmit(&huart7, (uint8_t *)ptr, (uint16_t)len, 100U);
    }
#else
    (void)ptr;
#endif
    return len;
}
/**
 * @brief 错误信息打印（红色样式）
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void print_info_err(const char *format, ...)
{
    va_list args;
    
    printf("\n==============[ERROR]=============\r\n");
    printf("[ERR]: ");
    
    va_start(args, format);
    vprintf(format, args);  // 使用vprintf处理可变参数
    va_end(args);
    
    printf("\r\n==============[ERROR]=============\r\n");
}

/**
 * @brief 普通信息打印
 * @param format 格式化字符串
 * @param ... 可变参数
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
 * @brief 警告信息打印（黄色样式）
 * @param format 格式化字符串
 * @param ... 可变参数
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
 * @brief 调试信息打印（黄色样式）
 * @param format 格式化字符串
 * @param ... 可变参数
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
 
UART_HandleTypeDef huart7;
 
/* USER CODE END 0 */
 
UART_HandleTypeDef huart3;

/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3;
    PeriphClkInitStruct.Usart3ClockSelection = RCC_USART3CLKSOURCE_CLKP;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PE1     ------> USART3_TX
    PD9     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
  else if(uartHandle->Instance==UART7)
  {
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_UART7;
    PeriphClkInitStruct.Uart7ClockSelection = RCC_UART7CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }
    __HAL_RCC_UART7_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART7;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART7;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PE1     ------> USART3_TX
    PD9     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_1);

    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_9);

  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
  if(uartHandle->Instance==UART7)
  {
    __HAL_RCC_UART7_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOH, GPIO_PIN_3);
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_7);
  }
}

/* USER CODE BEGIN 1 */

/* ==== UART7 RX：GPDMA + IDLE + 环形缓冲 ==== */

DMA_HandleTypeDef hdma_uart7_rx;

volatile uint32_t g_rx_isr_cnt = 0;
volatile uint32_t g_rx_bytes   = 0;
volatile int      g_rx_arm_rc  = -1;
volatile uint32_t g_rx_err_cnt = 0;
volatile uint32_t g_rx_wdog_cnt = 0;

__attribute__((aligned(32))) uint8_t rx_dma_buf[RX_DMA_BUF_SIZE];

static volatile uint8_t  rx_ring[512u];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;
static uint16_t          rx_old_pos = 0;

static inline void rx_ring_push(uint8_t b) {
  uint16_t next = (uint16_t)((rx_head + 1u) % 512u);
  if (next != rx_tail) { rx_ring[rx_head] = b; rx_head = next; }
}

int uart7_rx_pop(uint8_t *out) {
  if (rx_tail == rx_head) return 0;
  *out = rx_ring[rx_tail];
  rx_tail = (uint16_t)((rx_tail + 1u) % 512u);
  return 1;
}

void MX_UART7_RX_DMA_Init(void) {
  __HAL_RCC_GPDMA1_CLK_ENABLE();
  hdma_uart7_rx.Instance                   = GPDMA1_Channel0;
  hdma_uart7_rx.Init.Request               = GPDMA1_REQUEST_UART7_RX;
  hdma_uart7_rx.Init.BlkHWRequest          = DMA_BREQ_SINGLE_BURST;
  hdma_uart7_rx.Init.Direction             = DMA_PERIPH_TO_MEMORY;
  hdma_uart7_rx.Init.SrcInc                = DMA_SINC_FIXED;
  hdma_uart7_rx.Init.DestInc               = DMA_DINC_INCREMENTED;
  hdma_uart7_rx.Init.SrcDataWidth          = DMA_SRC_DATAWIDTH_BYTE;
  hdma_uart7_rx.Init.DestDataWidth         = DMA_DEST_DATAWIDTH_BYTE;
  hdma_uart7_rx.Init.Priority              = DMA_LOW_PRIORITY_LOW_WEIGHT;
  hdma_uart7_rx.Init.SrcBurstLength        = 1;
  hdma_uart7_rx.Init.DestBurstLength       = 1;
  hdma_uart7_rx.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
  hdma_uart7_rx.Init.TransferEventMode     = DMA_TCEM_BLOCK_TRANSFER;
  hdma_uart7_rx.Init.Mode                  = DMA_NORMAL;
  if (HAL_DMA_Init(&hdma_uart7_rx) != HAL_OK) { Error_Handler(); }
  if (HAL_DMA_ConfigChannelAttributes(&hdma_uart7_rx,
          DMA_CHANNEL_SEC | DMA_CHANNEL_PRIV |
          DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC) != HAL_OK) {
    Error_Handler();
  }
  __HAL_LINKDMA(&huart7, hdmarx, hdma_uart7_rx);
  HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
  HAL_NVIC_SetPriority(UART7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(UART7_IRQn);
}

void uart7_rx_start(void) {
  rx_old_pos = 0;
  /* 清除 ESP boot 期间可能引入的 ORE/FE/NF 错误，排空 RDR，再 arm DMA */
  __HAL_UART_CLEAR_FLAG(&huart7, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_IDLEF);
  (void)READ_REG(huart7.Instance->RDR);
  g_rx_arm_rc = (int)HAL_UARTEx_ReceiveToIdle_DMA(&huart7, rx_dma_buf, sizeof(rx_dma_buf));
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart->Instance != UART7) return;
  g_rx_isr_cnt++;
  g_rx_bytes += Size;
  if (READ_BIT(huart->Instance->ISR, USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
    g_rx_err_cnt++;
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_NEF);
  }
  SCB_InvalidateDCache_by_Addr((uint32_t *)rx_dma_buf, RX_DMA_BUF_SIZE);
  while (rx_old_pos < Size) { rx_ring_push(rx_dma_buf[rx_old_pos]); rx_old_pos++; }
  if (huart->RxState == HAL_UART_STATE_READY) {
    rx_old_pos = 0;
    HAL_UARTEx_ReceiveToIdle_DMA(&huart7, rx_dma_buf, RX_DMA_BUF_SIZE);
  }
}

/* UART7：PH3=TX, PE7=RX, AF8。原接 ESP8266(250000)，现改作 printf 调试口
 * （2026-09-11：WiFi 模块已舍弃，本口让给 NPU 插桩诊断输出）。 */
void MX_UART7_UART_Init(void)
{
  huart7.Instance = UART7;
  huart7.Init.BaudRate = DEBUG_UART_BAUD;
  huart7.Init.WordLength = UART_WORDLENGTH_8B;
  huart7.Init.StopBits = UART_STOPBITS_1;
  huart7.Init.Parity = UART_PARITY_NONE;
  huart7.Init.Mode = UART_MODE_TX_RX;
  huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart7.Init.OverSampling = UART_OVERSAMPLING_16;
  huart7.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart7.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart7.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart7) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart7, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart7, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart7) != HAL_OK)
  {
    Error_Handler();
  }
}

/* RX 看门狗:ESP 开机噪声(74880→FE/ORE)会卡死 DMA;卡死后 IDLE 回调不再触发、
   RxEventCallback 里的重arm 永远等不到 → RX 永久死。此函数在任务里周期调用,
   直接读 ISR 查错,卡死就 abort+重arm 自愈。稳态无错时永不触发,不影响正常收包。*/
void uart7_rx_health_check(void) {
  uint32_t isr = READ_REG(huart7.Instance->ISR);
  if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
    HAL_UART_AbortReceive(&huart7);
    uart7_rx_start();
    g_rx_wdog_cnt++;
  }
}

/* USER CODE END 1 */

