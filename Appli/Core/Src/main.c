/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "main.h"
#include "cmsis_os2.h"
#include "dcmipp.h"
#include "dma2d.h"
#include "ltdc.h"
#include "ramcfg.h"
#include "usart.h"
#include "xspi.h"
#include "xspim.h"

#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "lcd.h"
#include <stdint.h>
#include <stdio.h>
#ifdef DEBUG
#include "hyperram.h"
#endif // DEBUG
#include "lvgl.h"
#include "lv_port_disp.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern DMA2D_HandleTypeDef hdma2d;

extern LTDC_HandleTypeDef hltdc;

extern XSPI_HandleTypeDef hxspi1;

SRAM_HandleTypeDef hsram1;   /* FMC SRAM 句柄：8080 屏（MD0700） */

#ifdef DEBUG
static HyperRAM_ObjectTypeDef HyperRAMObject = {0};
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void MX_FREERTOS_Init(void);
static void SystemIsolation_Config(void);
static void MX_FMC_Init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void sys_clock_config_debug(void);

//git test 
/**
 * @brief 配置 MPU
 *//* main.c */
/**
 * @brief 配置 MPU
 * @note  MPU 配置为 Write-Through
 */
void sys_clock_config_debug(void);
/**
* todo ARM_MPU_ATTR_MEMORY_(NT, WB, RA, WA)
* todo                       │   │   │   │
* todo                       │   │   │   └── Write Allocate (0=不分配, 1=分配)
* todo                       │   │   └────── Read Allocate  (1=分配，加速读操作)
* todo                       │   └────────── Write-Back bit (0=Write-Through, 1=Write-Back)
* todo                       └────────────── Non-Transient  (1=非临时性，倾向保留在 Cache)


*/
//git test 
/**
 * @brief 配置 MPU
 *//* main.c */
/**
 * @brief 配置 MPU
 * @note  MPU 配置为 Write-Through
 */
/* 在系统初始化里加 */
void MPU_Config(void)
{
    MPU_Region_InitTypeDef     MPU_InitStruct = {0};
    MPU_Attributes_InitTypeDef attr           = {0};

    HAL_MPU_Disable();

    /* ── Region 0: 外部存储器 xSPI1 (0x90000000, 8MB) ── */
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress      = 0x90000000;
    MPU_InitStruct.LimitAddress     = 0x90000000 + (8U * 1024U * 1024U) - 1U;
    MPU_InitStruct.AttributesIndex  = MPU_ATTRIBUTES_NUMBER0;
    MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RW;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    attr.Number     = MPU_ATTRIBUTES_NUMBER0;
    attr.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
    HAL_MPU_ConfigMemoryAttributes(&attr);

    /* ── Region 1: NPU AXISRAM (0x34100000 ~ 0x343FFFFF) ── */
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress      = 0x34100000;
    MPU_InitStruct.LimitAddress     = 0x343FFFFF;
    MPU_InitStruct.AttributesIndex  = MPU_ATTRIBUTES_NUMBER1;
    MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RW;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    attr.Number     = MPU_ATTRIBUTES_NUMBER1;
    attr.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);
    HAL_MPU_ConfigMemoryAttributes(&attr);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
MPU_Config();  /* 必须在 Cache 初始化之前 */
  /* USER CODE BEGIN 1 */
