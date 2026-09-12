/**
 ******************************************************************************
 * @file    lv_port_disp.c
 * @brief   LVGL 9.x 显示接口 - 双缓冲模式（PARTIAL 渲染）
 * @details 平台：STM32N647 (Cortex-M55) + 7寸 RGB LCD 800x480 RGB565
 *          帧缓冲位于外部 SDRAM，Device 类型
 *          VSYNC 中断在优先级 5 处理缓冲区交换
 ******************************************************************************
 */

#include "lv_port_disp.h"
#include "lvgl.h"
#include "rgblcd.h"
#include "stm32n6xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define MY_DISP_HOR_RES     800
#define MY_DISP_VER_RES     480
#define MY_DISP_BPP         2U

#define FB_PIXELS           (MY_DISP_HOR_RES * MY_DISP_VER_RES)
#define FB_BYTES            (FB_PIXELS * MY_DISP_BPP)

static void disp_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map);

static lv_display_t *s_disp = NULL;
static uint16_t *s_work_buf = NULL;
static uint16_t *s_display_buf = NULL;

extern uint16_t g_ltdc_lcd_framebuf_0[FB_PIXELS];
extern uint16_t g_ltdc_lcd_framebuf_1[FB_PIXELS];
extern void rgblcd_request_buffer_swap(uint32_t new_buf);
/**
 * @brief  初始化 LVGL 显示驱动（DIRECT 双缓冲 + DMA2D 加速模式）
 */
void lv_port_disp_init(void)
{
    /*必须使能 DMA2D 时钟！
     * 没开时钟去访问 DMA2D 寄存器 = 直接 HardFault*/

    __HAL_RCC_DMA2D_CLK_ENABLE();
    
    /* 顺手清个状态，避免之前残留的 flag 干扰 */
    DMA2D->IFCR = 0x3FU;  /* 清掉所有中断标志 */
    
    s_disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    if (s_disp == NULL) {
        while (1) { }
    }

    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);

    lv_display_set_buffers(s_disp,
                           (void *)g_ltdc_lcd_framebuf_0,
                           (void *)g_ltdc_lcd_framebuf_1,
                           FB_BYTES,
                           LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_display_set_flush_cb(s_disp, disp_flush_cb);
    lv_display_set_default(s_disp);
}

/**
 * @brief  DIRECT 模式刷新回调（DMA2D 兼容版）
 */
static void disp_flush_cb(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    int32_t y1 = area->y1;
    int32_t y2 = area->y2;

    uint32_t dirty_start = (uint32_t)px_map + 
                           (y1 * MY_DISP_HOR_RES) * MY_DISP_BPP;
    uint32_t dirty_size  = (y2 - y1 + 1) * MY_DISP_HOR_RES * MY_DISP_BPP;
    
    /* 
     * 原因：DMA2D 可能直接写入了 framebuffer（绕过 cache），
     *      cache 里残留旧数据。CleanInvalidate 同时处理两种情况：
     *      1. CPU 写过的部分 → 先写回 SDRAM
     *      2. DMA2D 写过的部分 → 失效掉 cache 里的旧数据
     */
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)dirty_start, dirty_size);
    
    if (lv_display_flush_is_last(disp)) {
        rgblcd_request_buffer_swap((uint32_t)px_map);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
    }
    
    lv_display_flush_ready(disp);
}