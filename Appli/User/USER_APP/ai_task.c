/* ============================================================
 * ai_task.c — 茶叶包装检测（NPU / Neural-ART 推理版）
 * ------------------------------------------------------------
 *   流程：
 *     1) 摄像头当前帧 (cam_read_buf, 800x480 RGB565)
 *     2) 中心裁剪 480x480 → 缩放 256x256 → RGB888 → NCHW uint8
 *     3) 调 ai_model_run() 跑 NPU 推理（权重在 octoFlash 0x70A00000）
 *     4) YOLO 输出 float32 chfirst [9][1344] (cx,cy,w,h,cls4~8)，取最高分类框
 *     5) 填同一个 g_t3_result（class_name + confidence），GUI 照常显示
 *
 *   对接保持不变：g_t3_result / g_t3_result_ready / ai_task_trigger_infer()
 *
 *   本版改动：仅 run_npu_infer() 内——给推理调用也加上 fault 保护 +
 *             分步打印，使"点识别崩溃/卡死"时能回收到 GUI 并留下串口线索。
 * ============================================================ */

#include "ai_task.h"
#include "ai_model.h"        /* NPU 推理封装：ai_model_init / ai_model_run */
#include "main.h"            /* CMSIS/HAL：SCB_*、类型 */
#include "cmsis_os2.h"
#include "gui.h"             /* T3Result_t / g_t3_result / g_t3_result_ready */
#include "ov5640_dcmipp.h"  /* CAM_W / CAM_H / cam_read_buf */
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>          /* BusFault 捕获用 */

/* ------------------------------------------------------------
 * 这两个计数器原是给一个手写 CDNN0_IRQHandler 用的；现在 NPU 完成中断的
 * ISR 由 ll_aton 运行时自带（runtime.c 里的 ATON_STD_IRQHandler，经宏即
 * NPU0_IRQHandler / NPU_END_OF_EPOCH_IRQHandler），stm32n6xx_it.c 里并无
 * 自定义 NPU handler，故这两个变量现已无人自增。保留定义以免别处 extern
 * 引用导致链接报错；不再依赖它们。
 * ------------------------------------------------------------ */
volatile uint32_t g_cdnn0_irq_count   = 0U;
volatile uint32_t g_cdnn0_irq_entered = 0U;
volatile uint8_t  g_aton_irq_test_mode = 0U;

/* ============================================================
 *                      可调参数
 * ============================================================ */
#define CAM_STRIDE        (CAM_W * 2U)   /* RGB565 每行字节数 = 800*2 = 1600 */

/* 模型输入尺寸（来自 ai_model.h：256x256x3） */
#define NET_W   AI_INPUT_WIDTH     /* 256 */
#define NET_H   AI_INPUT_HEIGHT    /* 256 */
#define NET_C   AI_INPUT_CHANNELS  /* 3   */

/* 通道顺序开关：0 = RGB（默认，先这样试）；若红蓝判反 / 结果乱，改成 1 = BGR */
#define INPUT_BGR   0

/* 任务#31：R/B 平面对调实验开关。=1 时 plane0 写 B、plane2 写 R（G 不动） */
#define PREPROC_RB_SWAP  0   /* task#32: 改回原色序，先用输入/输出诊断定位 */

/* 摄像头若输出 BGR565（个别模组），把这里改 1。与 INPUT_BGR 独立： */
/* SWAP_RB 修正"从像素里取出的 R/B"，INPUT_BGR 修正"喂给网络的通道序" */
#define SWAP_RB     0

/* 检测判定阈值：最高框 score（反量化后）≥ 此值 → 判定检测到包装。
 * ★ 这个值现在是占位，烧完权重后看串口打印的实际 score 再调 ★ */
#define DET_SCORE_THRESH   0.35f   /* #61: 降到0.35，让英红/菊花过阈值线 */

