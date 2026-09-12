#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "gt9xxx.h"

void lv_port_indev_init(void);
void lv_port_indev_get_touch(GT9XXX_Data_t *data);  /* ← 新增：对外暴露多点触摸数据 */

#ifdef __cplusplus
}
#endif

#endif