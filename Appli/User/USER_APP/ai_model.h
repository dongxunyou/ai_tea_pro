#ifndef AI_MODEL_H
#define AI_MODEL_H

#include <stdint.h>

/* 输入参数（来自 08_stedgeai_gen.bat 的 generate 报告）
 * 模型：YOLOv8n-cls 四分类（black/green/puer/unknown），
 * 预处理：center-square-crop 480x480 -> 直接缩放到 320x320（与训练一致） */
#define AI_INPUT_WIDTH     320
#define AI_INPUT_HEIGHT    320
#define AI_INPUT_CHANNELS  3
#define AI_INPUT_SIZE      (AI_INPUT_WIDTH * AI_INPUT_HEIGHT * AI_INPUT_CHANNELS)

/* 输出参数：4 类 logits（[0]black [1]green [2]puer [3]unknown） */
#define AI_NUM_CLASSES     4

/* 输出 buffer 为 INT8 量化直出（4 字节）；
 * 反量化在 ai_model.c 内部完成，ai_model_run 对外仍返回 float* logits 指针 */
#define AI_OUT_SIZE_BYTES  4

/* 初始化 NPU 运行时（调度器启动后、推理前调用一次）*/
int  ai_model_init(void);

/* 跑一次推理。
 * input:  320x320x3 uint8 (channel-first / NCHW，RGB 序，值域 0-255)
 *         （pixel->int8 量化转换由 ai_model.c 内部按 network.c 的
 *           "Input_0_out_0" scale/zero_point 完成）
 * output: 返回内部 float32 logits 数组指针 [AI_NUM_CLASSES]（不要 free）
 * 返回 0=成功，<0=错误 */
int  ai_model_run(const uint8_t *input, const float  **output);

void ai_model_deinit(void);

#endif /* AI_MODEL_H */