//  MPU_Config();
  /* USER CODE END 1 */

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  SystemCoreClockUpdate();
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
  

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */

     #ifdef DEBUG
  sys_clock_config_debug();  /* ← 这里切换到 HSE */
  SystemCoreClockUpdate();   /* ← 更新时钟变量 */
  
  /* 重新配置 XSPI 时钟，使用 HSI 的 PLL1 */
  // RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  // PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_XSPI1;
  // PeriphClkInit.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_IC4;
  // PeriphClkInit.ICSelection[RCC_IC4].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  // PeriphClkInit.ICSelection[RCC_IC4].ClockDivider = 3;  /* ← HSI PLL1=1200MHz, /3=400MHz */
  // HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
  
  __HAL_RCC_XSPI1_CLK_ENABLE();
  MX_XSPI1_Init();
  //   HyperRAM_Init(&HyperRAMObject, &hxspi1);
  // HyperRAM_EnableMemoryMappedMode(&HyperRAMObject);
  if (HyperRAM_Init(&HyperRAMObject, &hxspi1) != HyperRAM_OK) {
    while(1);
  }
  
  if (HyperRAM_EnableMemoryMappedMode(&HyperRAMObject) != HyperRAM_OK) {
    while(1);
  }
  
  #endif
  
    /* ← 关键！手动重新配置 TIM16 */
    __HAL_RCC_TIM16_CLK_DISABLE();  // 先关闭
    __HAL_RCC_TIM16_CLK_ENABLE();   // 再打开
    
    /* 重新初始化 HAL 时基 */
    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
    {
        Error_Handler();
    }

  MX_GPIO_Init();
  MX_DMA2D_Init();

  MX_DCMIPP_Init();
  MX_USART3_UART_Init();  /* 调试日志口 PE1_TX 115200 —— 提到最前，后续外设挂死也能看到日志 */
  printf("[BOOT] USART3 log ready, FMC init...\r\n");
  MX_FMC_Init();          /* 8080 屏（MD0700）FMC 接口 */
  printf("[BOOT] FMC init ok\r\n");
  MX_RAMCFG_Init();
 
  
  //MX_XSPI2_Init();
  
 
 
  // #ifdef DEBUG
  // MX_XSPI1_Init();
  // MX_LTDC_Init();
  
  // #endif
#if defined(CPU_AS_TRUSTED_DOMAIN) && defined(CPU_IN_SECURE_STATE)
  print_info_debug("[CHK] main: RIMC master API ACTIVE (both macros defined)");
#else
  print_info_debug("[CHK] main: RIMC master API is EMPTY (macro missing!)");
#endif
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */

  // ! ------------------------------------------------
  
  

print_info_debug("初始化完成");

    
  //   rgblcd_show_string(10, 40, 240, 32, 32, "STM32", RED);
  // rgblcd_show_string(10, 80, 240, 24, 24, "RGBLCD TEST", RED);
  // rgblcd_show_string(10, 110, 240, 16, 16, "ATOM@ALIENTEK", RED);

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Call init function for freertos objects (in app_freertos.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CKPER;
  PeriphClkInitStruct.CkperClockSelection = RCC_CLKPCLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FMC Initialization Function（8080 屏 MD0700，NE1 + A13=RS，16bit）
  *        移植自正点原子 14_TFTLCD 例程
  */
static void MX_FMC_Init(void)
{
  FMC_NORSRAM_TimingTypeDef Timing = {0};
  FMC_NORSRAM_TimingTypeDef ExtTiming = {0};

  hsram1.Instance = FMC_NORSRAM_DEVICE;
  hsram1.Extended = FMC_NORSRAM_EXTENDED_DEVICE;
  /* hsram1.Init */
  hsram1.Init.NSBank = FMC_NORSRAM_BANK1;
  hsram1.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;
  hsram1.Init.MemoryType = FMC_MEMORY_TYPE_SRAM;
  hsram1.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram1.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;
  hsram1.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram1.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;
  hsram1.Init.WriteOperation = FMC_WRITE_OPERATION_ENABLE;
  hsram1.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
  hsram1.Init.ExtendedMode = FMC_EXTENDED_MODE_ENABLE;
  hsram1.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;
  hsram1.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
  hsram1.Init.PageSize = FMC_PAGE_SIZE_NONE;
  /* Timing */
  Timing.AddressSetupTime = 15;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 107;
  Timing.BusTurnAroundDuration = 15;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FMC_ACCESS_MODE_A;
  /* ExtTiming */
  ExtTiming.AddressSetupTime = 15;
  ExtTiming.AddressHoldTime = 15;
  ExtTiming.DataSetupTime = 20;
  ExtTiming.BusTurnAroundDuration = 15;
  ExtTiming.CLKDivision = 16;
  ExtTiming.DataLatency = 17;
  ExtTiming.AccessMode = FMC_ACCESS_MODE_A;

  if (HAL_SRAM_Init(&hsram1, &Timing, &ExtTiming) != HAL_OK)
  {
    Error_Handler( );
  }
}

/**
  * @brief RIF Initialization Function
  * @param None
  * @retval None
  */
  static void SystemIsolation_Config(void)
{

/* USER CODE BEGIN RIF_Init 0 */

/* USER CODE END RIF_Init 0 */

  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /*RIMC configuration*/
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D, &RIMC_master);

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &RIMC_master);
/* USER CODE BEGIN RIF_Init 0 */
HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);
HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP,
                                      RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
