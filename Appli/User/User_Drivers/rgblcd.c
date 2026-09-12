/*rgblcd.c*/
#include "rgblcd.h"
#include "rgblcdfont.h"
#include "usart.h"
#include <string.h>          /* ← 添加这一行 */
#include "app_freertos.h"

extern LTDC_HandleTypeDef hltdc;
extern DMA2D_HandleTypeDef hdma2d;
 uint32_t g_fuif_count = 0;
 uint32_t g_terrif_count = 0;

/* 全局计数器，方便看是哪个中断在响 */
 uint32_t g_lif_count_lo = 0;
 uint32_t g_lif_count_up = 0;
 uint32_t g_swap_done_count = 0;
// ! ------
volatile uint32_t g_lif_count = 0;

uint32_t g_back_color = 0xFFFF;
_rgblcd_dev rgblcddev;

/* 帧缓冲区：800*480*2 = 768KB，放外部RAM */
uint16_t g_ltdc_lcd_framebuf[800 * 480] __attribute__((section(".EXTRAM"), aligned(32)));

/* 缓冲区 0 */
uint16_t g_ltdc_lcd_framebuf_0[800 * 480] 
    __attribute__((section(".EXTRAM"))) 
    __attribute__((aligned(32)));

/* 缓冲区 1 */
uint16_t g_ltdc_lcd_framebuf_1[800 * 480] 
    __attribute__((section(".EXTRAM"))) 
    __attribute__((aligned(32)));
/* 缓冲区管理 */
static uint16_t *s_ltdc_display_buf = NULL;      /* LTDC 当前显示的缓冲区 */
static uint16_t *s_ltdc_pending_buf = NULL;      /* 待交换的缓冲区 */
static volatile uint8_t s_buf_swap_pending = 0;  /* 标志 */

/**
 * @brief 请求缓冲区交换 - 只设置标志，由 IRQ 真正执行
 */
void rgblcd_request_buffer_swap(uint32_t new_buf)
{
    s_ltdc_pending_buf = (uint16_t *)new_buf;
    s_buf_swap_pending = 1;
    /* 不要在这里碰 SRCR/CFBAR，让 IRQ 在 VBLANK 区域统一处理 */
}

/**
 * @brief LTDC VSYNC 中断处理（上层）
 * @note  中断优先级必须 >= 5（FreeRTOS 允许的范围）
 */
void LTDC_UP_IRQHandler_Impl(void)
{
    if (LTDC->ISR & LTDC_ISR_LIF) {
        LTDC->ICR |= LTDC_ICR_CLIF;
        g_lif_count_up++;
    }
    if (LTDC->ISR & LTDC_ISR_RRIF) {
        LTDC->ICR |= LTDC_ICR_CRRIF;
    }
}
extern osThreadId_t lvgl_mainHandle;  /* 你的 LVGL 任务句柄（已经定义好了）*/

void LTDC_LO_IRQHandler_Impl(void)
{
    if (LTDC->ISR & LTDC_ISR_LIF) {
        LTDC->ICR |= LTDC_ICR_CLIF;
        
    if (s_buf_swap_pending) {
            LTDC_Layer1->CFBAR = (uint32_t)s_ltdc_pending_buf;
            
            /* ★★★ 关键修改：VBR → IMR ★★★ */
            LTDC->SRCR |= LTDC_SRCR_IMR;   // 立即重载！
            
        s_ltdc_display_buf = s_ltdc_pending_buf;
        s_buf_swap_pending = 0;
            g_swap_done_count++;
            
            if (lvgl_mainHandle != NULL) {
                BaseType_t hpw = pdFALSE;
                vTaskNotifyGiveFromISR(lvgl_mainHandle, &hpw);
                portYIELD_FROM_ISR(hpw);
    }
}
        g_lif_count_lo++;
    }
    
    if (LTDC->ISR & LTDC_ISR_RRIF) {
        LTDC->ICR |= LTDC_ICR_CRRIF;
    }
        if (LTDC->ISR & LTDC_ISR_FUIF) {
        LTDC->ICR |= LTDC_ICR_CFUIF;
        g_fuif_count++;
    }
    if (LTDC->ISR & LTDC_ISR_TERRIF) {
        LTDC->ICR |= LTDC_ICR_CTERRIF;
        g_terrif_count++;
    }
}
/**
 * @brief 初始化 LTDC 帧缓冲和中断
 */