/* #63 招A+B: 类别增益 + 分类别阈值
 *  增益：cls0黑茶=1.0  cls1绿茶=1.0  cls2菊花=1.25  cls3英红=1.20  cls4青柑=1.0
 *  阈值：稳定类0.50，边界类菊花0.30/英红0.32                            */
static const float CLS_GAIN[5]   = { 1.00f, 1.00f, 1.25f, 1.20f, 1.00f };
static const float CLS_THRESH[5] = { 0.50f, 0.50f, 0.30f, 0.32f, 0.50f };

/* FIX-2: 每次按键突发投票参数（可调） */
#define BURST_N    5     /* 每次按键跑几帧 */
#define BURST_MIN  3     /* 突发内某类至少多少票才认定 */
extern volatile uint32_t g_cam_frame_count;  /* FIX-3: DCMIPP 回调维护 */

/* 5 类检测标签 */
static const char * const g_class_names[AI_NUM_CLASSES] = {
    "black_tea",               /* ch4 */
    "green_tea",               /* ch5 */
    "chrysanthemum_tea",       /* ch6 */
    "yinghong_black_tea",      /* ch7 */
    "green_tangerine_puer_tea" /* ch8 */
};
#define CLASS_NONE       "---"

/* NPU 初始化看门狗超时（毫秒）。RuntimeInit 正常应在 1 秒内返回，
 * 给宽裕值；超过即判定它 hang。 */
#define NPU_INIT_TIMEOUT_MS   3000U

/* ============================================================ */
TaskHandle_t ai_task_handle = NULL;
static osEventFlagsId_t s_evt_id = NULL;
#define EVT_REQ (1UL << 0)

/* NPU 输入缓冲：256*256*3 = 196608 字节。放静态区，别放栈上。 */
static uint8_t s_net_input[NET_W * NET_H * NET_C];

/* 标记 NPU 是否初始化成功 */
static uint8_t s_npu_ready = 0U;

/* FIX-3: 等待一帧新的完整帧到来（带超时降级） */
static const uint8_t *wait_fresh_frame(uint32_t timeout_ms)
{
    uint32_t start = g_cam_frame_count;
    uint32_t t0    = HAL_GetTick();
    while (g_cam_frame_count == start) {
        if ((HAL_GetTick() - t0) >= timeout_ms) break;
        osDelay(2);
    }
    return (const uint8_t *)cam_read_buf;
}

/* ============================================================
 * 故障/卡死保护
 * ------------------------------------------------------------
 * 1) BusFault/HardFault 捕获（setjmp/longjmp）—— 抓"访问越界"类崩溃。
 * 2) 看门狗超时 —— 抓 init 阶段"死等中断"类 hang（hang 没有异常）。
 * 需要在 stm32n6xx_it.c 的各 fault handler 里调用 ai_fault_recover()。
 * ============================================================ */
static jmp_buf           s_fault_jmp;
static volatile int      s_fault_armed   = 0;   /* 是否处于受保护区间 */
static volatile uint32_t s_init_deadline = 0;   /* init 超时绝对时刻(tick)；0=未武装 */

void ai_fault_recover(void)
{
    if (s_fault_armed) {
        s_fault_armed = 0;
        longjmp(s_fault_jmp, 1);   /* 跳回最近一次 setjmp 处（init 或 推理） */
    }
    /* 没武装保护时发生硬错误：真正的 bug，停下来便于调试 */
    while (1) { }
}

/* 看门狗监控任务：周期检查 init 是否超时未完成。
 * 注意：它只覆盖 init 阶段（s_init_deadline 仅在 init 期间被武装）。
 *       推理阶段不靠它（跨任务 longjmp 不安全），靠 run_npu_infer 内部的
 *       同栈 setjmp + fault handler。*/
