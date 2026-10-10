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
#include "ov5640_dcmipp.h"  /* CAM_W / CAM_H / cam_read_buf / g_cam_frame_ready */
#include "usart.h"          /* huart7 / uart7_rx_pop / uart7_raw_write / uart7_set_baud */
#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>         /* strtoul：波特率命令解析 */
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

/* ── 推理结果暂存（2026-09-12 由 gui.c 迁移至此，定义见 ai_task.h）──
 * ai_task 是唯一写者；tea_ui.c 的 LVGL timer 是唯一读者。 */
T3Result_t       g_t3_result       = { "---", 0.0f, 0, 0, 0, 0, 0 };
volatile uint8_t g_t3_result_ready = 0U;

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

/* ============================================================
 * 取图导出快照缓冲（干茶叶数据集采集用）
 * ------------------------------------------------------------
 * ★ 为什么必须先快照而不是边采边发：
 *   摄像头是硬件双缓冲，每帧完成回调就会切换 cam_read_buf。而导出一帧
 *   480x480 RGB565 = 460800 字节，在 921600 下要传约 5 秒，期间会翻过
 *   上百帧 —— 直接对着 cam_read_buf 发，必然读到半新半旧的撕裂画面。
 *   所以：先把一帧完整拷进快照缓冲，再从容地慢慢发送。
 *
 * ★ 为什么这里不用 ov5640_dcmipp_stop()/start() 代替快照：
 *   停在摄像头会冻结 LVGL 预览 5 秒，而且要重走 DoubleBuffer 状态机；
 *   快照方案完全不碰相机状态机，风险更低。
 *
 * ★ 必须放 .EXTRAM：内部 SRAM 放不下 460KB；外部 hyperRAM 有 32MB。
 *   该段是 NOLOAD，不会撑大固件体积。
 * ============================================================ */
#define EXPORT_SIDE       (((CAM_W) < (CAM_H)) ? (CAM_W) : (CAM_H))   /* 480 */
#define EXPORT_ROW_BYTES  (EXPORT_SIDE * 2U)                          /*  960 */
#define EXPORT_PAYLOAD    (EXPORT_ROW_BYTES * EXPORT_SIDE)            /* 460800 */
static uint8_t s_export_buf[EXPORT_PAYLOAD]
    __attribute__((aligned(32))) __attribute__((section(".EXTRAM")));

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
        /* ★ 探针：这一行是"每次点识别都显示未识别"的头号嫌疑人。*/
        printf("[NPU] PROBE: s_npu_ready=%u → NPU 未初始化成功, 直接填'%s'\r\n",
               (unsigned)s_npu_ready, CLASS_NONE);
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

/* ============================================================
 *                取图导出模式（干茶叶数据集采集）
 * ------------------------------------------------------------
 *   为什么需要它：模型要在 -NPU 上跑，训练数据就必须来自同一颗 OV5640、
 *   同一路 RGB565、同一种取景。用手机拍 → 部署到板端，色彩/锐化/畸变全
 *   不同，精度必崩。所以数据集只能由板端自己出图。
 *
 *   图像帧协议（MCU → PC）
 *     [0..3]    magic = 'T','F','R','M'
 *     [4..5]    u16 width       (LE)
 *     [6..7]    u16 height      (LE)
 *     [8]       u8  bpp         = 16 (RGB565)
 *     [9]       u8  flags       = 0
 *     [10..13]  u32 payload_len (LE)
 *     [...]     payload: height 行 × width*2 字节，行优先 RGB565
 *     [末尾]    u32 crc32(payload) (LE)
 *     最后紧跟一行 "#CAP OK\r\n"
 *
 *   命令协议（PC → MCU，ASCII 行，\r 或 \n 结尾）
 *     SETBAUD=<rate>  切波特率（★ 静默：成功后一个字节都不回）
 *     CAP             抓取并导出一帧
 *     RESTORE         回退基准波特率（★ 静默）
 *     PING            回 "#PONG\r\n"
 *
 *   ★★ 为什么 SETBAUD / RESTORE 必须静默：
 *      切波特率后 TX 立刻变成新速率，而 PC 此刻很可能还停在旧速率上 ——
 *      MCU 任何主动回包都必然被 PC 收成乱码丢掉。约定：切换类命令一律
 *      不回包，链路是否真的通，由 PC 切完之后补一个 PING 来确认。这样
 *      就不存在"MCU 回得早还是 PC 切得快"的竞态。
 *
 *   ★ 导出期间 UART7 进入 raw mode，printf 全静音 —— 一个日志字节混进
 *     图像流就是一整帧报废。
 *   ★ 切到非基准波特率后启动空闲自动回退，杜绝"板子卡在高速率上失联"。
 * ============================================================ */

