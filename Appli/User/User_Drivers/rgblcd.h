#ifndef __RGBLCD_H
#define __RGBLCD_H

#include "main.h"

/* ============================================================
 * 引脚定义
 * ============================================================ */

/* 背光控制引脚 */
#define RGBLCD_BL_GPIO_PORT     GPIOA
#define RGBLCD_BL_GPIO_PIN      GPIO_PIN_3

/* ID识别引脚 —— R7/G7/B7 */
#define RGBLCD_R7_GPIO_PORT     GPIOG
#define RGBLCD_R7_GPIO_PIN      GPIO_PIN_9
#define RGBLCD_R7_GPIO_AF       GPIO_AF14_LCD

#define RGBLCD_G7_GPIO_PORT     GPIOB
#define RGBLCD_G7_GPIO_PIN      GPIO_PIN_10
#define RGBLCD_G7_GPIO_AF       GPIO_AF14_LCD

#define RGBLCD_B7_GPIO_PORT     GPIOA
#define RGBLCD_B7_GPIO_PIN      GPIO_PIN_2
#define RGBLCD_B7_GPIO_AF       GPIO_AF14_LCD

/* ============================================================
 * 背光控制宏
 * ============================================================ */
#define RGBLCD_BL(x)    do {                                                            \
                            ((x) ?                                                      \
                            HAL_GPIO_WritePin(RGBLCD_BL_GPIO_PORT,                      \
                                              RGBLCD_BL_GPIO_PIN, GPIO_PIN_SET) :       \
                            HAL_GPIO_WritePin(RGBLCD_BL_GPIO_PORT,                      \
                                              RGBLCD_BL_GPIO_PIN, GPIO_PIN_RESET));     \
                        } while(0)

/* ============================================================
 * 屏幕固定参数（ATK-MD0700R-800480）
 * ============================================================ */
#define LCD_WIDTH       800
#define LCD_HEIGHT      480

/* ============================================================
 * RGB565 常用颜色定义
 * ============================================================ */
#define WHITE           0xFFFF
#define BLACK           0x0000
#define RED             0xF800
#define GREEN           0x07E0
#define BLUE            0x001F
#define YELLOW          0xFFE0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define GRAY            0x8430
#define BROWN           0xBC40
#define DARKBLUE        0x01CF
#define LIGHTBLUE       0x7D7C
#define LIGHTGREEN      0x841F
#define LGRAY           0xC618

/* ============================================================
 * LCD参数结构体
 * ============================================================ */
typedef struct {
    uint16_t id;            /* 屏幕ID，固定 0x7084 */
    uint32_t pwidth;        /* 面板物理宽度（像素） */
    uint32_t pheight;       /* 面板物理高度（像素） */
    uint16_t hsw;           /* 水平同步宽度 */
    uint16_t vsw;           /* 垂直同步宽度 */
    uint16_t hbp;           /* 水平后廊 */
    uint16_t vbp;           /* 垂直后廊 */
    uint16_t hfp;           /* 水平前廊 */
    uint16_t vfp;           /* 垂直前廊 */
    uint8_t  dir;           /* 显示方向：0=竖屏，1=横屏 */
    uint16_t width;         /* 当前显示宽度（随dir变化） */
    uint16_t height;        /* 当前显示高度（随dir变化） */
} _rgblcd_dev;

/* ============================================================
 * 全局变量导出
 * ============================================================ */
extern _rgblcd_dev  rgblcddev;
extern uint32_t     g_back_color;
//extern uint16_t     g_ltdc_lcd_framebuf[LCD_WIDTH * LCD_HEIGHT] __attribute__((section(".EXTRAM")));
extern uint16_t g_ltdc_lcd_framebuf[800 * 480] __attribute__((section(".EXTRAM"), aligned(32)));
extern  uint32_t g_lif_count_lo ;
extern  uint32_t g_lif_count_up ;
extern  uint32_t g_swap_done_count ;
extern uint32_t g_fuif_count ;
extern uint32_t g_terrif_count;
/* ============================================================
 * 函数声明
 * ============================================================ */
 void LTDC_UP_IRQHandler_Impl(void);
void     rgblcd_init(void);
void     rgblcd_display_dir(uint8_t dir);
void     rgblcd_clear(uint16_t color);
void     rgblcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void     rgblcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
void     rgblcd_draw_point(uint16_t x, uint16_t y, uint16_t color);
uint16_t rgblcd_read_point(uint16_t x, uint16_t y);
void     rgblcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void     rgblcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
void     rgblcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void     rgblcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void     rgblcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);
void     rgblcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);
void     rgblcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);
void     rgblcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);
void     rgblcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);
void         rgblcd_set_framebuffer(uint32_t addr);
#endif /* __RGBLCD_H */
