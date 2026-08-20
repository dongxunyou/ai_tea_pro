#ifndef __OV5640_DCMIPP_H
#define __OV5640_DCMIPP_H

#include "main.h"

/* ── 摄像头参数 ── */
#define CAM_W        800
#define CAM_H        480
/*
 * CAM_BPP = 2：DCMIPP PIPE0 无 pixel packer（P0PPCR 寄存器无 FORMAT 位），
 * 并行接口输入 DCMIPP_FORMAT_RGB565，直通输出 RGB565 = 2 字节/像素。
 * 行步长 = CAM_W * CAM_BPP = 1600。
 * 参考：HAL_Pipe_Config 对 PIPE0 仅写 P0FCTCR.FRATE，PixelPackerFormat/Pitch 被忽略。
 */
#define CAM_BPP      2
#define CAM_BUF_SIZE (CAM_W * CAM_H * CAM_BPP)  /* = 800*480*2 = 768000 */

/* ── 双缓冲区（外部RAM）── */
extern uint8_t ov5640_dcmipp_buf0[];
extern uint8_t ov5640_dcmipp_buf1[];

/* ── 当前可安全读取的缓冲区指针 ── */
extern uint8_t *cam_read_buf;

/* FIX-3: 帧计数器（DCMIPP 中断回调每帧 +1） */
extern volatile uint32_t g_cam_frame_count;

/* ── 函数声明 ── */
void ov5640_dcmipp_init(void);
void ov5640_dcmipp_start(void);
void ov5640_dcmipp_stop(void);

#endif