#ifndef EXPORT_BASE_BAUD
#define EXPORT_BASE_BAUD  115200U   /* 需与 usart.c 的 DEBUG_UART_BAUD 保持一致 */
#endif
/* 高速波特率下空闲多久自动回退基准速率（毫秒） */
#ifndef EXPORT_BAUD_REVERT_MS
#define EXPORT_BAUD_REVERT_MS  8000U
#endif

typedef struct {
    uint8_t  magic[4];
    uint16_t width;
    uint16_t height;
    uint8_t  bpp;
    uint8_t  flags;
    uint32_t payload_len;
} __attribute__((packed)) export_hdr_t;

static uint32_t s_base_baud      = EXPORT_BASE_BAUD;
static uint32_t s_baud_revert_ms = 0U;      /* 0 = 未启用自动回退 */

/* 逐字节 CRC32（IEEE 802.3，poly 0xEDB88320）。
 * 刻意不用 1KB 查表：省 flash；460KB 算完约几十毫秒，远小于串口传输耗时。 */
static uint32_t crc32_update(uint32_t crc, const uint8_t *p, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc & 1U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
        }
    }
    return crc;
}

static void export_reply(const char *s)
{
    (void)uart7_raw_write((const uint8_t *)s, (uint32_t)strlen(s), 200U);
}

/* ══════════════════════════════════════════════════════════════
 * 分块传输（推荐方式；CAP 整帧模式保留作对照/退路）
 * --------------------------------------------------------------
 * 为什么不再"整帧 460KB 一次传"：
 *   · 921600 下连续阻塞 5 秒、115200 下 40 秒，窗口期间完全无响应；
 *   · 只要丢 1 个字节，整帧 460KB 全部作废；
 *   · 一旦失败，根本无从判断卡在哪一步。
 * 分块之后：每块 8KB（921600 下约 89ms），失败只重传那一块；
 *   且 SNAP / BLK 每一步都有明确响应，出问题一眼能定位。
 * ══════════════════════════════════════════════════════════════ */
/* 块大小是实测出来的，不是算出来的：
 *   8192  字节 → 57 块 → 单帧 15.15s   ← 最快
 *   32768 字节 → 15 块 → 单帧 21.79s
 * 原本以为瓶颈是往返次数，放大块反而更慢，说明真正的瓶颈在别处
 * （高波特率下吞吐不升反降，怀疑与 CH340 的 USB 批量传输行为有关）。
 * 所以取实测最优值 8192。 */
#define EXPORT_CHUNK  8192U
#define EXPORT_NBLK   ((EXPORT_PAYLOAD + EXPORT_CHUNK - 1U) / EXPORT_CHUNK)  /* 57 */

typedef struct {
    uint8_t  magic[4];      /* 'T','B','L','K' */
    uint32_t idx;
    uint32_t len;
} __attribute__((packed)) blk_hdr_t;

static int s_snap_valid = 0;

/* 抓一帧进快照缓冲（只抓取，不发送）。 */
static int export_snapshot(void)
{
    const uint32_t side = (uint32_t)EXPORT_SIDE;
    const uint32_t x0   = ((uint32_t)CAM_W - side) / 2U;
    const uint32_t y0   = ((uint32_t)CAM_H - side) / 2U;

    /* ★ 分步诊断：这几行 printf 是定位"板端卡死"的唯一手段，不要删。
     *   抓帧阶段故意【不】开 raw 静音 —— 一旦卡在下面某一步，串口上
     *   最后打印出来的那一行就精确指出了卡在哪里。 */
    uint32_t t0 = HAL_GetTick();

    (void)wait_fresh_frame(200U);
    printf("[EXP] snap: wait=%lums\r\n", (unsigned long)(HAL_GetTick() - t0));

    /* DCMIPP 是 DMA 写外部 RAM，MPU 又是 Write-Through —— CPU 侧可能留着
     * 陈旧 cache 行，务必先 invalidate 再拷，否则会导出上一帧的残留。 */
    uint8_t *src = (uint8_t *)cam_read_buf;
    SCB_InvalidateDCache_by_Addr((uint32_t *)src, (int32_t)CAM_BUF_SIZE);
    printf("[EXP] snap: inv=%lums\r\n", (unsigned long)(HAL_GetTick() - t0));

    for (uint32_t y = 0U; y < side; y++) {
        const uint8_t *s = src + (uint32_t)((y0 + y) * CAM_STRIDE) + (x0 * 2U);
        memcpy(&s_export_buf[y * EXPORT_ROW_BYTES], s, EXPORT_ROW_BYTES);
    }
    printf("[EXP] snap: copy_done=%lums\r\n", (unsigned long)(HAL_GetTick() - t0));

    s_snap_valid = 1;
    return 0;
}

