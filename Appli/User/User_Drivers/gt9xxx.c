/**
 * @file  sw_i2c_gt9xxx.c
 * @brief 用 PD14/PD4 软件IIC 驱动 GT911
 *        完全替代之前依赖 hi2c4 的 gt9xxx.c
 */

#include "gt9xxx.h"
#include "ctiic.h"
#include "usart.h"
#include <string.h>

/*================================================================
 * 底层：16bit寄存器地址的软件IIC读写
 * GT911 协议：先发高字节地址，再发低字节地址
 *================================================================*/

/**
 * @brief  软件IIC写寄存器
 * @param  dev_write_addr : 设备写地址（7bit左移1位，如0x28）
 * @param  reg            : 16bit寄存器地址
 * @param  buf            : 数据
 * @param  len            : 长度
 * @retval 0=成功, 1=失败
 */
static uint8_t sw_wr_reg(uint8_t dev_write_addr,
                          uint16_t reg,
                          uint8_t *buf,
                          uint16_t len)
{
    uint8_t ret = 0;

    ct_iic_start();
    ct_iic_send_byte(dev_write_addr);       /* 设备地址+写 */
    if (ct_iic_wait_ack()) { ct_iic_stop(); return 1; }

    ct_iic_send_byte((uint8_t)(reg >> 8));  /* 寄存器高字节 */
    if (ct_iic_wait_ack()) { ct_iic_stop(); return 1; }

    ct_iic_send_byte((uint8_t)(reg & 0xFF));/* 寄存器低字节 */
    if (ct_iic_wait_ack()) { ct_iic_stop(); return 1; }

    for (uint16_t i = 0; i < len; i++) {
        ct_iic_send_byte(buf[i]);
        if (ct_iic_wait_ack()) { ret = 1; break; }
    }

    ct_iic_stop();
    return ret;
}

/**
 * @brief  软件IIC读寄存器
 */
static uint8_t sw_rd_reg(uint8_t dev_write_addr,
                          uint16_t reg,
                          uint8_t *buf,
                          uint16_t len)
{
    uint8_t dev_read_addr = dev_write_addr | 0x01U;

    /* 先写寄存器地址 */
    ct_iic_start();
    ct_iic_send_byte(dev_write_addr);
    if (ct_iic_wait_ack()) { ct_iic_stop(); return 1; }

    ct_iic_send_byte((uint8_t)(reg >> 8));
    if (ct_iic_wait_ack()) { ct_iic_stop(); return 1; }

    ct_iic_send_byte((uint8_t)(reg & 0xFF));
    if (ct_iic_wait_ack()) { ct_iic_stop(); return 1; }

    /* 重新START，切换为读 */
    ct_iic_start();
    ct_iic_send_byte(dev_read_addr);
    if (ct_iic_wait_ack()) { ct_iic_stop(); return 1; }

    for (uint16_t i = 0; i < len; i++) {
        /* 最后一个字节发NACK */
        buf[i] = ct_iic_read_byte(i < (len - 1) ? 1 : 0);
    }

    ct_iic_stop();
    return 0;
}

/*================================================================
 * 对外接口：与 gt9xxx.h 声明保持完全一致
 * （gt9xxx.h 里的 gt9xxx_wr_reg / gt9xxx_rd_reg 是static的，
 *   所以这里直接实现 gt9xxx_init 和 gt9xxx_scan 即可）
 *================================================================*/

/* 当前使用的设备地址（自动探测后确定） */
static uint8_t s_dev_addr = 0x28U;   /* 默认 0x14<<1 */

