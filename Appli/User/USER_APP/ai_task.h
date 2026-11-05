/* ============================================================
 * ai_task.h — 茶叶 AI 推理任务接口
 * ============================================================ */
#ifndef AI_TASK_H
#define AI_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 推理结果暂存（ai_task 写，LVGL 定时器读）
 * ------------------------------------------------------------
 * 2026-09-12: 原定义在 gui.h / gui.c（舵机调试 GUI），随 gui.c 删除迁移至此。
 *   生产者 = ai_task.c（唯一写者），消费者 = tea_ui.c 的结果刷新。
 *   放在这里语义最贴切：结果由 AI 任务产出，对外只暴露读取接口。
 * ============================================================ */
typedef struct {
    char  class_name[32];
    float confidence;
    volatile int      box_x, box_y, box_w, box_h;
    volatile uint8_t  box_valid;
} T3Result_t;

extern T3Result_t g_t3_result;

/* ai_task → LVGL 任务的异步通知标志：ai_task 写完 g_t3_result 后置 1 */
extern volatile uint8_t g_t3_result_ready;

/* 推理进行中标志（ai_task 置位；UI 定时器据此暂停相机预览缩放，
 * 减少 CPU 从 XSPI2 取指，把总线让给 NPU 读权重） */
extern volatile uint8_t g_ai_infer_busy;

/**
 * @brief FreeRTOS 任务入口
 *        建议栈 ≥ 4096 字节
 */
void ai_task(void *arg);

/**
 * @brief 触发一次推理（线程安全，可在任意上下文调用）
 */
void ai_task_trigger_infer(void);

#ifdef __cplusplus
}
#endif
#endif /* AI_TASK_H */