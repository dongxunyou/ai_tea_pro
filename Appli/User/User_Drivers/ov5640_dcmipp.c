/**
 * @file ov5640_dcmipp.c

 */
#include "ov5640_dcmipp.h"
#include "dcmipp.h"

extern DCMIPP_HandleTypeDef hdcmipp;

/* ── 双缓冲区，32字节对齐放外部RAM ── */
uint8_t ov5640_dcmipp_buf0[CAM_BUF_SIZE]
    __attribute__((aligned(32))) __attribute__((section(".EXTRAM")));
uint8_t ov5640_dcmipp_buf1[CAM_BUF_SIZE]
    __attribute__((aligned(32))) __attribute__((section(".EXTRAM")));

/*
 * 初始指向 buf1：
 * 因为第一帧硬件会写到 buf0，
 * 所以 buf1 是第一个"安全可读"的缓冲区
 */
uint8_t *cam_read_buf = ov5640_dcmipp_buf1;

/* FIX-3: 帧计数器，DCMIPP 中断回调每帧 +1，供 wait_fresh_frame() 取新帧 */
volatile uint32_t g_cam_frame_count = 0;

/* 新帧到达标志（2026-09-12 由 gui.c 迁移至此）*/
volatile uint8_t g_cam_frame_ready = 0;

/*
 * HAL 帧完成回调（硬件双缓冲）
 *
 * 硬件自动切换写入目标，无需手动调用 SetMemoryAddress：
 *   帧1→buf0, 帧2→buf1, 帧3→buf0 ...
 *
 * ★ 本函数是整条相机链路的源头：切换 cam_read_buf、推进 g_cam_frame_count、
 *   置位 g_cam_frame_ready。原定义在 gui.c（舵机调试 GUI），2026-09-12 随
 *   gui.c 删除迁移到本文件 —— 若丢失，会退回 HAL 的 __weak 空实现，导致
 *   相机不出图、AI 任务等帧超时。
 */
void HAL_DCMIPP_PIPE_FrameEventCallback(DCMIPP_HandleTypeDef *hdcmipp_arg,
                                         uint32_t Pipe)
{
    (void)hdcmipp_arg;
    if (Pipe != DCMIPP_PIPE0) return;

    static uint8_t cur = 0;
    cam_read_buf = (cur == 0) ? ov5640_dcmipp_buf0 : ov5640_dcmipp_buf1;
    cur ^= 1;

    g_cam_frame_count++;   /* FIX-3: 帧计数 */
    g_cam_frame_ready = 1;
}

/* ─────────────────────────────────────── */
void ov5640_dcmipp_init(void)
{
    __HAL_DCMIPP_DISABLE_IT(&hdcmipp,
        DCMIPP_IT_AXI_TRANSFER_ERROR | DCMIPP_IT_PARALLEL_SYNC_ERROR |
        DCMIPP_IT_PIPE0_FRAME | DCMIPP_IT_PIPE0_VSYNC  |
        DCMIPP_IT_PIPE0_LINE  | DCMIPP_IT_PIPE0_LIMIT  |
        DCMIPP_IT_PIPE0_OVR);

    __HAL_DCMIPP_ENABLE_IT(&hdcmipp, DCMIPP_IT_PIPE0_FRAME);
}

void ov5640_dcmipp_start(void)
{
    cam_read_buf = ov5640_dcmipp_buf1;  /* 重置指针 */

    /*
     * 硬件自动交替：
     *   帧1 → buf0，帧2 → buf1，帧3 → buf0 ...
     * IRQ里不需要手动调用 SetMemoryAddress
     */
    HAL_DCMIPP_PIPE_DoubleBufferStart(&hdcmipp, DCMIPP_PIPE0,
                                      (uint32_t)ov5640_dcmipp_buf0,
                                      (uint32_t)ov5640_dcmipp_buf1,
                                      DCMIPP_MODE_CONTINUOUS);
}

void ov5640_dcmipp_stop(void)
{
    HAL_DCMIPP_PIPE_Stop(&hdcmipp, DCMIPP_PIPE0);
}