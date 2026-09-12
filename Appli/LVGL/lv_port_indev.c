#include "lv_port_indev.h"
#include "gt9xxx.h"
#include "main.h"
#include "usart.h"
static lv_indev_t    *g_tp_indev   = NULL;
static GT9XXX_Data_t  s_touch_data = {0};   /* 共享触摸数据，全局只扫描一次 */

/* ----------------------------------------------------------------
 * 对外接口：demo 通过此函数读取多点触摸，不重复扫描 I2C
 * ---------------------------------------------------------------- */
void lv_port_indev_get_touch(GT9XXX_Data_t *data)
{
    if (data != NULL)
    {
        *data = s_touch_data;
    }
}

/* LVGL indev 回调，每个 LVGL tick 调用一次 */
static void touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    /* 扫描一次，结果存入共享变量 */
    gt9xxx_scan(&s_touch_data);

    if (s_touch_data.num > 0)
    {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = (int32_t)s_touch_data.pt[0].x;
        data->point.y = (int32_t)s_touch_data.pt[0].y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lv_port_indev_init(void)
{
    uint8_t ret = gt9xxx_init();

    /* 打印结果，不再沉默失败 */
    print_info_debug("[INDEV] gt9xxx_init() = %d (%s)\r\n",
                     (int)ret,
                     ret == 0 ? "OK" :
                     ret == 1 ? "I2C通信失败" : "芯片ID不匹配");

    if (ret != 0)
    {
        print_info_debug("[INDEV] 触摸未注册！等待你根据扫描结果处理\r\n");
        return;
    }

    g_tp_indev = lv_indev_create();
    lv_indev_set_type(g_tp_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_tp_indev, touchpad_read_cb);
    print_info_debug("[INDEV] LVGL触摸indev注册成功 ✓\r\n");
}