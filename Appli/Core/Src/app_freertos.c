/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  *                      （ai_tea_pro 精简版：仅保留 lvgl_main + aiTask，
  *                        摄像头 -> NPU 推理 -> 屏幕显示 最小链路）
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
#include "bus_lock.h"
#include "cmsis_os2.h"
#include "gui.h"
#include "lcd.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lvgl.h"
#include "ov5640.h"
#include "subtitle.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* ── 任务句柄 & 属性 ── */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for lvgl_main */
osThreadId_t lvgl_mainHandle;
const osThreadAttr_t lvgl_main_attributes = {
    .name = "lvgl_main", .priority = osPriorityNormal, .stack_size = 16384 * 4};
osThreadId_t aiTaskHandle;
const osThreadAttr_t aiTask_attributes = {
    .name = "aiTask", .priority = osPriorityNormal, .stack_size = 4096 * 4};

volatile bool g_ov5640_ok = false;

/* ============================================================
 * FreeRTOS 初始化
 * ============================================================ */
void MX_FREERTOS_Init(void) {
  /* PE 总线锁（ov5640 SCCB 读写用） */
  bus_lock_init();

  /* LVGL 主任务 */
  lvgl_mainHandle = osThreadNew(lvgl_main, NULL, &lvgl_main_attributes);

  /* 注意：AI task 延后创建，见 lvgl_main() 内 */
  aiTaskHandle = NULL;
}

/* ============================================================
 * defaultTask（空转占位，CubeMX 结构保留）
 * ============================================================ */
void StartDefaultTask(void *argument) {
  (void)argument;
  for (;;) {
    osDelay(1000);
  }
}

/* ============================================================
 * LVGL 主任务
 * ============================================================ */
static void lvgl_log_cb(lv_log_level_t level, const char *buf) {
  (void)level;
  (void)buf;
  // printf("[LVGL] %s\r\n", buf);
}

void lvgl_main(void *argument) {
  (void)argument;
  osDelay(100);

  lcd_init();   /* 8080 屏（MD0700）初始化：FMC 读 ID 自识别 + 点亮背光 */
  printf("[LCD] id = 0x%04X, %ux%u\r\n", lcddev.id, lcddev.width, lcddev.height);
  lv_init();
  lv_log_register_print_cb(lvgl_log_cb);
  lv_port_disp_init();
  lv_port_indev_init();
  lv_tick_set_cb((lv_tick_get_cb_t)osKernelGetTickCount);

  {
    int retry;
    g_ov5640_ok = false;
    for (retry = 0; retry < 5; retry++) {
      if (ov5640_init() == 0) { g_ov5640_ok = true; break; }
      osDelay(100);
    }
    if (g_ov5640_ok) {
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
    }
  }

  lv_ai_gui_init();  /* AI 识别界面（摄像头画面 + 检测框 + 识别按钮） */

  Subtitle_Init();  /* 创建底部字幕 label，默认隐藏/空 */

  /* ── 延后创建 AI task ── */
  if (aiTaskHandle == NULL) {
    aiTaskHandle = osThreadNew(ai_task, NULL, &aiTask_attributes);
  }

  for (;;) {
    if (g_cam_frame_ready) {
      g_cam_frame_ready = 0;
      lv_obj_invalidate_cam();
    }
    lv_timer_handler();
    osDelay(5);
  }
}
