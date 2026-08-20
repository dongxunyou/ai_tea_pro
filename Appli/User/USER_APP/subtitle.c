#include "subtitle.h"

LV_FONT_DECLARE(lv_font_cn_24);
#define SUB_CAM_W  800

static lv_obj_t *s_sub = NULL;

void Subtitle_Init(void)
{
    s_sub = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(s_sub, &lv_font_cn_24, 0);
    lv_obj_set_width(s_sub, SUB_CAM_W);
    lv_obj_set_style_text_align(s_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_sub, LV_ALIGN_BOTTOM_MID, 0, -15);
    Subtitle_Clear();
}

void Subtitle_Set(const char *utf8)
{
    if (!s_sub) return;
    lv_label_set_text(s_sub, utf8);
    lv_obj_clear_flag(s_sub, LV_OBJ_FLAG_HIDDEN);
}

void Subtitle_Clear(void)
{
    if (!s_sub) return;
    lv_label_set_text(s_sub, "");
    lv_obj_add_flag(s_sub, LV_OBJ_FLAG_HIDDEN);
}