/* ===== NPU 的 RIF 配置：主设备 + 从设备，两样都要 ===== */

  /* ★★ 关键：NPU 作为 AXI 主设备（它要去 0x70700000 读权重）。
   *    RIMC 主设备配 CID1+SEC+PRIV。
   *    任务#23：绕过 HAL（非安全世界 HAL_RIF_RIMC_ConfigMasterAttributes 是空操作），
   *    直接写 RIFSC 寄存器给 NPU 打标签，否则 NPU 发默认(非安全)事务被 RISAF 拦掉。 */
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &RIMC_master);
  {
      uint32_t _rimc_val = RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_NPU];
      _rimc_val &= ~(RIFSC_RIMC_ATTRx_MCID | RIFSC_RIMC_ATTRx_MPRIV | RIFSC_RIMC_ATTRx_MSEC);
      _rimc_val |= (0x1U << RIFSC_RIMC_ATTRx_MCID_Pos)   /* RIF_CID_1 */
                |  (0x1U << RIFSC_RIMC_ATTRx_MSEC_Pos)    /* SEC       */
                |  (0x1U << RIFSC_RIMC_ATTRx_MPRIV_Pos);  /* PRIV      */
      RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_NPU] = _rimc_val;
      __DSB();
      print_info_debug("[CHK] main: NPU RIMC direct register write, ATTRx[1]=%08lX [task#23]\r\n",
                       (unsigned long)RIFSC->RIMC_ATTRx[RIF_MASTER_INDEX_NPU]);
  }

   /* ★ 新增:给 NPU 放行(否则 RuntimeInit 碰 NPU 被 RIF 拦,总线错误冻死) */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
   /* ★ 新增:给 FMC 放行(8080屏 MD0700；NS 侧访问 FMC 寄存器必须授权，否则 HAL_SRAM_Init 即死) */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_FMC,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  /* ★ 彩蛋 SAI1: 不要配 SEC！实测配 SEC|PRIV 后 NS 侧 RCC 时钟使能/寄存器访问被 RIF 拦
   *    (诊断: CR1 读回=0, DMA 等 FIFO 超时 t=3)。SAI1 保持默认非安全属性即可被 NS 使用。
   *    引脚 SEC 配置(PB0/PB2/PC2/PE2)仍保留——那是 NS 使用 GPIO 的必要前提。 */
  /* HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SAI1,
                                        RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV); */
  /* set up PWR configuration */
  HAL_PWR_ConfigAttributes(PWR_ITEM_0, PWR_SEC_NPRIV);
/* USER CODE END RIF_Init 0 */
  /* RIF-Aware IPs Config */
  /* 现有配置 */
  HAL_GPIO_ConfigPinAttributes(GPIOQ, GPIO_PIN_2, GPIO_PIN_SEC|GPIO_PIN_NPRIV);

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_12,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_14,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_15,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_14,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_14,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOQ,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

/* ===== 8080 屏 FMC 引脚补充（MD0700：PA4=RS, PA12=D0, PA15=D15, PB4=D13, PB5=D12, PB6=D14）=====
 * 其余 FMC 引脚（PA0~3/5/8~11、PB7~12、PF8、PG9/13）已在上方列表配置过 */
  HAL_GPIO_ConfigPinAttributes(GPIOA, GPIO_PIN_4,  GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA, GPIO_PIN_12, GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA, GPIO_PIN_15, GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_4,  GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_5,  GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_6,  GPIO_PIN_SEC|GPIO_PIN_NPRIV);
/* 触摸 GT911 复位/中断（MD0700：PD10=RST, PB3=INT） */
  HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_10, GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_3,  GPIO_PIN_SEC|GPIO_PIN_NPRIV);

/* USER CODE BEGIN RIF_Init 1 */

/* USER CODE END RIF_Init 1 */
/* USER CODE BEGIN RIF_Init 2 */
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RCC_PERIPH_INDEX_GPDMA1,
                                        RIF_ATTRIBUTE_PRIV);
/* USER CODE END RIF_Init 2 */

}