/* 发送第 idx 块：块头(12) + 数据(<=8192) + CRC32(4) */
static void export_send_block(uint32_t idx)
{
    if ((!s_snap_valid) || (idx >= (uint32_t)EXPORT_NBLK)) { return; }

    uint32_t off = idx * EXPORT_CHUNK;
    uint32_t len = EXPORT_CHUNK;
    if ((off + len) > (uint32_t)EXPORT_PAYLOAD) {
        len = (uint32_t)EXPORT_PAYLOAD - off;
    }

    blk_hdr_t bh;
    bh.magic[0] = 'T'; bh.magic[1] = 'B'; bh.magic[2] = 'L'; bh.magic[3] = 'K';
    bh.idx = idx;
    bh.len = len;

    if (uart7_raw_write((const uint8_t *)&bh, (uint32_t)sizeof(bh), 500U) != 0) { return; }
    if (uart7_raw_write(&s_export_buf[off], len, 3000U) != 0) { return; }

    uint32_t crc = crc32_update(0xFFFFFFFFU, &s_export_buf[off], len) ^ 0xFFFFFFFFU;
    (void)uart7_raw_write((const uint8_t *)&crc, 4U, 500U);
}

/* 整帧模式（保留）：一次把 460KB 全发出去 */
static void export_do_capture(void)
{
    const uint32_t side = (uint32_t)EXPORT_SIDE;

    if (export_snapshot() != 0) { export_reply("#CAP ERR snap\r\n"); return; }

    export_hdr_t hdr;
    hdr.magic[0] = 'T'; hdr.magic[1] = 'F'; hdr.magic[2] = 'R'; hdr.magic[3] = 'M';
    hdr.width       = (uint16_t)side;
    hdr.height      = (uint16_t)side;
    hdr.bpp         = 16U;
    hdr.flags       = 0U;
    hdr.payload_len = (uint32_t)EXPORT_PAYLOAD;

    if (uart7_raw_write((const uint8_t *)&hdr, (uint32_t)sizeof(hdr), 200U) != 0) {
        export_reply("#CAP ERR head\r\n"); return;
    }

    uint32_t crc = 0xFFFFFFFFU;
    uint32_t off = 0U;
    for (uint32_t y = 0U; y < side; y++) {
        if (uart7_raw_write(&s_export_buf[off], EXPORT_ROW_BYTES, 200U) != 0) {
            export_reply("#CAP ERR data\r\n"); return;
        }
        crc = crc32_update(crc, &s_export_buf[off], EXPORT_ROW_BYTES);
        off += EXPORT_ROW_BYTES;
        /* 每 16 行让出一次 CPU，避免 LVGL 长时间得不到调度。
         * 代价约 30 次 1ms tick，相对 5 秒传输可忽略。 */
        if ((y & 15U) == 15U) { osDelay(1); }
    }

    uint32_t out = crc ^ 0xFFFFFFFFU;
    if (uart7_raw_write((const uint8_t *)&out, 4U, 200U) != 0) {
        export_reply("#CAP ERR crc\r\n"); return;
    }
    export_reply("#CAP OK\r\n");
}

static char   s_cmd_line[64];
static uint8_t s_cmd_len = 0U;

