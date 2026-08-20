/* ============================================================
 * gui.c — ai_tea_pro 精简版 AI 识别界面
 *
 * 布局（800x480）：
 *   全屏摄像头画面（RGB565 canvas）
 *   + 检测框覆盖层（绿框 + 中文茶名标签）
 *   + 右上角"识别"按钮（触发突发推理）
 *   + 底部字幕显示识别结果（subtitle.c，中文字体 lv_font_cn_24）
 *
 * 数据流：
 *   DCMIPP 帧中断 → HAL_DCMIPP_PIPE_FrameEventCallback → cam_read_buf 翻转
 *   lvgl_main 轮询 g_cam_frame_ready → lv_obj_invalidate_cam() 全屏刷新
 *   按钮 → ai_task_trigger_infer() → ai_task 写 g_t3_result → 100ms 定时器读
 * ============================================================ */
#include "gui.h"
#include "ai_task.h"
#include "ov5640_dcmipp.h"
#include "subtitle.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_cn_24);

/* ============================================================
 * 全局变量（ai_task.c 链接契约，勿改名）
 * ============================================================ */
volatile uint8_t g_cam_frame_ready = 0;
volatile uint8_t g_t3_result_ready = 0U;
T3Result_t       g_t3_result       = { "---", 0.0f };

/* ============================================================
 * 静态 UI 对象
 * ============================================================ */
static lv_obj_t *cam_canvas    = NULL;
static lv_obj_t *s_det_box     = NULL;
static lv_obj_t *s_det_box_lbl = NULL;

/* ============================================================
 * 中文茶名映射（与 ai_task.c g_class_names 顺序一致）
 * 换模型改类别时，同步修改此表与 ai_task.c
 * ============================================================ */
static const char *tea_name_cn_from_str(const char *eng)
{
    static const char *eng_names[] = {
        "black_tea",
        "green_tea",
        "chrysanthemum_tea",
        "yinghong_black_tea",
        "green_tangerine_puer_tea",
    };
    static const char *cn_names[] = {
        "红茶",
        "绿茶",
        "菊花茶",
        "英红红茶",
        "小青柑普洱茶",
    };
    for (size_t i = 0; i < sizeof(eng_names) / sizeof(eng_names[0]); i++) {
        if (strcmp(eng, eng_names[i]) == 0)
            return cn_names[i];
    }
    return eng; /* fallback: 显示原文 */
}

/* ============================================================
 * DCMIPP 帧完成回调（硬件双缓冲，中断上下文）
 * 帧1→buf0, 帧2→buf1, 帧3→buf0 ... 硬件自动切换写入目标
 * ============================================================ */
void HAL_DCMIPP_PIPE_FrameEventCallback(DCMIPP_HandleTypeDef *hdcmipp,
                                        uint32_t Pipe)
{
    (void)hdcmipp;
    if (Pipe != DCMIPP_PIPE0) return;

    static uint8_t cur = 0;
    cam_read_buf = (cur == 0) ? ov5640_dcmipp_buf0 : ov5640_dcmipp_buf1;
    cur ^= 1;

    g_cam_frame_count++;
    g_cam_frame_ready = 1;
}

/* ============================================================
 * LVGL 刷新接口（lvgl_main 主循环中调用）
 * 外部 RAM 经 DMA 写入后 CPU D-Cache 可能脏，先 Invalidate 再让 LVGL 读
 * ============================================================ */
void lv_obj_invalidate_cam(void)
{
    if (cam_canvas == NULL) return;

    SCB_InvalidateDCache_by_Addr((uint32_t *)cam_read_buf, CAM_BUF_SIZE);

    lv_canvas_set_buffer(cam_canvas,
                         (void *)cam_read_buf,
                         CAM_W, CAM_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_invalidate(cam_canvas);
}

/* ============================================================
 * 识别按钮回调：触发一次突发推理
 * ============================================================ */
static void infer_btn_cb(lv_event_t *e)
{
    (void)e;
    g_t3_result_ready = 0U;   /* 丢弃上一次旧结果 */
    ai_task_trigger_infer();
    Subtitle_Set("识别中...");
}

/* ============================================================
 * 100ms 定时器：检测框跟手刷新 + 识别结果上屏
 * ============================================================ */
static void ui_refresh_cb(lv_timer_t *tmr)
{
    (void)tmr;

    /* 检测框实时更新 */
    if (s_det_box && g_t3_result.box_valid) {
        lv_obj_set_pos(s_det_box, g_t3_result.box_x, g_t3_result.box_y);
        lv_obj_set_size(s_det_box, g_t3_result.box_w, g_t3_result.box_h);
        if (s_det_box_lbl) {
            lv_label_set_text_fmt(s_det_box_lbl, "%s %d%%",
                                  tea_name_cn_from_str(g_t3_result.class_name),
                                  (int)(g_t3_result.confidence * 100.0f));
        }
        lv_obj_clear_flag(s_det_box, LV_OBJ_FLAG_HIDDEN);
    } else if (s_det_box) {
        lv_obj_add_flag(s_det_box, LV_OBJ_FLAG_HIDDEN);
    }

    /* 识别结果就绪 → 字幕显示 */
    if (g_t3_result_ready) {
        g_t3_result_ready = 0U;

        if (g_t3_result.box_valid) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%s  %.1f%%",
                     tea_name_cn_from_str(g_t3_result.class_name),
                     g_t3_result.confidence * 100.0f);
            Subtitle_Set(buf);
        } else {
            Subtitle_Set("未识别到茶叶，请重试");
        }
    }
}

/* ============================================================
 * 界面构建（lvgl_main 中调用一次）
 * ============================================================ */
void lv_ai_gui_init(void)
{
    /* 首帧未完成前 cam_read_buf 为 NULL，先指向 buf0 防止空指针 */
    if (cam_read_buf == NULL) {
        cam_read_buf = ov5640_dcmipp_buf0;
    }

    /* ── 全屏摄像头画面 ── */
    cam_canvas = lv_canvas_create(lv_screen_active());
    lv_canvas_set_buffer(cam_canvas,
                         (void *)cam_read_buf,
                         CAM_W, CAM_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(cam_canvas, CAM_W, CAM_H);
    lv_obj_center(cam_canvas);

    /* ── 检测框覆盖层 ── */
    s_det_box = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_opa(s_det_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_det_box, lv_color_hex(0x5C955A), 0);
    lv_obj_set_style_border_width(s_det_box, 3, 0);
    lv_obj_set_style_radius(s_det_box, 0, 0);
    lv_obj_set_style_pad_all(s_det_box, 0, 0);
    lv_obj_clear_flag(s_det_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_det_box, LV_OBJ_FLAG_HIDDEN);

    s_det_box_lbl = lv_label_create(s_det_box);
    lv_obj_set_style_text_font(s_det_box_lbl, &lv_font_cn_24, 0);
    lv_obj_set_style_bg_opa(s_det_box_lbl, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(s_det_box_lbl, lv_color_hex(0x5C955A), 0);
    lv_obj_set_style_text_color(s_det_box_lbl, lv_color_hex(0x000000), 0);
    lv_label_set_text(s_det_box_lbl, "---");
    lv_obj_align(s_det_box_lbl, LV_ALIGN_OUT_TOP_LEFT, 0, 0);

    /* ── 识别按钮（右上角） ── */
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 56);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -16, 16);
    lv_obj_add_event_cb(btn, infer_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_cn_24, 0);
    lv_label_set_text(btn_lbl, "识别");
    lv_obj_center(btn_lbl);

    /* ── 100ms 刷新定时器 ── */
    lv_timer_create(ui_refresh_cb, 100, NULL);
}
