#ifndef __TEA_UI_H
#define __TEA_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* 在 lvgl_main 里、LVGL/摄像头初始化完成后调用一次。
 * 内部会：建「开始画面」与「AI 识别画面」两个屏，建 50ms 周期 timer
 *         （刷新相机预览 + 识别结果），最后加载开始画面。*/
void tea_ui_init(void);
void ui_show_brewing(void);
void ui_show_main(void);

#ifdef __cplusplus
}
#endif

#endif /* __TEA_UI_H */
