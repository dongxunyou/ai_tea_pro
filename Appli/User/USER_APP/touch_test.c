/**
 * @file    touch_test.c
 * @brief   GT911 五点触摸测试 Demo
 *
 * 界面布局（800 × 480）：
 * ┌─────────────────────────────────────────────────┐ y=0
 * │  GT911 Touch Test      Pts: 0/5       [CLEAR]   │ H=50
 * ├─────────────────────────────────────────────────┤ y=50
 * │                                                 │
 * │           黑色绘图区（触摸轨迹落点）              │ H=360
 * │     彩色跟随圆圈浮在所有 UI 之上                  │
 * │                                                 │
 * ├─────────────────────────────────────────────────┤ y=410
 * │ P1(xxx,xxx)  P2(xxx,xxx)  P3(xxx,xxx)           │
 * │ P4(xxx,xxx)  P5(xxx,xxx)                        │ H=70
 * └─────────────────────────────────────────────────┘ y=480
 */

#include "touch_test.h"
#include "lv_port_indev.h"
#include "lvgl.h"

/*================================================================
 * 布局常量
 *================================================================*/
#define SCR_W       800
#define SCR_H       480
#define HDR_H       50
#define FTR_H       70
#define AREA_Y      HDR_H
#define AREA_H      (SCR_H - HDR_H - FTR_H)    /* 360 */
#define FTR_Y       (SCR_H - FTR_H)             /* 410 */

/*================================================================
 * 触摸样式常量
 *================================================================*/
#define FDOT_D      40      /* 跟随手指的大圆直径（px） */
#define TDOT_D      8       /* 轨迹落点小圆直径（px）   */
#define MAX_TP      5       /* 最大触摸点数             */
#define MAX_TRAIL   300     /* 轨迹点上限，超出自动清屏  */

/*================================================================
 * 5个触摸点的颜色（0xRRGGBB）
 *================================================================*/
static const uint32_t TP_COLOR[MAX_TP] = {
    0xFF5555,   /* P1 - 珊瑚红  */
    0x55DD55,   /* P2 - 草绿    */
    0x55AAFF,   /* P3 - 天蓝    */
    0xFFCC33,   /* P4 - 明黄    */
    0xDD55FF,   /* P5 - 紫罗兰  */
};

/*================================================================
 * 静态控件句柄
 *================================================================*/
static lv_obj_t  *s_scr           = NULL;
static lv_obj_t  *s_draw          = NULL;           /* 绘图区容器        */
static lv_obj_t  *s_fdot[MAX_TP]  = {NULL};         /* 跟随手指的大圆圈  */
static lv_obj_t  *s_coord[MAX_TP] = {NULL};         /* 底部坐标标签      */
static lv_obj_t  *s_cnt_lbl       = NULL;           /* 触摸点数量标签    */
static lv_timer_t *s_timer        = NULL;
static uint16_t   s_trail_cnt     = 0;              /* 当前轨迹点计数    */

/*----------------------------------------------------------------
 * 清除绘图区（删除所有轨迹 + 重新显示提示文字）
 *----------------------------------------------------------------*/
static void do_clear(void)
{
    lv_obj_clean(s_draw);
    s_trail_cnt = 0;

    lv_obj_t *hint = lv_label_create(s_draw);
    lv_label_set_text(hint, "Touch here...");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x7E7663), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_28, 0);
    lv_obj_center(hint);
}

static void clear_btn_cb(lv_event_t *e) { (void)e; do_clear(); }

/*----------------------------------------------------------------
 * 在绘图区添加一个轨迹落点
 * @param sx, sy  屏幕绝对坐标
 *----------------------------------------------------------------*/
static void add_trail_dot(int32_t sx, int32_t sy, uint32_t color_hex)
{
    /* 过滤绘图区范围外的坐标 */
    if (sy < AREA_Y || sy >= (AREA_Y + AREA_H)) return;
    if (sx < 0 || sx >= SCR_W) return;

    /* 超出轨迹上限：自动清屏 */
    if (s_trail_cnt >= MAX_TRAIL)
    {
        do_clear();
    }

    lv_obj_t *dot = lv_obj_create(s_draw);
    lv_obj_set_size(dot, TDOT_D, TDOT_D);
    /* 转换为绘图区的局部坐标 */
    lv_obj_set_pos(dot,
                   sx - TDOT_D / 2,
                   sy - AREA_Y - TDOT_D / 2);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color_hex), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_80, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    s_trail_cnt++;
}

