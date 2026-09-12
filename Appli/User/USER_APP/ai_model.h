#ifndef AI_MODEL_H
#define AI_MODEL_H

#include <stdint.h>

/* 输入参数（来自 generate 报告）*/
#define AI_INPUT_WIDTH     256
#define AI_INPUT_HEIGHT    256
#define AI_INPUT_CHANNELS  3
#define AI_INPUT_SIZE      (AI_INPUT_WIDTH * AI_INPUT_HEIGHT * AI_INPUT_CHANNELS)

/* 输出参数 */
#define AI_NUM_DETECTIONS  1344
#define AI_NUM_VALUES      9
#define AI_NUM_CLASSES     5

/* 输出是 float32 直出（no dequant），chfirst [9][1344] */
#define AI_OUT_SIZE_BYTES  48384  /* 9 * 1344 * sizeof(float) */

/* 初始化 NPU 运行时（调度器启动后、推理前调用一次）*/
int  ai_model_init(void);

/* 跑一次推理。
 * input:  256x256x3 uint8 (channel-first / NCHW)
 * output: 返回内部 float32 输出 buffer 指针（chfirst [9][1344]，不要 free）
 * 返回 0=成功，<0=错误 */
int  ai_model_run(const uint8_t *input, const float  **output);

void ai_model_deinit(void);

#endif /* AI_MODEL_H */