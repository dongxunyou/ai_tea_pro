/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dcmipp.c
  * @brief   This file provides code for the configuration
  *          of the DCMIPP instances.
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
#include "dcmipp.h"

/* USER CODE BEGIN 0 */
#include "gui.h"
#include "stm32n6xx_hal_dcmipp.h"
/* USER CODE END 0 */

DCMIPP_HandleTypeDef hdcmipp;

/* DCMIPP init function */
void MX_DCMIPP_Init(void)
{

  /* USER CODE BEGIN DCMIPP_Init 0 */

  /* USER CODE END DCMIPP_Init 0 */

  DCMIPP_ParallelConfTypeDef pParallelConfig = {0};
  DCMIPP_PipeConfTypeDef pPipeConfig = {0};

  /* USER CODE BEGIN DCMIPP_Init 1 */

  /* USER CODE END DCMIPP_Init 1 */
  hdcmipp.Instance = DCMIPP;
  if (HAL_DCMIPP_Init(&hdcmipp) != HAL_OK)
  {
    Error_Handler();
  }

  /** Parallel Config
  */
  pParallelConfig.PCKPolarity = DCMIPP_PCKPOLARITY_RISING ;
  pParallelConfig.HSPolarity = DCMIPP_HSPOLARITY_LOW ;
  pParallelConfig.VSPolarity = DCMIPP_VSPOLARITY_LOW ;
  pParallelConfig.ExtendedDataMode = DCMIPP_INTERFACE_8BITS;
  pParallelConfig.Format = DCMIPP_FORMAT_RGB565;
  pParallelConfig.SwapBits = DCMIPP_SWAPBITS_DISABLE;
  pParallelConfig.SwapCycles = DCMIPP_SWAPCYCLES_ENABLE;
  pParallelConfig.SynchroMode = DCMIPP_SYNCHRO_HARDWARE;
  HAL_DCMIPP_PARALLEL_SetConfig(&hdcmipp, &pParallelConfig);

  /** Pipe 0 Config
  */
  pPipeConfig.FrameRate = DCMIPP_FRAME_RATE_ALL;
  pPipeConfig.PixelPipePitch = 10;
  pPipeConfig.PixelPackerFormat = DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1;
  if (HAL_DCMIPP_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE0, &pPipeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Pipe 1 Config
  */
  HAL_DCMIPP_PARALLEL_SetConfig(&hdcmipp, &pParallelConfig);
  /* USER CODE BEGIN DCMIPP_Init 2 */

  /* USER CODE END DCMIPP_Init 2 */

}

void HAL_DCMIPP_MspInit(DCMIPP_HandleTypeDef* dcmippHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(dcmippHandle->Instance==DCMIPP)
  {
  /* USER CODE BEGIN DCMIPP_MspInit 0 */

  /* USER CODE END DCMIPP_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
    PeriphClkInitStruct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
    PeriphClkInitStruct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    PeriphClkInitStruct.ICSelection[RCC_IC17].ClockDivider = 4;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* DCMIPP clock enable */
    __HAL_RCC_DCMIPP_CLK_ENABLE();

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**DCMIPP GPIO Configuration
    PE8     ------> DCMIPP_D4
    PE5     ------> DCMIPP_D5
    PH9     ------> DCMIPP_D6
    PE6     ------> DCMIPP_D1
    PD0     ------> DCMIPP_HSYNC
    PD5     ------> DCMIPP_PIXCLK
    PB9     ------> DCMIPP_D3
    PB8     ------> DCMIPP_VSYNC
    PD7     ------> DCMIPP_D0
    PB7     ------> DCMIPP_D7
    PE0     ------> DCMIPP_D2
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_5|GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF9_DCMIPP;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF9_DCMIPP;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF10_DCMIPP;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_5|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF9_DCMIPP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_8|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF9_DCMIPP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* DCMIPP interrupt Init */
    HAL_NVIC_SetPriority(DCMIPP_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DCMIPP_IRQn);
  /* USER CODE BEGIN DCMIPP_MspInit 1 */

  /* USER CODE END DCMIPP_MspInit 1 */
  }
}

void HAL_DCMIPP_MspDeInit(DCMIPP_HandleTypeDef* dcmippHandle)
{

  if(dcmippHandle->Instance==DCMIPP)
  {
  /* USER CODE BEGIN DCMIPP_MspDeInit 0 */

  /* USER CODE END DCMIPP_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_DCMIPP_CLK_DISABLE();

    /**DCMIPP GPIO Configuration
    PE8     ------> DCMIPP_D4
    PE5     ------> DCMIPP_D5
    PH9     ------> DCMIPP_D6
    PE6     ------> DCMIPP_D1
    PD0     ------> DCMIPP_HSYNC
    PD5     ------> DCMIPP_PIXCLK
    PB9     ------> DCMIPP_D3
    PB8     ------> DCMIPP_VSYNC
    PD7     ------> DCMIPP_D0
    PB7     ------> DCMIPP_D7
    PE0     ------> DCMIPP_D2
    */
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_8|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_0);

    HAL_GPIO_DeInit(GPIOH, GPIO_PIN_9);

    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_0|GPIO_PIN_5|GPIO_PIN_7);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_9|GPIO_PIN_8|GPIO_PIN_7);

    /* DCMIPP interrupt Deinit */
    HAL_NVIC_DisableIRQ(DCMIPP_IRQn);
  /* USER CODE BEGIN DCMIPP_MspDeInit 1 */

  /* USER CODE END DCMIPP_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