/*----------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------*/
uint8_t gt9xxx_init(void)
{
    uint8_t pid[5] = {0};
    uint8_t tmp    = 0;
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* RST / INT GPIO 初始化 */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* RST = PD10，推挽输出 */
    GPIO_InitStruct.Pin   = GT9XXX_RST_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GT9XXX_RST_PORT, &GPIO_InitStruct);

    /* INT = PB3，先设置为输出（复位期间控制地址选择） */
    GPIO_InitStruct.Pin   = GT9XXX_INT_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GT9XXX_INT_PORT, &GPIO_InitStruct);

    /* 软件IIC引脚初始化 */
    ct_iic_init();

    /*
     * 复位时序（选择I2C地址 0x14 → 写地址 0x28）
     *   INT低电平 → RST拉低10ms → RST拉高 → INT切输入
     */
    HAL_GPIO_WritePin(GT9XXX_INT_PORT, GT9XXX_INT_PIN, GPIO_PIN_RESET); /* INT=0 */
    HAL_GPIO_WritePin(GT9XXX_RST_PORT, GT9XXX_RST_PIN, GPIO_PIN_RESET); /* RST=0 */
    HAL_Delay(10);
    HAL_GPIO_WritePin(GT9XXX_RST_PORT, GT9XXX_RST_PIN, GPIO_PIN_SET);   /* RST=1 */
    HAL_Delay(10);

    /* INT 切换为浮空输入 */
    GPIO_InitStruct.Pin  = GT9XXX_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GT9XXX_INT_PORT, &GPIO_InitStruct);

    HAL_Delay(100);  /* 等GT911启动 */

    /* 先尝试地址 0x28，失败则尝试 0xBA */
    s_dev_addr = 0x28U;
    if (sw_rd_reg(s_dev_addr, GT9XXX_PID_REG, pid, 4) != 0)
    {
        print_info_debug("[GT9XXX] 0x28读取失败，尝试0xBA...\r\n");
        s_dev_addr = 0xBAU;
        if (sw_rd_reg(s_dev_addr, GT9XXX_PID_REG, pid, 4) != 0)
        {
            print_info_debug("[GT9XXX] 0xBA也失败！检查PD14/PD4接线\r\n");
            return 1;
        }
    }

    print_info_debug("[GT9XXX] addr=0x%02X  PID=%02X %02X %02X %02X => \"%s\"\r\n",
                     s_dev_addr, pid[0], pid[1], pid[2], pid[3], (char *)pid);

    if ((strcmp((char *)pid, "911")  != 0) &&
        (strcmp((char *)pid, "9147") != 0) &&
        (strcmp((char *)pid, "1158") != 0))
    {
        print_info_debug("[GT9XXX] PID不匹配，实际=\"%s\"\r\n", (char *)pid);
        return 2;
    }

    /* 软复位 → 正常工作 */
    tmp = 0x02U;
    sw_wr_reg(s_dev_addr, GT9XXX_CTRL_REG, &tmp, 1);
    HAL_Delay(10);
    tmp = 0x00U;
    sw_wr_reg(s_dev_addr, GT9XXX_CTRL_REG, &tmp, 1);

    print_info_debug("[GT9XXX] 初始化成功! IC=%s  addr=0x%02X\r\n",
                     (char *)pid, s_dev_addr);
    return 0;
}

/*----------------------------------------------------------------
 * 扫描触摸数据
 *----------------------------------------------------------------*/
uint8_t gt9xxx_scan(GT9XXX_Data_t *data)
{
    uint8_t status = 0;
    uint8_t clear  = 0;
    uint8_t buf[4];

    data->num = 0;

    if (sw_rd_reg(s_dev_addr, GT9XXX_GSTID_REG, &status, 1) != 0)
        return 0;

    if (!(status & 0x80U))
        return 0;   /* 数据未就绪 */

    /* ! 必须清除bit7，GT911才会更新下一帧 */
    sw_wr_reg(s_dev_addr, GT9XXX_GSTID_REG, &clear, 1);

    uint8_t touch_num = status & 0x0FU;
    if (touch_num == 0 || touch_num > GT9XXX_MAX_TOUCH)
        return 0;

    data->num = touch_num;

    for (uint8_t i = 0; i < touch_num; i++)
    {
        uint16_t tp_reg = (uint16_t)(GT9XXX_TP1_REG + (uint16_t)i * 8U);

        if (sw_rd_reg(s_dev_addr, tp_reg, buf, 4) != 0)
            continue;

        data->pt[i].x = ((uint16_t)buf[1] << 8U) | buf[0];
        data->pt[i].y = ((uint16_t)buf[3] << 8U) | buf[2];
    }

    return touch_num;
}