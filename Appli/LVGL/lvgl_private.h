/**
 * @file lvgl_private.h
 * Private header for LVGL internal structures
 */

#ifndef LVGL_PRIVATE_H
#define LVGL_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

/* Include all private headers needed by demos */
#include "src/display/lv_display_private.h"
#include "src/core/lv_obj_private.h"
#include "src/draw/lv_draw_private.h"

/* Sysmon private headers */
#if LV_USE_SYSMON
    #include "src/debugging/sysmon/lv_sysmon_private.h"
#endif

/* Observer private headers */
#if LV_USE_OBSERVER
    #include "src/core/lv_observer_private.h"
#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LVGL_PRIVATE_H */