static void rgblcd_init_framebuffer(void)
{
    print_info_debug("[LTDC] Initializing framebuffer...\n");
    
    s_ltdc_display_buf = g_ltdc_lcd_framebuf_0;
    s_ltdc_pending_buf = g_ltdc_lcd_framebuf_0;

    memset(g_ltdc_lcd_framebuf_0, 0, sizeof(g_ltdc_lcd_framebuf_0));
    memset(g_ltdc_lcd_framebuf_1, 0, sizeof(g_ltdc_lcd_framebuf_1));

    LTDC_Layer1->CFBAR = (uint32_t)g_ltdc_lcd_framebuf_0;
    LTDC->SRCR |= LTDC_SRCR_IMR;

    /* 配置 LIE */
    LTDC->IER  &= ~LTDC_IER_RRIE;
    LTDC->LIPCR = 0;
    LTDC->ICR  |= LTDC_ICR_CLIF;
    LTDC->IER  |= LTDC_IER_LIE;

    /* ★★★ 关键改动：把 LO 和 UP 两个 NVIC 都启用 ★★★ */
    HAL_NVIC_SetPriority(LTDC_LO_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(LTDC_LO_IRQn);
    
    HAL_NVIC_SetPriority(LTDC_UP_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(LTDC_UP_IRQn);
    
    /* 打印两个中断号，验证是否对应得上 */
    print_info_debug("[LTDC] LTDC_LO_IRQn=%d, LTDC_UP_IRQn=%d\n", 
                     LTDC_LO_IRQn, LTDC_UP_IRQn);
    print_info_debug("[LTDC] NVIC_ISER[1]=0x%08X (LO bit %d)\n",
                     NVIC->ISER[1], LTDC_LO_IRQn % 32);
    print_info_debug("[LTDC] NVIC_ISER[6]=0x%08X (UP bit %d)\n",
                     NVIC->ISER[6], LTDC_UP_IRQn % 32);
}
// ! --------------------------------------------------------------
static uint8_t rgblcd_ltdc_clk_set(uint32_t clock);
static uint32_t rgblcd_pow(uint8_t m, uint8_t n);
void rgblcd_init(void)
{
    print_info_debug("[LTDC] rgblcd_init() called\n");
    LTDC_LayerCfgTypeDef ltdc_layer_cfg_struct = {0};

    /* 直接固定为 ATK-MD0700R-800480 参数 */
    rgblcddev.id      = 0x7084;
    rgblcddev.pwidth  = 800;
    rgblcddev.pheight = 480;
    rgblcddev.hsw     = 1;
    rgblcddev.vsw     = 1;
    rgblcddev.hbp     = 46;
    rgblcddev.vbp     = 23;
    rgblcddev.hfp     = 210;
    rgblcddev.vfp     = 22;

    print_info_debug("[LTDC] Timing parameters:\n");
    print_info_debug("  HSW=%d, VSW=%d\n", rgblcddev.hsw, rgblcddev.vsw);
    print_info_debug("  HBP=%d, VBP=%d\n", rgblcddev.hbp, rgblcddev.vbp);
    print_info_debug("  HFP=%d, VFP=%d\n", rgblcddev.hfp, rgblcddev.vfp);
    print_info_debug("  Width=%d, Height=%d\n", rgblcddev.pwidth, rgblcddev.pheight);

    /* 配置LTDC时序参数 */
    hltdc.Init.PCPolarity       = LTDC_PCPOLARITY_IPC;
    hltdc.Init.DEPolarity       = LTDC_DEPOLARITY_AL;
    hltdc.Init.VSPolarity       = LTDC_VSPOLARITY_AL;
    hltdc.Init.HSPolarity       = LTDC_HSPOLARITY_AL;
    
    hltdc.Init.HorizontalSync   = rgblcddev.hsw - 1;
    hltdc.Init.VerticalSync     = rgblcddev.vsw - 1;
    hltdc.Init.AccumulatedHBP   = rgblcddev.hsw + rgblcddev.hbp - 1;
    hltdc.Init.AccumulatedVBP   = rgblcddev.vsw + rgblcddev.vbp - 1;
    hltdc.Init.AccumulatedActiveW = rgblcddev.hsw + rgblcddev.hbp + rgblcddev.pwidth - 1;
    hltdc.Init.AccumulatedActiveH = rgblcddev.vsw + rgblcddev.vbp + rgblcddev.pheight - 1;
    hltdc.Init.TotalWidth       = rgblcddev.hsw + rgblcddev.hbp + rgblcddev.pwidth + rgblcddev.hfp - 1;
    hltdc.Init.TotalHeigh       = rgblcddev.vsw + rgblcddev.vbp + rgblcddev.pheight + rgblcddev.vfp - 1;
    
    hltdc.Init.Backcolor.Blue   = 0;
    hltdc.Init.Backcolor.Green  = 0;
    hltdc.Init.Backcolor.Red    = 0;
    
    HAL_LTDC_DeInit(&hltdc);
    if (HAL_LTDC_Init(&hltdc) != HAL_OK) {
        print_info_err("[LTDC] HAL_LTDC_Init() failed!\n");
        while(1);
    }

    /* ← 关键：在 HAL_LTDC_Init() 之后立即重新配置时钟 */
    print_info_debug("[LTDC] Configuring LTDC clock...\n");
    uint8_t clk_result = rgblcd_ltdc_clk_set(33333333);
    print_info_debug("[LTDC] Clock config result: %d\n", clk_result);
    
    /* ← 等待时钟稳定 */
    osDelay(10);
    
    print_info_debug("[LTDC] After rgblcd_ltdc_clk_set():\n");
    print_info_debug("  pll1_freq = %u\n", HAL_RCCEx_GetPLL1CLKFreq());

    /* 配置图层0 */
    ltdc_layer_cfg_struct.WindowX0       = 0;
    ltdc_layer_cfg_struct.WindowX1       = rgblcddev.pwidth;
    ltdc_layer_cfg_struct.WindowY0       = 0;
    ltdc_layer_cfg_struct.WindowY1       = rgblcddev.pheight;
    ltdc_layer_cfg_struct.PixelFormat    = LTDC_PIXEL_FORMAT_RGB565;
    ltdc_layer_cfg_struct.Alpha          = 255;
    ltdc_layer_cfg_struct.Alpha0         = 0;
    ltdc_layer_cfg_struct.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    ltdc_layer_cfg_struct.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
    ltdc_layer_cfg_struct.FBStartAdress  = 0;
    ltdc_layer_cfg_struct.ImageWidth     = rgblcddev.pwidth;
    ltdc_layer_cfg_struct.ImageHeight    = rgblcddev.pheight;
    ltdc_layer_cfg_struct.Backcolor.Blue  = 0;
    ltdc_layer_cfg_struct.Backcolor.Green = 0;
    ltdc_layer_cfg_struct.Backcolor.Red   = 0;
    HAL_LTDC_ConfigLayer(&hltdc, &ltdc_layer_cfg_struct, 0);
    HAL_LTDC_SetAddress(&hltdc, (uint32_t)g_ltdc_lcd_framebuf_0, 0);

    rgblcd_display_dir(1);
    rgblcd_clear(0xFFFF);
    RGBLCD_BL(1);
    
    print_info_debug("[LTDC] Calling rgblcd_init_framebuffer()...\n");
    rgblcd_init_framebuffer();

    /* ← 启动 LTDC */
    print_info_debug("[LTDC] Starting LTDC...\n");
    print_info_debug("[LTDC] Before: GCR = 0x%08X\n", LTDC->GCR);
    
    LTDC->GCR |= LTDC_GCR_LTDCEN;
    
    print_info_debug("[LTDC] After: GCR = 0x%08X\n", LTDC->GCR);
    print_info_debug("[LTDC] LTDC started successfully\n");
    
    /* 检查 LTDC 状态 */
    print_info_debug("[LTDC] LTDC->ISR = 0x%08X\n", LTDC->ISR);
    print_info_debug("[LTDC] LTDC->IER = 0x%08X\n", LTDC->IER);
    print_info_debug("[LTDC] LTDC->LIPCR = 0x%08X\n", LTDC->LIPCR);
    print_info_debug("[LTDC] LTDC->CPSR = 0x%08X\n", LTDC->CPSR);
    
    print_info_debug("[LTDC] rgblcd_init() completed\n");
}
/* rgblcd.c */
void rgblcd_display_dir(uint8_t dir)
{
    rgblcddev.dir = dir;
    if (dir == 1)  /* 1 = 横屏模式 */
    {
        rgblcddev.width  = 800;   /* 横屏宽度 */
        rgblcddev.height = 480;   /* 横屏高度 */
    }
    else  /* 0 = 竖屏模式 */
    {
        rgblcddev.width  = 480;   /* 竖屏宽度 */
        rgblcddev.height = 800;   /* 竖屏高度 */
    }
}
// void rgblcd_display_dir(uint8_t dir)
// {
//     rgblcddev.dir = dir;
//     if (dir != 0)
//     {
//         rgblcddev.width  = rgblcddev.pwidth;
//         rgblcddev.height = rgblcddev.pheight;
//     }
//     else
//     {
//         rgblcddev.width  = rgblcddev.pheight;
//         rgblcddev.height = rgblcddev.pwidth;
//     }
// }

void rgblcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
#define CONVERTRGB5652ARGB8888(Color) \
    ((((((((Color) >> 11U) & 0x1FU) * 527U) + 23U) >> 6U) << 16U) | \
     (((((((Color) >>  5U) & 0x3FU) * 259U) + 33U) >> 6U) <<  8U) | \
     ((((Color) & 0x1FU) * 527U + 23U) >> 6U) | 0xFF000000U)

    uint16_t psx, psy, pex, pey;

    if (rgblcddev.dir == 0)
    {
        psx = sy;  psy = rgblcddev.pheight - ex - 1;
        pex = ey;  pey = rgblcddev.pheight - sx - 1;
    }
    else
    {
        psx = sx; psy = sy; pex = ex; pey = ey;
    }

    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.OutputOffset = rgblcddev.pwidth - (pex - psx + 1);
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, CONVERTRGB5652ARGB8888(color),
                    (uint32_t)&g_ltdc_lcd_framebuf[psy * rgblcddev.pwidth + psx],
                    pex - psx + 1, pey - psy + 1);
    HAL_DMA2D_PollForTransfer(&hdma2d, 50);
}

void rgblcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t psx, psy, pex, pey;

    if (rgblcddev.dir == 0)
    {
        psx = sy;  psy = rgblcddev.pheight - ex - 1;
        pex = ey;  pey = rgblcddev.pheight - sx - 1;
    }
    else
    {
        psx = sx; psy = sy; pex = ex; pey = ey;
    }

    hdma2d.Init.Mode         = DMA2D_M2M;
    hdma2d.Init.OutputOffset = rgblcddev.pwidth - (pex - psx + 1);
    HAL_DMA2D_Init(&hdma2d);
    HAL_DMA2D_Start(&hdma2d, (uint32_t)color,
                    (uint32_t)&g_ltdc_lcd_framebuf[psy * rgblcddev.pwidth + psx],
                    pex - psx + 1, pey - psy + 1);
    HAL_DMA2D_PollForTransfer(&hdma2d, 50);
}

void rgblcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    uint16_t px, py;

    if (rgblcddev.dir == 0)
    {
        px = y;  py = rgblcddev.pheight - x - 1;
    }
    else
    {
        px = x;  py = y;
    }
    g_ltdc_lcd_framebuf[rgblcddev.pwidth * py + px] = color;
}

uint16_t rgblcd_read_point(uint16_t x, uint16_t y)
{
    uint16_t px, py;

    if (rgblcddev.dir == 0)
    {
        px = y;  py = rgblcddev.pheight - x - 1;
    }
    else
    {
        px = x;  py = y;
    }
    return g_ltdc_lcd_framebuf[rgblcddev.pwidth * py + px];
}

