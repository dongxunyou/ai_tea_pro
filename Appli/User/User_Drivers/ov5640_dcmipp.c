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