/*----------------------------------------------------------------
 * LVGL 定时器回调（每 20ms）
 * 从共享变量读触摸数据，不再重复扫描 I2C
 *----------------------------------------------------------------*/
static void touch_timer_cb(lv_timer_t *t)
{
    (void)t;

    GT9XXX_Data_t td = {0};
    lv_port_indev_get_touch(&td);   /* 读取上一次 indev 回调存下来的数据 */

    char buf[24];

    /* 更新触摸点数量 */
    lv_snprintf(buf, sizeof(buf), "Pts: %d / %d", (int)td.num, MAX_TP);
    lv_label_set_text(s_cnt_lbl, buf);

    for (int i = 0; i < MAX_TP; i++)
    {
        if (i < (int)td.num)
        {
            int32_t sx = (int32_t)td.pt[i].x;
            int32_t sy = (int32_t)td.pt[i].y;

            /* 移动大圆圈跟随手指（s_fdot 是 scr 的子对象，坐标 = 屏幕坐标） */
            lv_obj_set_pos(s_fdot[i], sx - FDOT_D / 2, sy - FDOT_D / 2);
            lv_obj_clear_flag(s_fdot[i], LV_OBJ_FLAG_HIDDEN);

            /* 添加轨迹落点 */
            add_trail_dot(sx, sy, TP_COLOR[i]);

            /* 更新坐标标签 */
            lv_snprintf(buf, sizeof(buf), "P%d(%3d,%3d)", i + 1, (int)sx, (int)sy);
            lv_label_set_text(s_coord[i], buf);
        }
        else
        {
            lv_obj_add_flag(s_fdot[i], LV_OBJ_FLAG_HIDDEN);

            lv_snprintf(buf, sizeof(buf), "P%d(---,---)", i + 1);
            lv_label_set_text(s_coord[i], buf);
        }
    }
}

/*================================================================
 * touch_test_run() - 对外入口
 *================================================================*/
