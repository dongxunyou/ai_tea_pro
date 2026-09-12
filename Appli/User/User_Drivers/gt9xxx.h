#ifndef __GT9XXX_H
#define __GT9XXX_H

#include "main.h"


/*============================================================
 *  引脚定义（与厂商一致：RST=PD10, INT=PB3）
 *============================================================*/
#define GT9XXX_RST_PORT             GPIOD
#define GT9XXX_RST_PIN              GPIO_PIN_10
#define GT9XXX_RST_CLK_ENABLE()     __HAL_RCC_GPIOD_CLK_ENABLE()

#define GT9XXX_INT_PORT             GPIOB
#define GT9XXX_INT_PIN              GPIO_PIN_3
#define GT9XXX_INT_CLK_ENABLE()     __HAL_RCC_GPIOB_CLK_ENABLE()

/*============================================================
 *  I2C 配置
 *  GT911复位时INT=低，I2C地址固定为 0x14（写地址=0x28）
 *============================================================*/
#define GT9XXX_I2C_HANDLE           hi2c4
#define GT9XXX_I2C_ADDR             0x28U   /* 7位地址0x14，左移1位 */
#define GT9XXX_I2C_TIMEOUT          50U

/*============================================================
 *  屏幕尺寸（ATK-MD0700R-800480）
 *============================================================*/
#define GT9XXX_LCD_WIDTH            800
#define GT9XXX_LCD_HEIGHT           480
#define GT9XXX_MAX_TOUCH            5

/*============================================================
 *  寄存器地址
 *============================================================*/
#define GT9XXX_CTRL_REG             0x8040U
#define GT9XXX_PID_REG              0x8140U
#define GT9XXX_GSTID_REG            0x814EU
#define GT9XXX_TP1_REG              0x8150U   /* 每个触摸点间隔8字节 */

/*============================================================
 *  数据结构
 *============================================================*/
typedef struct {
    uint16_t x;
    uint16_t y;
} GT9XXX_Point_t;

typedef struct {
    uint8_t         num;                        /* 当前触摸点数 0~5 */
    GT9XXX_Point_t  pt[GT9XXX_MAX_TOUCH];       /* 各触摸点坐标    */
} GT9XXX_Data_t;

/*============================================================
 *  函数声明
 *============================================================*/
uint8_t gt9xxx_init(void);
uint8_t gt9xxx_scan(GT9XXX_Data_t *data);

#endif /* __GT9XXX_H */