void rgblcd_clear(uint16_t color)
{
    rgblcd_fill(0, 0, rgblcddev.width - 1, rgblcddev.height - 1, color);
}

void rgblcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int xerr = 0, yerr = 0;
    int delta_x = x2 - x1, delta_y = y2 - y1;
    int incx = (delta_x > 0) ? 1 : (delta_x < 0 ? -1 : 0);
    int incy = (delta_y > 0) ? 1 : (delta_y < 0 ? -1 : 0);
    int row = x1, col = y1;

    if (delta_x < 0) delta_x = -delta_x;
    if (delta_y < 0) delta_y = -delta_y;
    int distance = (delta_x > delta_y) ? delta_x : delta_y;

    for (uint16_t t = 0; t <= distance + 1; t++)
    {
        rgblcd_draw_point(row, col, color);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) { xerr -= distance; row += incx; }
        if (yerr > distance) { yerr -= distance; col += incy; }
    }
}

void rgblcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x > rgblcddev.width) || (y > rgblcddev.height)) return;
    rgblcd_fill(x, y, x + len - 1, y, color);
}

void rgblcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    rgblcd_draw_line(x1, y1, x2, y1, color);
    rgblcd_draw_line(x1, y1, x1, y2, color);
    rgblcd_draw_line(x1, y2, x2, y2, color);
    rgblcd_draw_line(x2, y1, x2, y2, color);
}

void rgblcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a = 0, b = r, di = 3 - (r << 1);
    while (a <= b)
    {
        rgblcd_draw_point(x0+a, y0-b, color); rgblcd_draw_point(x0+b, y0-a, color);
        rgblcd_draw_point(x0+b, y0+a, color); rgblcd_draw_point(x0+a, y0+b, color);
        rgblcd_draw_point(x0-a, y0+b, color); rgblcd_draw_point(x0-b, y0+a, color);
        rgblcd_draw_point(x0-a, y0-b, color); rgblcd_draw_point(x0-b, y0-a, color);
        a++;
        di += (di < 0) ? (4 * a + 6) : (10 + 4 * (a - b--));
    }
}

void rgblcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
    uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
    uint32_t sqmax = (uint32_t)r * r + r / 2;
    uint32_t xr = r;

    rgblcd_draw_hline(x - r, y, 2 * r, color);
    for (uint32_t i = 1; i <= imax; i++)
    {
        if ((i * i + xr * xr) > sqmax)
        {
            if (xr > imax)
            {
                rgblcd_draw_hline(x - i + 1, y + xr, 2 * (i - 1), color);
                rgblcd_draw_hline(x - i + 1, y - xr, 2 * (i - 1), color);
            }
            xr--;
        }
        rgblcd_draw_hline(x - xr, y + i, 2 * xr, color);
        rgblcd_draw_hline(x - xr, y - i, 2 * xr, color);
    }
}

void rgblcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t csize = ((size >> 3) + (((size & 0x7) != 0) ? 1 : 0)) * (size >> 1);
    uint8_t *pfont;
    chr -= ' ';

    switch (size)
    {
        case 12: pfont = (uint8_t *)asc2_1206[(uint8_t)chr]; break;
        case 16: pfont = (uint8_t *)asc2_1608[(uint8_t)chr]; break;
        case 24: pfont = (uint8_t *)asc2_2412[(uint8_t)chr]; break;
        case 32: pfont = (uint8_t *)asc2_3216[(uint8_t)chr]; break;
        default: return;
    }

    uint16_t y0 = y;
    for (uint8_t t = 0; t < csize; t++)
    {
        uint8_t temp = pfont[t];
        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)       rgblcd_draw_point(x, y, color);
            else if (mode == 0)    rgblcd_draw_point(x, y, g_back_color);
            temp <<= 1;
            if (++y >= rgblcddev.height) return;
            if ((y - y0) == size) { y = y0; if (++x >= rgblcddev.width) return; break; }
        }
    }
}

void rgblcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)
{
    uint8_t enshow = 0;
    for (uint8_t t = 0; t < len; t++)
    {
        uint8_t temp = (num / rgblcd_pow(10, len - t - 1)) % 10;
        if (!enshow && t < (len - 1))
        {
            if (temp == 0) { rgblcd_show_char(x + (size >> 1) * t, y, ' ', size, 0, color); continue; }
            else enshow = 1;
        }
        rgblcd_show_char(x + (size >> 1) * t, y, temp + '0', size, 0, color);
    }
}

void rgblcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t enshow = 0;
    for (uint8_t t = 0; t < len; t++)
    {
        uint8_t temp = (num / rgblcd_pow(10, len - t - 1)) % 10;
        if (!enshow && t < (len - 1))
        {
            if (temp == 0)
            {
                char pad = (mode & 0x80) ? '0' : ' ';
                rgblcd_show_char(x + (size >> 1) * t, y, pad, size, mode & 0x01, color);
                continue;
            }
            else enshow = 1;
        }
        rgblcd_show_char(x + (size >> 1) * t, y, temp + '0', size, mode & 0x01, color);
    }
}

void rgblcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)
{
    uint8_t x0 = x;
    width += x; height += y;
    while (*p >= ' ' && *p <= '~')
    {
        if (x >= width)  { x = x0; y += size; }
        if (y >= height) break;
        rgblcd_show_char(x, y, *p, size, 0, color);
        x += (size >> 1);
        p++;
    }
}

static uint8_t rgblcd_ltdc_clk_set(uint32_t clock)
{
    RCC_PeriphCLKInitTypeDef rcc_periph_clk_init_struct = {0};
    uint32_t pll1_freq = HAL_RCCEx_GetPLL1CLKFreq();
    
    /* 调试：打印实际 PLL1 频率 */
    volatile uint32_t debug_pll1 = pll1_freq;  /* ← 在这里打断点，看 pll1_freq 是多少！ */
    
    uint32_t divider = pll1_freq / clock;
    
    rcc_periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    rcc_periph_clk_init_struct.LtdcClockSelection   = RCC_LTDCCLKSOURCE_IC16;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC16].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC16].ClockDivider   = divider;
    
    return (HAL_RCCEx_PeriphCLKConfig(&rcc_periph_clk_init_struct) != HAL_OK) ? 1 : 0;//
}
static uint32_t rgblcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}
/**
 * @brief 动态设置 LTDC 帧缓冲地址
 * @param addr 新的帧缓冲地址
 */
void rgblcd_set_framebuffer(uint32_t addr)
{
    /* 等待 VSYNC，确保在垂直消隐期间更新 */
    while ((LTDC->CDSR & LTDC_CDSR_VSYNCS) == 0);  /* 等待 VSYNC 开始 */
    while ((LTDC->CDSR & LTDC_CDSR_VSYNCS) != 0);  /* 等待 VSYNC 结束 */

    /* 更新帧缓冲地址 */
    LTDC_Layer1->CFBAR = addr;

    /* 重新加载配置 */
    LTDC->SRCR |= LTDC_SRCR_VBR;  /* 立即重新加载 */
}