void touch_test_run(void)
{
    /*----------------------------------------------------------------
     * 主屏幕
     *----------------------------------------------------------------*/
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0xEBE2C8), 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);         /* ← 必须清零，保证坐标对齐 */
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(s_scr);

    /*----------------------------------------------------------------
     * Header（标题栏）
     *----------------------------------------------------------------*/
    lv_obj_t *hdr = lv_obj_create(s_scr);
    lv_obj_set_size(hdr, SCR_W, HDR_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0xFBF4DE), 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);

    /* 标题文字 */
    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "GT911 Touch Test");
    lv_obj_set_style_text_color(title, lv_color_hex(0x292520), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    /* 触摸点数量标签 */
    s_cnt_lbl = lv_label_create(hdr);
    lv_label_set_text(s_cnt_lbl, "Pts: 0 / 5");
    lv_obj_set_style_text_color(s_cnt_lbl, lv_color_hex(0x5C955A), 0);
    lv_obj_set_style_text_font(s_cnt_lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(s_cnt_lbl, LV_ALIGN_CENTER, 0, 0);

    /* CLEAR 按钮 */
    lv_obj_t *btn = lv_button_create(hdr);   /* LVGL v9 API */
    lv_obj_set_size(btn, 80, 36);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xBF4030), 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_add_event_cb(btn, clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "CLEAR");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_lbl);

    /*----------------------------------------------------------------
     * 绘图区（黑色背景，用于展示触摸轨迹）
     *----------------------------------------------------------------*/
    s_draw = lv_obj_create(s_scr);
    lv_obj_set_size(s_draw, SCR_W, AREA_H);
    lv_obj_set_pos(s_draw, 0, AREA_Y);
    lv_obj_clear_flag(s_draw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_draw, lv_color_hex(0xFBF4DE), 0);
    lv_obj_set_style_border_width(s_draw, 0, 0);
    lv_obj_set_style_radius(s_draw, 0, 0);
    lv_obj_set_style_pad_all(s_draw, 0, 0);

    /* 初始提示文字 */
    lv_obj_t *hint = lv_label_create(s_draw);
    lv_label_set_text(hint, "Touch here...");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x7E7663), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_28, 0);
    lv_obj_center(hint);

    /*----------------------------------------------------------------
     * Footer（坐标信息区）
     *----------------------------------------------------------------*/
    lv_obj_t *ftr = lv_obj_create(s_scr);
    lv_obj_set_size(ftr, SCR_W, FTR_H);
    lv_obj_set_pos(ftr, 0, FTR_Y);
    lv_obj_clear_flag(ftr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ftr, lv_color_hex(0xFBF4DE), 0);
    lv_obj_set_style_border_width(ftr, 0, 0);
    lv_obj_set_style_radius(ftr, 0, 0);
    lv_obj_set_style_pad_all(ftr, 6, 0);

    /* 第一行：P1 P2 P3 */
    for (int i = 0; i < 3; i++)
    {
        char init[16];
        lv_snprintf(init, sizeof(init), "P%d(---,---)", i + 1);
        s_coord[i] = lv_label_create(ftr);
        lv_label_set_text(s_coord[i], init);
        lv_obj_set_style_text_color(s_coord[i], lv_color_hex(TP_COLOR[i]), 0);
        lv_obj_set_style_text_font(s_coord[i], &lv_font_montserrat_14, 0);
        lv_obj_set_pos(s_coord[i], 8 + i * 260, 4);
    }

    /* 第二行：P4 P5 */
    for (int i = 3; i < 5; i++)
    {
        char init[16];
        lv_snprintf(init, sizeof(init), "P%d(---,---)", i + 1);
        s_coord[i] = lv_label_create(ftr);
        lv_label_set_text(s_coord[i], init);
        lv_obj_set_style_text_color(s_coord[i], lv_color_hex(TP_COLOR[i]), 0);
        lv_obj_set_style_text_font(s_coord[i], &lv_font_montserrat_14, 0);
        lv_obj_set_pos(s_coord[i], 8 + (i - 3) * 260, 36);
    }

    /*----------------------------------------------------------------
     * 跟随手指的大圆圈
     * 作为 s_scr 的最后一批子对象添加 → 渲染层级最高，覆盖在所有 UI 之上
     * s_scr 的 pad=0，所以 set_pos 直接使用屏幕绝对坐标
     *----------------------------------------------------------------*/
    for (int i = 0; i < MAX_TP; i++)
    {
        s_fdot[i] = lv_obj_create(s_scr);
        lv_obj_set_size(s_fdot[i], FDOT_D, FDOT_D);
        lv_obj_set_style_radius(s_fdot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_fdot[i], lv_color_hex(TP_COLOR[i]), 0);
        lv_obj_set_style_bg_opa(s_fdot[i], LV_OPA_70, 0);
        lv_obj_set_style_border_color(s_fdot[i], lv_color_white(), 0);
        lv_obj_set_style_border_width(s_fdot[i], 2, 0);
        lv_obj_set_style_shadow_width(s_fdot[i], 14, 0);
        lv_obj_set_style_shadow_color(s_fdot[i], lv_color_hex(TP_COLOR[i]), 0);
        lv_obj_set_style_shadow_opa(s_fdot[i], LV_OPA_50, 0);
        lv_obj_set_style_pad_all(s_fdot[i], 0, 0);
        lv_obj_clear_flag(s_fdot[i],
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_fdot[i], LV_OBJ_FLAG_HIDDEN);    /* 初始隐藏 */

        /* 圆圈中心的点号 */
        lv_obj_t *num = lv_label_create(s_fdot[i]);
        char txt[4];
        lv_snprintf(txt, sizeof(txt), "P%d", i + 1);
        lv_label_set_text(num, txt);
        lv_obj_set_style_text_color(num, lv_color_white(), 0);
        lv_obj_set_style_text_font(num, &lv_font_montserrat_14, 0);
        lv_obj_center(num);
    }

    /*----------------------------------------------------------------
     * 启动扫描定时器（20ms）
     *----------------------------------------------------------------*/
    s_timer = lv_timer_create(touch_timer_cb, 20, NULL);
}