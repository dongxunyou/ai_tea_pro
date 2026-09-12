#ifndef YOLO_POSTPROCESS_H
#define YOLO_POSTPROCESS_H

#include <stdint.h>

#define YOLO_CONF_THRESHOLD   0.25f
#define YOLO_NMS_THRESHOLD    0.45f
#define YOLO_MAX_DETECTIONS   20

typedef struct {
    float x1, y1, x2, y2;
    float score;
    int   class_id;
} Detection;

typedef struct {
    Detection dets[YOLO_MAX_DETECTIONS];
    int count;
} DetectionResult;

/* 注意：raw_output 是 int8_t*（NPU 原始输出），函数内部反量化 */
void yolo_postprocess(const int8_t *raw_output, DetectionResult *result);
void yolo_scale_coords(DetectionResult *result, int display_w, int display_h);

#endif /* YOLO_POSTPROCESS_H */