static void ai_init_watchdog(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t dl = s_init_deadline;
        if (dl != 0U) {
            if (osKernelGetTickCount() > dl) {
                s_init_deadline = 0U;
                if (s_fault_armed) {
                    s_fault_armed = 0;
                    /* 同一地址空间、裸机 RTOS，init 期间 ai_task 阻塞在
                     * RuntimeInit 内，直接 longjmp 回其 jmp_buf 可行。*/
                    longjmp(s_fault_jmp, 2);
                }
            }
        }
        osDelay(50);
    }
}

/* ------------------------------------------------------------
 * 预处理：cam_read_buf (800x480 RGB565) → s_net_input (256x256x3 NCHW uint8)
 * ------------------------------------------------------------ */
static void preprocess_frame(const uint8_t *frame)
{
    const uint32_t crop = (CAM_H < CAM_W) ? CAM_H : CAM_W;   /* 480 */
    const uint32_t x0   = (CAM_W - crop) / 2U;               /* 160 */
    const uint32_t y0   = (CAM_H - crop) / 2U;               /* 0   */

    uint8_t *plane0 = &s_net_input[0];
    uint8_t *plane1 = &s_net_input[NET_W * NET_H];
    uint8_t *plane2 = &s_net_input[NET_W * NET_H * 2];

    for (uint32_t dy = 0U; dy < NET_H; dy++) {
        uint32_t sy = y0 + (dy * crop) / NET_H;
        const uint8_t *srow = frame + (uint32_t)sy * CAM_STRIDE;

        for (uint32_t dx = 0U; dx < NET_W; dx++) {
            uint32_t sx = x0 + (dx * crop) / NET_W;
            const uint8_t *p = srow + (uint32_t)sx * 2U;
            uint16_t px = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));

            uint8_t c_hi  = (uint8_t)((px >> 11) & 0x1FU);   /* 5 bit = R */
            uint8_t c_mid = (uint8_t)((px >>  5) & 0x3FU);   /* 6 bit = G */
            uint8_t c_lo  = (uint8_t)( px        & 0x1FU);   /* 5 bit = B */
            uint8_t R8 = (uint8_t)((c_hi  << 3) | (c_hi  >> 2));
            uint8_t G8 = (uint8_t)((c_mid << 2) | (c_mid >> 4));
            uint8_t B8 = (uint8_t)((c_lo  << 3) | (c_lo  >> 2));

#if SWAP_RB
            { uint8_t t = R8; R8 = B8; B8 = t; }
#endif

            uint32_t idx = dy * NET_W + dx;

#if PREPROC_RB_SWAP
            plane0[idx] = B8;
            plane1[idx] = G8;
            plane2[idx] = R8;
#elif INPUT_BGR
            plane0[idx] = B8;
            plane1[idx] = G8;
            plane2[idx] = R8;
#else
            plane0[idx] = R8;
            plane1[idx] = G8;
            plane2[idx] = B8;
#endif
        }
    }
}

/* ------------------------------------------------------------
 * 后处理：YOLO 输出 float32 chfirst [9,1344]，找最高分框+类
 * chfirst 布局：ch0=cx  ch1=cy  ch2=w  ch3=h  ch4~8=5 类 confidence
 * ------------------------------------------------------------ */