/* USER CODE BEGIN 4 */
/**
 * @brief   配置系统时钟
 * @param   无
 * @retval  无
 */
void sys_clock_config_debug(void)
{
    RCC_OscInitTypeDef rcc_osc_init_struct = {0};
    RCC_ClkInitTypeDef rcc_clk_init_struct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY);

    /* task#24: NPU clock lowered for overclock test */
    printf("[NPU] init: NPU clock LOWERED test = %lu Hz (task#24, IC6/8)\r\n",
           (unsigned long)HAL_RCC_GetNPUClockFreq());

    /* 第一步：启用 HSE 并切换到 HSE */
    rcc_osc_init_struct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    rcc_osc_init_struct.HSEState = RCC_HSE_ON;
    rcc_osc_init_struct.PLL1.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL2.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL3.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL4.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&rcc_osc_init_struct);

    HAL_RCC_GetClockConfig(&rcc_clk_init_struct);
    if ((rcc_clk_init_struct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) || 
        (rcc_clk_init_struct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
    {
        rcc_clk_init_struct.ClockType = RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK;
        rcc_clk_init_struct.CPUCLKSource = RCC_CPUCLKSOURCE_HSE;
        rcc_clk_init_struct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
        HAL_RCC_ClockConfig(&rcc_clk_init_struct);
    }

    /* 第二步：配置 PLL1（HSE 48MHz → VCO 1200MHz） */
    rcc_osc_init_struct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
    rcc_osc_init_struct.PLL1.PLLState = RCC_PLL_ON;
    rcc_osc_init_struct.PLL1.PLLSource = RCC_PLLSOURCE_HSE;
    rcc_osc_init_struct.PLL1.PLLM = 4;
    rcc_osc_init_struct.PLL1.PLLN = 100;
    rcc_osc_init_struct.PLL1.PLLFractional = 0;
    rcc_osc_init_struct.PLL1.PLLP1 = 1;
    rcc_osc_init_struct.PLL1.PLLP2 = 1;
    rcc_osc_init_struct.PLL2.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL3.PLLState = RCC_PLL_NONE;
    rcc_osc_init_struct.PLL4.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&rcc_osc_init_struct);

    /* 第三步：配置系统时钟 */
    rcc_clk_init_struct.ClockType = RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_HCLK | 
                                     RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | 
                                     RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK5 | 
                                     RCC_CLOCKTYPE_PCLK4;
    rcc_clk_init_struct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
    rcc_clk_init_struct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
    rcc_clk_init_struct.AHBCLKDivider = RCC_HCLK_DIV2;
    rcc_clk_init_struct.APB1CLKDivider = RCC_APB1_DIV1;
    rcc_clk_init_struct.APB2CLKDivider = RCC_APB2_DIV1;
    rcc_clk_init_struct.APB4CLKDivider = RCC_APB4_DIV1;
    rcc_clk_init_struct.APB5CLKDivider = RCC_APB5_DIV1;
    rcc_clk_init_struct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC1Selection.ClockDivider = 2;
    rcc_clk_init_struct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC2Selection.ClockDivider = 3;
    rcc_clk_init_struct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC6Selection.ClockDivider = 8;    /* 任务#24: 从4改为8, NPU降频~150MHz 验超频 */
    rcc_clk_init_struct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_clk_init_struct.IC11Selection.ClockDivider = 3;
    HAL_RCC_ClockConfig(&rcc_clk_init_struct);

    /* 第四步：配置 XSPI 时钟（1200MHz / 12 = 100MHz） */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_XSPI1;
    PeriphClkInit.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_IC4;
    PeriphClkInit.ICSelection[RCC_IC4].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    PeriphClkInit.ICSelection[RCC_IC4].ClockDivider = 12;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    /* 第五步：配置 LTDC 时钟（1200MHz / 36 = 33.33MHz） */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInit.LtdcClockSelection = RCC_LTDCCLKSOURCE_IC16;
    PeriphClkInit.ICSelection[RCC_IC16].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    PeriphClkInit.ICSelection[RCC_IC16].ClockDivider = 36;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    /* 第六步：重新初始化 TIM16（HAL 时基） */
    __HAL_RCC_TIM16_CLK_ENABLE();
    HAL_InitTick(TICK_INT_PRIORITY);
}
/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */




/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM16 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM16)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
