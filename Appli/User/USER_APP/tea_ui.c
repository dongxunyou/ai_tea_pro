/* ============================================================
 * tea_ui.c — 界面层（LVGL v9）
 *
 * 2026-09-12 精简：剔除即热 / WiFi / 电机(舵机+编码器) / 抽水泵 四大模块，
 *   以及依赖它们的三个泡茶状态机、称重、温度、SD 卡、调试 GUI。
 *   保留的只有两件事：
 *     ① 开始画面（s_main_tile）  壁纸 + 品牌 + AI 识茶卡
 *     ② AI 识别画面（s_id_scr）   相机预览 + 识别按钮 + 类别/置信度/框
 *
 * 屏幕结构：
 *   s_main_scr  开始画面（主界面）
 *   s_id_scr    AI 识别画面（点首页 AI 卡进入，返回按钮回首页）
 *
 * 一个 50ms 的 LVGL timer：刷新相机预览，并在 g_t3_result_ready 置位后
 * 把识别结果刷到界面上。识别触发走 ai_task_trigger_infer()。
 * ============================================================ */

#include "tea_ui.h"

#include "lvgl.h"
#include "main.h"               /* SCB cache 操作 */
#include "ai_task.h"            /* ai_task_trigger_infer / T3Result_t / g_t3_result(_ready) */
#include "ai_model.h"           /* ai_model_set_mode / ai_model_get_mode / AI_MODE_* */
#include "ov5640_dcmipp.h"      /* cam_read_buf / CAM_W / CAM_H */

#include <stdio.h>
#include <string.h>

/* 主界面壁纸（RGB565 800×480），符号在 preview.c 里 */
extern const lv_image_dsc_t preview;

/* ============================================================
 * 配色 / 常量 —— 「茶韵」主题：宣纸底 + 松绿/朱砂/琥珀
 * 色板统一在 ui_theme.h
 * ============================================================ */
#include "ui_theme.h"
#define C_BG       UI_C_BG
#define C_PANEL    UI_C_PANEL
#define C_PANEL2   UI_C_PANEL2
#define C_ACCENT   UI_C_ACCENT
#define C_OK       UI_C_OK
#define C_WARN     UI_C_WARN
#define C_AMBER    UI_C_AMBER
#define C_TXT      UI_C_TXT
#define C_DIM      UI_C_DIM
#define C_LINE     UI_C_LINE
#define C_MIST     UI_C_MIST
#define C_BTNTXT   UI_C_BTNTXT

LV_FONT_DECLARE(lv_font_cn_14);
LV_FONT_DECLARE(lv_font_cn_16);
LV_FONT_DECLARE(lv_font_cn_24);
LV_FONT_DECLARE(lv_font_cn_28);
LV_FONT_DECLARE(lv_font_cn_36);

/* 保留 28/36 大号字库链接（放独立 section，由链接脚本 KEEP，防 gc-sections 裁剪）*/
const lv_font_t * const g_keep_fonts[] __attribute__((used, section(".keep_fonts"))) = {
    &lv_font_cn_28, &lv_font_cn_36,
};

#define SCR_W      800
#define SCR_H      480

/* 相机预览缩放目标尺寸（与 ai_task 的预处理无关，仅用于界面显示）*/
#define PREV_W     200
#define PREV_H     200

/* ============================================================
 * 屏幕 / 对象句柄
 * ============================================================ */
static lv_obj_t *s_main_scr = NULL;   /* 开始画面 */
static lv_obj_t *s_id_scr   = NULL;   /* AI 识别画面 */

static lv_obj_t *s_cam_canvas = NULL; /* 识别画面里的相机预览 canvas */
static lv_obj_t *s_cls_lbl    = NULL; /* 类别 */
static lv_obj_t *s_conf_lbl   = NULL; /* 置信度 */
static lv_obj_t *s_box_lbl    = NULL; /* 检测框 */
static lv_obj_t *s_btn_mode_tea = NULL; /* 模式按钮：茶叶分类 */
static lv_obj_t *s_btn_mode_pkg = NULL; /* 模式按钮：包装检测 */
static lv_obj_t *s_tip_lbl      = NULL; /* 提示文字（随模式变化） */
static lv_obj_t *s_prog_bar     = NULL; /* 识别进度条 */

