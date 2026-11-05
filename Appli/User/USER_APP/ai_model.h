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

/* ============================================================
 * 双模型模式（2026-11 新增）
 * ------------------------------------------------------------
 *   AI_MODE_TEA     : 茶叶四分类（cls 4 类，输入 320×320，权重 @0x70A00000）
 *   AI_MODE_PACKAGE : 茶叶包装检测（detect 5 类，输入 256×256，权重 @0x70D00000）
 * 切换只决定下一次 ai_model_run 用哪个网络实例；两网络分时执行，不会并发。
 * 预处理尺寸必须用 ai_model_input_width()/height() 查询，不要写死 320。
 * ============================================================ */
typedef enum {
    AI_MODE_TEA     = 0,   /* 茶叶分类（默认） */
    AI_MODE_PACKAGE = 1,   /* 茶叶包装检测 */
} ai_mode_t;

/* 输入缓冲最大尺寸（两模式取大者：320×320×3），供 ai_task 分配 s_net_input */
#define AI_INPUT_SIZE_MAX  (320 * 320 * 3)

void      ai_model_set_mode(ai_mode_t mode);
ai_mode_t ai_model_get_mode(void);
uint32_t  ai_model_input_width(void);
uint32_t  ai_model_input_height(void);

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