static int postprocess(const float *out, float *best_score_out,
                       uint32_t *best_idx_out, uint32_t *best_cls_out)
{
    const uint32_t N = AI_NUM_DETECTIONS;

    float    best_score = -1.0f;
    uint32_t best_idx   = 0U;
    uint32_t best_cls   = 0U;
    uint32_t n_active   = 0U;

    for (uint32_t i = 0U; i < N; i++) {
        float cls_score  = -1.0f;
        uint32_t cls_id  = 0U;
        for (uint32_t c = 0U; c < AI_NUM_CLASSES; c++) {
            float v = out[(4U + c) * N + i] * CLS_GAIN[c];   /* #63 乘增益 */
            if (v > cls_score) { cls_score = v; cls_id = c; }
        }
        if (cls_score > 0.10f) n_active++;
        if (cls_score > best_score) {
            best_score = cls_score; best_idx = i; best_cls = cls_id;
        }
    }

    float cx = out[0U*N + best_idx], cy = out[1U*N + best_idx];
    float w  = out[2U*N + best_idx],  h = out[3U*N + best_idx];
    printf("[DET] top=%s score_g=%.4f raw=%.4f thr=%.2f n_active=%lu box(%.1f,%.1f,%.1f,%.1f)\r\n",
           g_class_names[best_cls], (double)best_score,
           (double)(best_score / CLS_GAIN[best_cls]),
           (double)CLS_THRESH[best_cls], (unsigned long)n_active,
           (double)cx, (double)cy, (double)w, (double)h);

    if (best_score_out) *best_score_out = best_score;
    if (best_idx_out)   *best_idx_out   = best_idx;
    if (best_cls_out)   *best_cls_out   = best_cls;

    return (best_score > CLS_THRESH[best_cls]) ? 1 : 0;
}

/* 小工具：填"未检测到"结果 */
static void fill_none_result(float conf)
{
    strncpy(g_t3_result.class_name, CLASS_NONE, sizeof(g_t3_result.class_name) - 1U);
    g_t3_result.class_name[sizeof(g_t3_result.class_name) - 1U] = '\0';
    g_t3_result.confidence = conf;
    g_t3_result.box_valid = 0;
    g_t3_result_ready = 1U;
}

/* FIX-2: 单次按键突发投票 —— 独立完成 K 帧推理+多数表决，不继承历史 */
static void run_npu_infer(void)
{
    if (!s_npu_ready) {
        printf("[NPU] not ready, skip\r\n");
        fill_none_result(0.0f);
        return;
    }

    int      votes[AI_NUM_CLASSES] = {0};
    float    win_score = 0.0f;
    float    win_box[4] = {0, 0, 0, 0};   /* cx,cy,w,h in model 256 space */

    for (int k = 0; k < BURST_N; k++) {
        const uint8_t *frame = wait_fresh_frame(60);

        /* ---- fault 保护（FIX-2 保留原有 setjmp）---- */
        int jc = setjmp(s_fault_jmp);
        if (jc != 0) {
            s_fault_armed = 0;
            printf("[NPU] *** FAULT during inference burst[%d] (jc=%d) CFSR=%08lX HFSR=%08lX -> skip frame ***\r\n",
                   k, jc, (unsigned long)SCB->CFSR, (unsigned long)SCB->HFSR);
            continue;
        }
        s_fault_armed = 1;

        preprocess_frame(frame);

        const float *output = NULL;
        int ret = ai_model_run(s_net_input, &output);
        s_fault_armed = 0;

        if (ret != 0 || output == NULL) {
            printf("[NPU] ai_model_run failed ret=%d burst[%d]\r\n", ret, k);
            continue;
        }

        float best_score; uint32_t best_idx, best_cls;
        int passed = postprocess(output, &best_score, &best_idx, &best_cls);
        if (passed) {
            if (best_cls < AI_NUM_CLASSES) votes[best_cls]++;
            if (best_score > win_score) {
                const uint32_t N = AI_NUM_DETECTIONS;  /* 1344 */
                win_score = best_score;
                win_box[0] = output[0*N + best_idx];
                win_box[1] = output[1*N + best_idx];
                win_box[2] = output[2*N + best_idx];
                win_box[3] = output[3*N + best_idx];
            }
        }
    }

    /* 突发内多数表决 */
    int top = -1, topv = 0;
    for (int c = 0; c < AI_NUM_CLASSES; c++)
        if (votes[c] > topv) { topv = votes[c]; top = c; }

    if (top >= 0 && topv >= BURST_MIN) {
        const float S  = 480.0f / 256.0f;
        const int   OX = 160, OY = 0;
        int L = (int)((win_box[0] - win_box[2] * 0.5f) * S) + OX;
        int T = (int)((win_box[1] - win_box[3] * 0.5f) * S) + OY;
        int BW = (int)(win_box[2] * S), BH = (int)(win_box[3] * S);
        if (L < 0) { BW += L; L = 0; }
        if (T < 0) { BH += T; T = 0; }
        if (L + BW > 800) BW = 800 - L;
        if (T + BH > 480) BH = 480 - T;
        if (BW < 1) BW = 1;
        if (BH < 1) BH = 1;

        strncpy(g_t3_result.class_name, g_class_names[top], sizeof(g_t3_result.class_name) - 1U);
        g_t3_result.class_name[sizeof(g_t3_result.class_name) - 1U] = '\0';
        g_t3_result.confidence = win_score;
        g_t3_result.box_x = L; g_t3_result.box_y = T;
        g_t3_result.box_w = BW; g_t3_result.box_h = BH;
        g_t3_result.box_valid = 1;
        g_t3_result_ready = 1U;

        printf("[NPU] -> %s  conf=%.4f  burst_votes=%d/%d\r\n",
               g_class_names[top], (double)win_score, topv, BURST_N);
    } else {
        fill_none_result(win_score);
        printf("[NPU] -> %s  conf=%.4f  burst_votes=%d/%d (insufficient)\r\n",
               CLASS_NONE, (double)win_score, topv, BURST_N);
    }
}

