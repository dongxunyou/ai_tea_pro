/* yolo_postprocess.c — superseded by ai_task.c postprocess() with 5-class float32 output */
#include "yolo_postprocess.h"
#include <string.h>

void yolo_postprocess(const int8_t *raw_output, DetectionResult *result)
{
    (void)raw_output;
    memset(result, 0, sizeof(*result));
}

void yolo_scale_coords(DetectionResult *result, int display_w, int display_h)
{
    (void)result; (void)display_w; (void)display_h;
}
