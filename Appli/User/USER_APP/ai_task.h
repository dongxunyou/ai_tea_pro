/* ============================================================
 * ai_task.h — 茶叶 AI 推理任务接口
 * ============================================================ */
#ifndef AI_TASK_H
#define AI_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

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