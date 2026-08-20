/* ============================================================
 * ai_model.h — NPU 模型推理封装（ll_aton / ST Edge AI）
 *
 * 【换模型操作步骤】（重新训练后按此流程替换）
 *  1. PC 侧：训练导出 ONNX(QDQ 量化) → 用 ST Edge AI (stai / neural-art)
 *     重新生成，得到两个产物：
 *       - network.c (+ network_ecblobs.h)：模型描述，替换
 *         ExtMemLoader/X-CUBE-AI/App/ 下同名文件（Appli 工程直接引用它）
 *       - network_atonbuf.xSPI2.raw：权重二进制，替换
 *         Middlewares/ST/AI/weights/ 下同名文件
 *  2. 烧录：经 ExtMemLoader 把新 .raw 烧到 XSPI2 Flash 0x70A00000
 *     （若新模型内存池基址变化，同步修改 ai_model.c 的 WEIGHT_PROBE_ADDR
 *       及 RISAF 授权地址段）
 *  3. 按新模型 I/O 形状修改本文件下方宏：
 *       AI_INPUT_WIDTH/HEIGHT/CHANNELS、AI_NUM_DETECTIONS、
 *       AI_NUM_CLASSES、AI_OUT_SIZE_BYTES
 *  4. 同步修改 ai_task.c：
 *       - g_class_names[] 类名表（及 gui.c 的中文映射表）
 *       - CLS_GAIN[] / CLS_THRESH[] 分类增益与阈值
 *       - 若网络结构变化（非 anchor-free YOLO），重写 postprocess()
 *  5. 输入/输出 buffer 当前固定在 AXISRAM5 0x342E0000，若生成报告
 *     给出新地址，检查 ai_model.c 的 probe 自检地址段。
 * ============================================================ */
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