static void export_handle_line(char *line)
{
    size_t n = strlen(line);
    while ((n > 0U) && ((line[n - 1U] == '\r') || (line[n - 1U] == '\n'))) {
        line[--n] = '\0';
    }
    if (n == 0U) { return; }

    if (strncmp(line, "SETBAUD=", 8U) == 0) {
        uint32_t baud = (uint32_t)strtoul(&line[8], NULL, 10);
        if (baud == 0U) { return; }
        /* ★ 静默切换：成功后不回任何东西，由 PC 补 PING 确认链路 */
        (void)uart7_set_baud(baud);
        /* raw mode 必须在首次 SETBAUD 就开：高速率下有 printf 漏出就是这个下场 */
        uart7_set_raw_mode(1U);
        s_baud_revert_ms = HAL_GetTick() + EXPORT_BAUD_REVERT_MS;

    } else if (strcmp(line, "CAP") == 0) {
        uart7_set_raw_mode(1U);
        s_baud_revert_ms = HAL_GetTick() + EXPORT_BAUD_REVERT_MS;
        export_do_capture();

    } else if (strcmp(line, "SNAP") == 0) {
        /* ★ 抓帧阶段【不】静音 printf：卡在 wait/cache/memcpy 任意一步时，
         * 串口还能看到 [EXP] snap: 诊断行，不至于变成黑盒。
         * 抓完帧、准备吐数据了才静音。 */
        s_baud_revert_ms = HAL_GetTick() + EXPORT_BAUD_REVERT_MS;
        if (export_snapshot() == 0) {
            uart7_set_raw_mode(1U);
            export_reply("#SNAP OK\r\n");
        } else {
            export_reply("#SNAP ERR\r\n");
        }

    } else if (strcmp(line, "DUMP") == 0) {
        /* Stream mode: grab one frame, then blast all blocks back-to-back in a
         * single continuous stream (only 1 request/response round trip).
         * Faster and more stable than the SNAP+BLK chunked handshake.
         * Snapshot diagnostics print BEFORE raw mode is armed; once streaming
         * starts, UART7 is raw so no log byte can corrupt the image. */
        uint32_t t0 = HAL_GetTick();
        s_baud_revert_ms = HAL_GetTick() + EXPORT_BAUD_REVERT_MS;
        if (export_snapshot() != 0) {
            export_reply("#DUMP ERR snap\r\n");
            return;
        }
        uart7_set_raw_mode(1U);
        for (uint32_t i = 0U; i < (uint32_t)EXPORT_NBLK; i++) {
            export_send_block(i);
        }
        char tail[32];
        (void)snprintf(tail, sizeof(tail), "#DUMP OK t=%lu\r\n",
                       (unsigned long)(HAL_GetTick() - t0));
        export_reply(tail);
        s_baud_revert_ms = HAL_GetTick() + EXPORT_BAUD_REVERT_MS;

    } else if (strncmp(line, "BLK=", 4U) == 0) {
        uint32_t idx = (uint32_t)strtoul(&line[4], NULL, 10);
        s_baud_revert_ms = HAL_GetTick() + EXPORT_BAUD_REVERT_MS;
        export_send_block(idx);

    } else if (strcmp(line, "RESTORE") == 0) {
        /* ★ 同样静默：切完后 PC 会回到基准波特率自己 PING */
        if (huart7.Init.BaudRate != s_base_baud) { (void)uart7_set_baud(s_base_baud); }
        s_baud_revert_ms = 0U;
        uart7_set_raw_mode(0U);

    } else if (strcmp(line, "PING") == 0) {
        export_reply("#PONG\r\n");
    }
}

/* 主循环必须周期调用：①喂入串口字节并处理整行 ②波特率空闲自动回退 */
static void export_poll(void)
{
    /* RX 自愈：串口出过 ORE/FE/NE 错误后 DMA 会卡死、IDLE 回调不再触发，
     * 之后就再也收不到任何命令。
     * ★ 但只在【非导出会话】时执行：health_check 内部会走 UART/DMA abort 流程，
     *   高波特率下有卡死风险；而高波特率恰恰最容易出帧错误，等于一错就死。
     *   空闲态（115200）出错概率极低，放在那里自愈即可。 */
    if (!uart7_get_raw_mode()) {
        uart7_rx_health_check();
    }

    uint8_t b;
    while (uart7_rx_pop(&b)) {
        if ((b == '\r') || (b == '\n')) {
            s_cmd_line[s_cmd_len] = '\0';
            if (s_cmd_len > 0U) { export_handle_line(s_cmd_line); }
            s_cmd_len = 0U;
        } else if (s_cmd_len < (sizeof(s_cmd_line) - 1U)) {
            s_cmd_line[s_cmd_len++] = (char)b;
        } else {
            s_cmd_len = 0U;    /* 超长即丢弃，防止一行永远不结束而卡死解析器 */
        }
    }

    if (s_baud_revert_ms != 0U) {
        if ((int32_t)(HAL_GetTick() - s_baud_revert_ms) >= 0) {
            if (huart7.Init.BaudRate != s_base_baud) { (void)uart7_set_baud(s_base_baud); }
            s_baud_revert_ms = 0U;
            uart7_set_raw_mode(0U);
        }
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

    /* 记录上电时的波特率作为"基准速率"（= usart.c 的 DEBUG_UART_BAUD）。
     * 导出会话结束后回到它，避免把 115200 写死两处而失配。 */
    s_base_baud = huart7.Init.BaudRate;

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
        /* 取图导出：非阻塞喂入串口字节并处理整行命令，到期自动回退波特率。
         * 放在推理之前，保证采集会话期间命令响应优先。 */
        export_poll();

        /* 推理请求：这里刻意用超时而不是 osWaitForever ——
         * 永久阻塞会让串口命令完全收不到，导出会话就再也无法结束。
         * ★ 导出会话期间收紧到 1ms：每块命令的响应延迟直接乘以块数进
         *   入总耗时（15 块 × 平均 5ms 就是 75ms，虽不大但白捡）。 */
        uint32_t wait_ms = (s_baud_revert_ms != 0U) ? 1U : 10U;
        uint32_t flags = osEventFlagsWait(s_evt_id, EVT_REQ,
                                          osFlagsWaitAny, wait_ms);
        if ((flags & osFlagsError) == 0U) {
            run_npu_infer();
        }
    }
}