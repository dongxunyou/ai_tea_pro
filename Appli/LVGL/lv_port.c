/**
  ******************************************************************************
  * @file    lv_port.c
  * @brief   LVGL 9.x 显示接口 - 8080 MCU 屏（MD0700，FMC 接口）
  * @details 平台：STM32N647 (Cortex-M55) + ATK-MD0700 800x480 RGB565
  *          LVGL 渲染到内部 SRAM 双 draw buffer（800x40 行），
  *          flush 回调经 FMC 8080 总线写入屏 GRAM。
  *          draw buffer 与 FMC 地址全程 CPU 访问，无 D-Cache 一致性问题。
  ******************************************************************************
  */

#include "lv_port_disp.h"
#include "lvgl.h"
#include "lcd.h"
#include "stm32n6xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define MY_DISP_HOR_RES     800
#define MY_DISP_VER_RES     480
#define MY_DISP_BPP         2U

/* 渲染缓冲行数：800x40 RGB565 = 64KB/块，双缓冲共 128KB 内部 SRAM */
#define DISP_BUF_LINES      40U
#define DISP_BUF_PIXELS     (MY_DISP_HOR_RES * DISP_BUF_LINES)
#define DISP_BUF_BYTES      (DISP_BUF_PIXELS * MY_DISP_BPP)

static void disp_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map);

static lv_display_t *s_disp = NULL;

/* 双渲染缓冲：内部 SRAM（LVGL 渲染热点，放内部 RAM 远快于 HyperRAM） */
static __attribute__((aligned(32))) uint8_t s_draw_buf0[DISP_BUF_BYTES];
static __attribute__((aligned(32))) uint8_t s_draw_buf1[DISP_BUF_BYTES];

/**
 * @brief  初始化 LVGL 显示驱动（部分刷新双缓冲 + FMC 8080 写屏）
 */
void lv_port_disp_init(void)
{
    s_disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    if (s_disp == NULL) {
        while (1) { }
    }

    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

    lv_display_set_buffers(s_disp,
                           s_draw_buf0,
                           s_draw_buf1,
                           DISP_BUF_BYTES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_flush_cb(s_disp, disp_flush_cb);
    lv_display_set_default(s_disp);
}

/**
 * @brief  刷新回调：开窗 + 连续写 GRAM
 * @note   px_map 指向本次渲染区域（宽度 = x2-x1+1），
 *         lcd_set_window 参数为 (x, y, width, height)。
 *         FMC 为内存映射，CPU 直写；draw buffer 在内部 SRAM，
 *         同一 CPU 读写无 D-Cache 一致性问题。
 */
static void disp_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    uint16_t w = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t h = (uint16_t)(area->y2 - area->y1 + 1);
    uint32_t pixels = (uint32_t)w * h;

    lcd_set_window((uint16_t)area->x1, (uint16_t)area->y1, w, h);
    lcd_write_ram_prepare();

    const uint16_t *src = (const uint16_t *)px_map;
    for (uint32_t i = 0; i < pixels; i++) {
        LCD->LCD_RAM = src[i];
    }

    lv_display_flush_ready(disp);
}
