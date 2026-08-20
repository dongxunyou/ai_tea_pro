#ifndef  __GUI_H_
#define __GUI_H_
#include "lvgl.h"
#include "main.h"

/* 摄像头显示相关 */
#define CAM_W  800
#define CAM_H  480

/* AI 识别界面入口（lvgl_main 中调用一次） */
void lv_ai_gui_init(void);

/* LVGL 主循环刷新摄像头画面 */
void lv_obj_invalidate_cam(void);
extern volatile uint8_t g_cam_frame_ready;

/* ai_task → LVGL 任务的异步通知标志 */
extern volatile uint8_t g_t3_result_ready;

/* 推理结果暂存（ai_task 写，LVGL 定时器读）*/
typedef struct {
    char  class_name[32];
    float confidence;
    volatile int      box_x, box_y, box_w, box_h;
    volatile uint8_t  box_valid;
} T3Result_t;
extern T3Result_t g_t3_result;

#endif // ! __GUI_H_
