/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
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
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
#include "ai_task.h"
#include "cmsis_os2.h"
#include "demos/lv_demos.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"
#include "ov5640.h"
#include "rgblcd.h"
#include "subtitle.h"
#include "src/lv_init.h"
#include "touch_debug.h"
#include "touch_test.h"
#include "usart.h"
#include <stdint.h>
#include "tea_ui.h"

/* ── 任务句柄 & 属性 ── */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for led01 */
osThreadId_t led01Handle;
const osThreadAttr_t led01_attributes = {
  .name = "led01",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for led02 */
osThreadId_t led02Handle;
const osThreadAttr_t led02_attributes = {
  .name = "led02",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for lvgl_main */
osThreadId_t lvgl_mainHandle;
const osThreadAttr_t lvgl_main_attributes = {
    .name = "lvgl_main", .priority = osPriorityNormal, .stack_size = 16384 * 4};
osThreadId_t aiTaskHandle;
const osThreadAttr_t aiTask_attributes = {
    .name = "aiTask", .priority = osPriorityNormal, .stack_size = 4096 * 4};

/* ============================================================
 * FreeRTOS 初始化
 * ============================================================ */
void MX_FREERTOS_Init(void) {
  /* 任务创建
   * 2026-09-12 精简：原 defaultTask(WiFi 遥测) / servoTask(舵机) /
   *   sensorTask(称重) / weightPumpTask(水泵) / hotWaterTask(即热) 已随
   *   对应模块一并删除，只保留 led01 / led02 / lvgl_main，
   *   aiTask 延后在 lvgl_main() 内创建（需等摄像头与 LVGL 就绪）。*/
  defaultTaskHandle =
      osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  led01Handle = osThreadNew(led01, NULL, &led01_attributes);

  /* creation of led02 */
  led02Handle = osThreadNew(led02, NULL, &led02_attributes);

  /* creation of lvgl_main */
  lvgl_mainHandle = osThreadNew(lvgl_main, NULL, &lvgl_main_attributes);

  /* 注意：AI task 延后创建，见 lvgl_main() 内 */
  aiTaskHandle = NULL;
}

/* ============================================================
 * defaultTask
 * ============================================================ */
void StartDefaultTask(void *argument) {
  (void)argument;
  /* 原 WiFi 遥测任务；WiFi 模块已于 2026-09-12 删除，此处仅保留空转。*/
  for (;;) {
    osDelay(1000);
  }
  /* USER CODE END defaultTask */
}

/* ============================================================
 * led01 / led02
 * ============================================================ */
void led01(void *argument) {
  for (;;){
    osDelay(100);
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_10);
    osDelay(200);
    HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_10);
    osDelay(200);
  
  
  }
}
void led02(void *argument) {
  for (;;)
    osDelay(500);
  }

/* ============================================================
 * LVGL 主任务
 * ============================================================ */
static void lvgl_log_cb(lv_log_level_t level, const char *buf) {
  (void)level;
  // printf("[LVGL] %s\r\n", buf);
}

void lvgl_main(void *argument) {
  (void)argument;
  osDelay(100);

  rgblcd_init();
  lv_init();
  lv_log_register_print_cb(lvgl_log_cb);
  lv_port_disp_init();
  lv_port_indev_init();
  lv_tick_set_cb((lv_tick_get_cb_t)osKernelGetTickCount);

  while (ov5640_init() != 0)
    osDelay(100);
  ov5640_rgb565_mode();
  ov5640_focus_init();
  ov5640_light_mode(0);
  ov5640_color_saturation(3);
  ov5640_brightness(4);
  ov5640_contrast(3);
  ov5640_sharpness(33);
  ov5640_focus_constant();
  ov5640_outsize_set(4, 0, CAM_W, CAM_H);
  ov5640_dcmipp_init();
  ov5640_dcmipp_start();


  tea_ui_init();

  Subtitle_Init();  /* 创建底部字幕 label，默认隐藏/空 */

  /* ── 延后创建 AI task ── */
  if (aiTaskHandle == NULL) {
    aiTaskHandle = osThreadNew(ai_task, NULL, &aiTask_attributes);
  }

  /* 相机预览刷新由 tea_ui 的 50ms LVGL timer 负责（读 cam_read_buf 后
   * cam_resize_128 缩放并 invalidate canvas），此处只跑 LVGL 时基。*/
  for (;;) {
    lv_timer_handler();
    osDelay(5);
  }
}