/* 预览 RGB888 缓冲（cam_resize 写入，LVGL canvas 直接引用）*/
static uint8_t s_cam_prev[PREV_W * PREV_H * 3]
    __attribute__((aligned(32)));

/* 是否停在识别画面（决定是否刷新预览） */
static uint8_t s_on_id_scr = 0U;

/* ============================================================
 * 小控件工厂
 * ============================================================ */
static lv_obj_t *mk_label(lv_obj_t *p, const char *txt, uint32_t color)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, &lv_font_cn_16, LV_PART_MAIN);
    return l;
}

static lv_obj_t *mk_btn_solid(lv_obj_t *p, const char *txt, lv_event_cb_t cb,
                              void *ud, int32_t w, int32_t h, uint32_t color)
{
    lv_obj_t *b = lv_btn_create(p);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    if (cb != NULL) {
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    }
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_cn_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(C_BTNTXT), LV_PART_MAIN);
    lv_obj_center(l);
    return b;
}

/* HUD 小胶囊：圆点 + 文字 */
static lv_obj_t *hud_pill(lv_obj_t *p, const char *txt, uint32_t dot_color,
                          const lv_font_t *font)
{
    lv_obj_t *c = lv_obj_create(p);
    lv_obj_set_size(c, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(c, lv_color_hex(C_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(c, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(c, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(c, 4, LV_PART_MAIN);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    if (dot_color != 0U) {
        lv_obj_t *dot = lv_obj_create(c);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_bg_color(dot, lv_color_hex(dot_color), LV_PART_MAIN);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(dot, 0, LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(C_TXT), LV_PART_MAIN);
    return c;
}

/* ============================================================
 * 相机预览：RGB565(cam_read_buf) → RGB888 中心裁剪缩放
 * ============================================================ */
static void cam_resize_128(void)
{
    const uint8_t *frame = (const uint8_t *)cam_read_buf;
    SCB_InvalidateDCache_by_Addr((uint32_t *)frame, (int32_t)(CAM_W * CAM_H * 2));

    const uint32_t crop   = CAM_H;                /* 480：取中间正方形 */
    const uint32_t cx     = (CAM_W - crop) / 2U;  /* 160 */
    const uint32_t stride = CAM_W * 2U;

    for (uint32_t dy = 0U; dy < PREV_H; dy++) {
        uint32_t sy = (dy * crop) / PREV_H;
        const uint8_t *row = frame + sy * stride;
        for (uint32_t dx = 0U; dx < PREV_W; dx++) {
            uint32_t sx = cx + (dx * crop) / PREV_W;
            const uint8_t *p = row + sx * 2U;
            uint16_t px = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
            uint8_t r5 = (px >> 11) & 0x1FU, g6 = (px >> 5) & 0x3FU, b5 = px & 0x1FU;
            uint8_t *q = s_cam_prev + (dy * PREV_W + dx) * 3U;
            q[0] = (uint8_t)((b5 << 3) | (b5 >> 2));
            q[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
            q[2] = (uint8_t)((r5 << 3) | (r5 >> 2));
        }
    }
    SCB_CleanDCache_by_Addr((uint32_t *)s_cam_prev, (int32_t)(PREV_W * PREV_H * 3));
}

/* ============================================================
 * 导航
 * ============================================================ */
static void go_main(void)
{
    s_on_id_scr = 0U;
    lv_screen_load(s_main_scr);
}

static void go_id(void)
{
    s_on_id_scr = 1U;
    lv_screen_load(s_id_scr);
}

void ui_show_main(void) { go_main(); }

/* 前向声明 */
static void on_ai_card_click(lv_event_t *e);
static void on_back_to_main(lv_event_t *e);
static void on_identify(lv_event_t *e);
static void on_mode_tea(lv_event_t *e);
static void on_mode_pkg(lv_event_t *e);
static void ui_update_mode_buttons(void);

/* ============================================================
 * 开始画面（主界面）：壁纸 + 品牌 + AI 识茶卡
 * ============================================================ */
static void build_main_scr(void)
{
    lv_obj_set_scrollbar_mode(s_main_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(s_main_scr, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_main_scr, lv_color_hex(C_BG), LV_PART_MAIN);

    /* 壁纸铺满 */
    lv_obj_t *bg = lv_image_create(s_main_scr);
    lv_image_set_src(bg, &preview);
    lv_obj_set_pos(bg, 0, 0);

    /* 底部压暗带（提升控制带可读性）*/
    lv_obj_t *shade = lv_obj_create(s_main_scr);
    lv_obj_set_size(shade, SCR_W, 150);
    lv_obj_align(shade, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(shade, lv_color_hex(0x241D12), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shade, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(shade, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(shade, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(shade, 0, LV_PART_MAIN);
    lv_obj_clear_flag(shade, LV_OBJ_FLAG_SCROLLABLE);

    /* HUD 状态条 */
    lv_obj_t *h1 = hud_pill(s_main_scr, "SYSTEM ONLINE", C_OK, &lv_font_montserrat_14);
    lv_obj_align(h1, LV_ALIGN_TOP_LEFT, 14, 8);

    /* ── 品牌区（左上）：煮茗之芯 + 朱砂印章 ── */
    lv_obj_t *brand = lv_label_create(s_main_scr);
    lv_label_set_text(brand, "煮茗之芯");
    lv_obj_set_style_text_font(brand, &lv_font_cn_28, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(brand, 6, LV_PART_MAIN);
    lv_obj_set_style_text_color(brand, lv_color_hex(C_TXT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brand, lv_color_hex(C_PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(brand, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(brand, lv_color_hex(C_LINE), LV_PART_MAIN);
    lv_obj_set_style_border_width(brand, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(brand, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(brand, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(brand, 5, LV_PART_MAIN);
    lv_obj_set_pos(brand, 22, 44);

    lv_obj_t *seal = lv_obj_create(s_main_scr);
    lv_obj_set_size(seal, 31, 31);
    lv_obj_set_style_bg_color(seal, lv_color_hex(0xC9523F), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(seal, lv_color_hex(0x9E3325), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(seal, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_radius(seal, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(seal, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(seal, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(seal, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(seal, -45, LV_PART_MAIN);   /* -4.5°，盖章微歪 */
    lv_obj_clear_flag(seal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sl = lv_label_create(seal);
    lv_label_set_text(sl, "茶");
    lv_obj_set_style_text_color(sl, lv_color_hex(0xF6EFE0), LV_PART_MAIN);
    lv_obj_center(sl);
    lv_obj_align_to(seal, brand, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    lv_obj_t *sub = hud_pill(s_main_scr, "且将新火试新茶 · AI TEA SOMMELIER",
                             0, &lv_font_cn_14);
    lv_obj_align_to(sub, brand, LV_ALIGN_OUT_BOTTOM_LEFT, 6, 8);

    /* ── AI 核心卡（居中放大，深色 + 琥珀取景框角）── */
    lv_obj_t *ai = lv_btn_create(s_main_scr);
    lv_obj_set_size(ai, 392, 94);
    lv_obj_align(ai, LV_ALIGN_BOTTOM_MID, 0, -34);
    lv_obj_set_style_bg_color(ai, lv_color_hex(0x2A3D24), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ai, lv_color_hex(0x101A0F), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ai, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(ai, lv_color_hex(C_AMBER), LV_PART_MAIN);
    lv_obj_set_style_border_width(ai, 1, LV_PART_MAIN);
    lv_obj_set_style_border_opa(ai, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_radius(ai, 14, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ai, 22, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(ai, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(ai, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ai, lv_color_hex(0x14201A), LV_PART_MAIN);
    lv_obj_set_layout(ai, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ai, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ai, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ai, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(ai, on_ai_card_click, LV_EVENT_CLICKED, NULL);
    {   /* 行1：琥珀指示灯 + 主标题 */
        lv_obj_t *r1 = lv_obj_create(ai);
        lv_obj_set_size(r1, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(r1, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(r1, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(r1, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_column(r1, 9, LV_PART_MAIN);
        lv_obj_clear_flag(r1, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(r1, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *dot = lv_obj_create(r1);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_bg_color(dot, lv_color_hex(C_AMBER), LV_PART_MAIN);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(dot, 0, LV_PART_MAIN);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *t1 = lv_label_create(r1);
        lv_label_set_text(t1, "AI 识茶");
        lv_obj_set_style_text_font(t1, &lv_font_cn_28, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(t1, 3, LV_PART_MAIN);
        lv_obj_set_style_text_color(t1, lv_color_hex(0xF3E9C8), LV_PART_MAIN);
        /* 行2：流程小字 */
        lv_obj_t *t2 = lv_label_create(ai);
        lv_label_set_text(t2, "SCAN - RECOGNIZE");
        lv_obj_set_style_text_font(t2, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(t2, 4, LV_PART_MAIN);
        lv_obj_set_style_text_color(t2, lv_color_hex(C_AMBER), LV_PART_MAIN);
    }
    /* 取景框 4 角（L 形双线边）*/
    {
        static const lv_border_side_t CS[4] = {
            LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT,  LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_RIGHT,
            LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_LEFT, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT };
        static const int16_t CX[4] = { -5, 383, -5, 383 };
        static const int16_t CY[4] = { -5, -5, 85, 85 };
        for (int i = 0; i < 4; i++) {
            lv_obj_t *cn = lv_obj_create(ai);
            lv_obj_set_size(cn, 14, 14);
            lv_obj_set_pos(cn, CX[i], CY[i]);
            lv_obj_set_style_bg_opa(cn, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_color(cn, lv_color_hex(C_AMBER), LV_PART_MAIN);
            lv_obj_set_style_border_width(cn, 2, LV_PART_MAIN);
            lv_obj_set_style_border_side(cn, CS[i], LV_PART_MAIN);
            lv_obj_set_style_radius(cn, 4, LV_PART_MAIN);
            lv_obj_set_style_pad_all(cn, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(cn, 0, LV_PART_MAIN);
            lv_obj_clear_flag(cn, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(cn, LV_OBJ_FLAG_IGNORE_LAYOUT);
        }
    }
}

static void on_back_to_main(lv_event_t *e)
{
    (void)e;
    go_main();
}

static void on_ai_card_click(lv_event_t *e)
{
    (void)e;
    go_id();
}

/* ============================================================
 * AI 识别画面：相机预览 + 识别按钮 + 类别/置信度/框
 * ============================================================ */
static void build_id_scr(void)
{
    lv_obj_set_scrollbar_mode(s_id_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(s_id_scr, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_id_scr, lv_color_hex(C_BG), LV_PART_MAIN);

    /* 返回 */
    lv_obj_t *back = mk_btn_solid(s_id_scr, LV_SYMBOL_LEFT " 返回",
                                  on_back_to_main, NULL, 140, 46, C_ACCENT);
    lv_obj_set_pos(back, 16, 14);

    /* 左列：相机预览（放大到 400×400） */
    s_cam_canvas = lv_canvas_create(s_id_scr);
    lv_canvas_set_buffer(s_cam_canvas, s_cam_prev, PREV_W, PREV_H,
                         LV_COLOR_FORMAT_RGB888);
    lv_obj_set_size(s_cam_canvas, 400, 400);
    lv_obj_set_pos(s_cam_canvas, 10, 65);
    lv_obj_set_style_border_color(s_cam_canvas, lv_color_hex(C_LINE), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_cam_canvas, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(s_cam_canvas, 10, LV_PART_MAIN);

    /* 右列：识别结果 + 识别按钮（x=420，宽 370） */
    s_cls_lbl = mk_label(s_id_scr, "类别：-", C_TXT);
    lv_obj_set_style_text_font(s_cls_lbl, &lv_font_cn_36, LV_PART_MAIN);
    lv_obj_set_pos(s_cls_lbl, 420, 70);
    lv_obj_set_width(s_cls_lbl, 360);
    lv_label_set_long_mode(s_cls_lbl, LV_LABEL_LONG_WRAP);

    s_conf_lbl = mk_label(s_id_scr, "置信度：-", C_DIM);
    lv_obj_set_style_text_font(s_conf_lbl, &lv_font_cn_28, LV_PART_MAIN);
    lv_obj_set_pos(s_conf_lbl, 420, 160);

    s_box_lbl = mk_label(s_id_scr, "检测框：-", C_DIM);
    lv_obj_set_style_text_font(s_box_lbl, &lv_font_cn_28, LV_PART_MAIN);
    lv_obj_set_pos(s_box_lbl, 420, 205);

    /* 模式选择：点哪个跑哪个（TEA=茶叶分类 / PACKAGE=包装检测） */
    s_btn_mode_tea = mk_btn_solid(s_id_scr, "茶叶分类", on_mode_tea, NULL, 160, 50, C_ACCENT);
    lv_obj_set_pos(s_btn_mode_tea, 420, 250);

    s_btn_mode_pkg = mk_btn_solid(s_id_scr, "包装检测", on_mode_pkg, NULL, 160, 50, C_PANEL2);
    lv_obj_set_pos(s_btn_mode_pkg, 590, 250);

    s_tip_lbl = mk_label(s_id_scr, "对准茶叶，点识别", C_DIM);
    lv_obj_set_style_text_font(s_tip_lbl, &lv_font_cn_24, LV_PART_MAIN);
    lv_obj_set_pos(s_tip_lbl, 420, 310);

    /* 识别进度条（右列，识别期间显示 0-100%） */
    s_prog_bar = lv_bar_create(s_id_scr);
    lv_obj_set_size(s_prog_bar, 350, 16);
    lv_obj_set_pos(s_prog_bar, 420, 350);
    lv_bar_set_range(s_prog_bar, 0, 100);
    lv_bar_set_value(s_prog_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_prog_bar, lv_color_hex(C_PANEL2), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prog_bar, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_prog_bar, 7, LV_PART_MAIN);
    lv_obj_add_flag(s_prog_bar, LV_OBJ_FLAG_HIDDEN);

    /* 识别按钮（右列底部） */
    lv_obj_t *idbtn = mk_btn_solid(s_id_scr, LV_SYMBOL_EYE_OPEN "识别",
                                   on_identify, NULL, 350, 56, C_ACCENT);
    lv_obj_set_pos(idbtn, 420, 385);

    ui_update_mode_buttons();
}

/* 触发一次识别（AI 任务内部做 5 帧投票，结果写 g_t3_result）*/
static void on_identify(lv_event_t *e)
{
    (void)e;
    g_t3_result_ready = 0U;
    lv_label_set_text(s_cls_lbl, "识别中...");
    lv_label_set_text(s_conf_lbl, "");
    lv_label_set_text(s_box_lbl, "");
    if (s_prog_bar != NULL) {
        lv_obj_clear_flag(s_prog_bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(s_prog_bar, 0, LV_ANIM_OFF);
    }
    ai_task_trigger_infer();
}

/* ============================================================
 * 识别模式切换（茶叶分类 / 包装检测）
 * ------------------------------------------------------------
 * 点哪个按钮就 ai_model_set_mode(对应模式)，下一次「识别」跑对应权重：
 *   茶叶分类 → cls  4 类 @0x70A00000（320×320）
 *   包装检测 → detect 5 类 @0x70D00000（256×256）
 * ============================================================ */
static void ui_update_mode_buttons(void)
{
    int pkg = (ai_model_get_mode() == AI_MODE_PACKAGE);

    if (s_btn_mode_tea != NULL) {
        lv_obj_set_style_bg_color(s_btn_mode_tea,
                                  lv_color_hex(pkg ? C_PANEL2 : C_ACCENT), LV_PART_MAIN);
    }
    if (s_btn_mode_pkg != NULL) {
        lv_obj_set_style_bg_color(s_btn_mode_pkg,
                                  lv_color_hex(pkg ? C_ACCENT : C_PANEL2), LV_PART_MAIN);
    }
    if (s_tip_lbl != NULL) {
        lv_label_set_text(s_tip_lbl, pkg ? "对准茶叶包装，点识别"
                                         : "对准茶叶，点识别");
    }
}

static void on_mode_tea(lv_event_t *e)
{
    (void)e;
    ai_model_set_mode(AI_MODE_TEA);
    ui_update_mode_buttons();
    lv_label_set_text(s_cls_lbl, "类别：-");
    lv_label_set_text(s_conf_lbl, "置信度：-");
    lv_label_set_text(s_box_lbl, "检测框：-");
    g_t3_result_ready = 0U;
}

static void on_mode_pkg(lv_event_t *e)
{
    (void)e;
    ai_model_set_mode(AI_MODE_PACKAGE);
    ui_update_mode_buttons();
    lv_label_set_text(s_cls_lbl, "类别：-");
    lv_label_set_text(s_conf_lbl, "置信度：-");
    lv_label_set_text(s_box_lbl, "检测框：-");
    g_t3_result_ready = 0U;
}

/* ============================================================
 * 茶名显示层
 * ------------------------------------------------------------
 * 三分类改造后 ai_task.c 的 g_class_names[] 已直接输出中文
 * （红茶/绿茶/青柑普洱）；本函数保留 英文->中文 映射与"未识别"兜底。
 * ============================================================ */
static const char *tea_name_cn_from_str(const char *name)
{
    static const char *eng_names[] = {
        /* TEA 模式（cls 4 类） */
        "black",
        "green",
        "puer",
        "unknown",
        /* PACKAGE 模式（detect 5 类，ai_task.c 的 g_pkg_class_names） */
        "black_tea",
        "green_tea",
        "chrysanthemum_tea",
        "yinghong_black_tea",
        "green_tangerine_puer_tea",
    };
    static const char *cn_names[] = {
        /* TEA 模式 */
        "红茶",
        "绿茶",
        "青柑普洱",
        "未识别",
        /* PACKAGE 模式 */
        "红茶包装",
        "绿茶包装",
        "菊花茶包装",
        "英红红茶包装",
        "小青柑普洱茶包装",
    };

    if (name == NULL) return "未识别";
    if (strcmp(name, "---") == 0) return "未识别";

    for (size_t i = 0U; i < sizeof(eng_names) / sizeof(eng_names[0]); i++) {
        if (strcmp(name, eng_names[i]) == 0) {
            return cn_names[i];
        }
    }
    return name;   /* 已是中文则原样返回 */
}

/* ============================================================
 * 50ms 周期刷新
 * ============================================================ */
static void tick_cb(lv_timer_t *tmr)
{
    (void)tmr;

    /* 识别画面：刷新相机预览（推理期间跳过缩放，减少 XSPI2 取指争用） */
    if (s_on_id_scr && (s_cam_canvas != NULL) && (g_ai_infer_busy == 0U)) {
        cam_resize_128();
        lv_obj_invalidate(s_cam_canvas);
    }

    /* 识别进度条：推理期间按帧刷新 */
    if (s_prog_bar != NULL) {
        if (g_ai_infer_busy != 0U) {
            lv_obj_clear_flag(s_prog_bar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(s_prog_bar, g_ai_infer_progress, LV_ANIM_OFF);
        }
    }

    /* 结果到达：刷到界面上 */
    if (g_t3_result_ready != 0U) {
        g_t3_result_ready = 0U;

        if (s_prog_bar != NULL) {
            lv_bar_set_value(s_prog_bar, 100, LV_ANIM_OFF);
        }

        const char *name = g_t3_result.class_name;
        float       conf = g_t3_result.confidence;

        /* 显示中文茶名（g_class_names 已是中文，函数做兜底映射）*/
        lv_label_set_text_fmt(s_cls_lbl, "类别：%s", tea_name_cn_from_str(name));
        lv_label_set_text_fmt(s_conf_lbl, "置信度：%.2f%%", (double)conf * 100.0);

        if (g_t3_result.box_valid != 0U) {
            lv_label_set_text_fmt(s_box_lbl, "框：%d,%d %dx%d",
                                  (int)g_t3_result.box_x, (int)g_t3_result.box_y,
                                  (int)g_t3_result.box_w, (int)g_t3_result.box_h);
        } else {
            lv_label_set_text(s_box_lbl, "检测框：无");
        }
    }
}

/* ============================================================
 * 入口
 * ============================================================ */
void tea_ui_init(void)
{
    s_main_scr = lv_obj_create(NULL);
    build_main_scr();

    s_id_scr = lv_obj_create(NULL);
    build_id_scr();

    lv_screen_load(s_main_scr);
    s_on_id_scr = 0U;

    lv_timer_create(tick_cb, 50, NULL);
}

void ui_show_brewing(void) { ui_show_main(); }