/* ------------------------------------------------------------
 * 外部触发（线程安全，可在任意上下文调用）——接口保持不变
 * ------------------------------------------------------------ */
void ai_task_trigger_infer(void)
{
    if (s_evt_id != NULL) {
        osEventFlagsSet(s_evt_id, EVT_REQ);
    }
}

/* ------------------------------------------------------------
 *                      FreeRTOS 任务入口
 * ------------------------------------------------------------ */
void ai_task(void *arg)
{
    (void)arg;
    ai_task_handle = xTaskGetCurrentTaskHandle();

    s_evt_id = osEventFlagsNew(NULL);
    configASSERT(s_evt_id != NULL);

    /* 启动看门狗监控任务（高优先级，便于在 ai_task 卡住时仍能运行） */
    static const osThreadAttr_t wd_attr = {
        .name = "aiInitWdg",
        .priority = osPriorityAboveNormal,
        .stack_size = 512 * 4
    };
    osThreadNew(ai_init_watchdog, NULL, &wd_attr);

    /* 初始化 NPU 运行时（整个程序一次）
     * 双保险：setjmp 抓 fault；s_init_deadline 看门狗抓 init hang。*/
    int jmp_code = setjmp(s_fault_jmp);
    if (jmp_code == 0) {
        s_fault_armed   = 1;
        s_init_deadline = osKernelGetTickCount() + NPU_INIT_TIMEOUT_MS;

        int ret = ai_model_init();

        s_init_deadline = 0U;
        s_fault_armed   = 0;
        if (ret == 0) {
            s_npu_ready = 1U;
            printf("[NPU] runtime init OK, detector ready  build=" __DATE__ " " __TIME__ "\r\n");
        } else {
            s_npu_ready = 0U;
            printf("[NPU] ai_model_init FAILED ret=%d\r\n", ret);
        }
    } else {
        s_init_deadline = 0U;
        s_fault_armed   = 0;
        s_npu_ready     = 0U;
        if (jmp_code == 2) {
            printf("[NPU] *** RuntimeInit TIMEOUT (hang) -> NPU disabled, GUI keeps running ***\r\n");
        } else {
            printf("[NPU] *** FAULT during init -> NPU disabled, GUI keeps running ***\r\n");
            printf("[NPU] (check CFSR/HFSR printed above)\r\n");
        }
    }

    for (;;) {
        osEventFlagsWait(s_evt_id, EVT_REQ, osFlagsWaitAny, osWaitForever);
        run_npu_infer();
    }
}