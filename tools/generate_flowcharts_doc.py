#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Generate flowcharts (matplotlib PNG) and Word document for tea machine architecture."""

import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch
import numpy as np
from docx import Document
from docx.shared import Cm, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'output')
os.makedirs(OUT_DIR, exist_ok=True)

plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'SimSun', 'KaiTi']
plt.rcParams['axes.unicode_minus'] = False

FONT_MAIN = 'Microsoft YaHei'
FONT_MONO = 'Consolas'

COLOR_BOX    = '#E3F2FD'
COLOR_BOX2   = '#FFF3E0'
COLOR_BOX3   = '#E8F5E9'
COLOR_BOX4   = '#FCE4EC'
COLOR_BOX5   = '#EDE7F6'
COLOR_LINE   = '#37474F'
COLOR_ARROW  = '#1565C0'
COLOR_TEXT   = '#212121'
COLOR_TITLE  = '#0D47A1'


def draw_box(ax, x, y, w, h, text, fc=COLOR_BOX, fs=9, bold=False,
             ec=COLOR_LINE, lw=1.2, alpha=1.0, color=COLOR_TEXT):
    fbox = FancyBboxPatch((x - w/2, y - h/2), w, h,
                          boxstyle="round,pad=0.02", fc=fc, ec=ec, lw=lw, alpha=alpha)
    ax.add_patch(fbox)
    weight = 'bold' if bold else 'normal'
    ax.text(x, y, text, ha='center', va='center', fontsize=fs,
            fontname=FONT_MAIN, color=color, weight=weight)


def draw_arrow(ax, x0, y0, x1, y1, color=COLOR_ARROW, lw=1.6, ls='-'):
    ax.annotate('', xy=(x1, y1), xytext=(x0, y0),
                arrowprops=dict(arrowstyle='->', color=color, lw=lw, ls=ls,
                                connectionstyle='arc3,rad=0'))


def draw_diamond(ax, x, y, r, text, fs=8, fc=COLOR_BOX2, color=COLOR_TEXT):
    diamond = mpatches.RegularPolygon((x, y), 4, radius=r, orientation=np.pi/4,
                                      fc=fc, ec=COLOR_LINE, lw=1.2)
    ax.add_patch(diamond)
    ax.text(x, y, text, ha='center', va='center', fontsize=fs,
            fontname=FONT_MAIN, color=color)


def save_chart(fig, name):
    path = os.path.join(OUT_DIR, f'fig_{name}.png')
    fig.savefig(path, dpi=200, bbox_inches='tight', facecolor='white', edgecolor='none')
    plt.close(fig)
    return path


def draw_architecture():
    fig, ax = plt.subplots(figsize=(8.5, 3.8))
    ax.set_xlim(0, 8.5); ax.set_ylim(0, 3.8); ax.axis('off')
    ax.set_title('图2-10  软件整体架构框图', fontsize=13, fontname=FONT_MAIN,
                 fontweight='bold', color=COLOR_TITLE, pad=12)

    LAYERS = [
        ('应用层', 3.1, COLOR_BOX, [
            '泡茶状态机', 'AI 茶叶识别', '人机交互 UI',
            '温度闭环', '称重闭环', 'WiFi 命令',
        ]),
        ('中间件层', 2.0, COLOR_BOX3, [
            'FreeRTOS', 'LVGL v8', 'STM32 AI/NPU',
            'AT 命令解析', 'Modbus RTU/TCP',
        ]),
        ('驱动层', 0.9, COLOR_BOX2, [
            'DS18B20', 'HX711', 'OV5640',
            'GT9xx', 'RGB LCD', 'RS485/WiFi',
            'PWM 舵机/泵', 'GPIO/SSR',
        ]),
    ]

    for ly, (name, y, color, modules) in enumerate(LAYERS):
        ax.text(0.45, y, name, va='center', fontsize=11, fontname=FONT_MAIN,
                fontweight='bold', color=COLOR_LINE)
        n = len(modules)
        for i, mod in enumerate(modules):
            bx = 1.15 + i * (7.0 / n) + (7.0 / n) / 2
            draw_box(ax, bx, y, (6.8 / n) - 0.06, 0.48, mod, fc=color, fs=8)

    for yl in [2.55, 1.45]:
        ax.axhline(y=yl, xmin=0.05, xmax=0.95, color='#B0BEC5', lw=0.8, ls='--')

    ax.annotate('', xy=(7.9, 3.6), xytext=(7.9, 0.6),
                arrowprops=dict(arrowstyle='<->', color='#78909C', lw=1.4))
    ax.text(8.05, 2.1, 'API', va='center', fontsize=7, fontname=FONT_MAIN, color='#78909C')

    draw_box(ax, 4.25, 0.25, 8.1, 0.40,
             '硬件层: STM32N6 MCU + ATON NPU + OV5640 + DS18B20 + HX711 + SSR + 舵机/泵阵列',
             fc='#ECEFF1', fs=8, ec='#90A4AE')
    return save_chart(fig, 'architecture')


def draw_brew_sm():
    fig, ax = plt.subplots(figsize=(8.5, 4.4))
    ax.set_xlim(0, 8.5); ax.set_ylim(-0.4, 4.2); ax.axis('off')
    ax.set_title('图2-11  泡茶主流程状态机 (BrewState_t)', fontsize=12, fontname=FONT_MAIN,
                 fontweight='bold', color=COLOR_TITLE, pad=10)

    ROW1_Y = 2.9; ROW2_Y = 1.4
    top = [
        ('BREW_IDLE', 0.55, '待机'),
        ('BREW_PICKUP', 2.05, '机械臂\n接茶位'),
        ('BREW_WAIT_CONFIRM', 3.60, '等用户\n确认'),
        ('BREW_DISPENSE', 5.15, '开盖下料\n1500ms'),
        ('BREW_TO_CUP', 6.90, '多步搬运\n4步×1.8s'),
    ]
    bot = [
        ('BREW_FILL', 0.55, '注水到目标\n≥90℃ 泵0'),
        ('BREW_STEEP', 2.05, '浸泡\n30s'),
        ('BREW_POUR', 3.60, '漏勺反转\n180°倒渣'),
        ('BREW_PARK', 5.15, '回home\nch2=140\nch3=240'),
        ('BREW_WAIT_OUTPUT', 6.90, '等用户\n出茶'),
    ]

    for i, (name, x, desc) in enumerate(top):
        draw_box(ax, x, ROW1_Y, 1.30, 0.62, desc, fc=COLOR_BOX, fs=8)
        ax.text(x, ROW1_Y - 0.44, name, ha='center', fontsize=5.6, fontname=FONT_MONO, color='#546E7A')
    for i, (name, x, desc) in enumerate(bot):
        draw_box(ax, x, ROW2_Y, 1.30, 0.62, desc, fc=COLOR_BOX3, fs=8)
        ax.text(x, ROW2_Y - 0.45, name, ha='center', fontsize=5.6, fontname=FONT_MONO, color='#546E7A')

    draw_box(ax, 8.05, ROW2_Y, 0.85, 0.62, '泵1出茶\n≤15g\n或60s', fc='#E8F0FE', fs=7.5)
    ax.text(8.05, ROW2_Y - 0.45, 'BREW_OUTPUT', ha='center', fontsize=5.2, fontname=FONT_MONO, color='#546E7A')

    draw_box(ax, 8.05, 3.75, 0.95, 0.42, 'BREW_DONE', fc='#C8E6C9', fs=8, bold=True)
    draw_box(ax, 8.05, 0.30, 0.95, 0.42, 'BREW_ERROR', fc='#FFCDD2', fs=8, bold=True, ec='#C62828')

    tx = [0.55, 2.05, 3.60, 5.15, 6.90]
    for j in range(4):
        draw_arrow(ax, tx[j] + 0.62, ROW1_Y, tx[j+1] - 0.62, ROW1_Y)
    draw_arrow(ax, 6.90, ROW1_Y - 0.32, 6.90, ROW2_Y + 0.32)
    ax.text(7.05, 2.15, '搬运到位', fontsize=6, fontname=FONT_MAIN, color='#1976D2')

    bx = [0.55, 2.05, 3.60, 5.15, 6.90]
    for j in range(4):
        draw_arrow(ax, bx[j] + 0.62, ROW2_Y, bx[j+1] - 0.62, ROW2_Y)
    draw_arrow(ax, 6.90 + 0.62, ROW2_Y, 8.05 - 0.44, ROW2_Y)
    draw_arrow(ax, 8.05, ROW2_Y + 0.32, 8.05, 3.75 - 0.22)
    ax.text(8.20, 2.6, 'DONE', fontsize=6, fontname=FONT_MAIN, color='#2E7D32')

    ax.text(2.60, 3.35, 'confirm()', ha='center', fontsize=6, fontname=FONT_MONO, color='#1976D2')

    for x in tx:
        ax.annotate('', xy=(8.05, 0.30 + 0.22), xytext=(x, ROW1_Y - 0.32),
                    arrowprops=dict(arrowstyle='->', color='#EF9A9A', lw=0.7, ls='--',
                                    connectionstyle='arc3,rad=0.25'))
    for x in bx:
        ax.annotate('', xy=(8.05, 0.30 + 0.22), xytext=(x, ROW2_Y - 0.32),
                    arrowprops=dict(arrowstyle='->', color='#EF9A9A', lw=0.7, ls='--',
                                    connectionstyle='arc3,rad=0.15'))

    ax.text(0.35, 4.05,
            '实线 = 正常流程；虚线 = 任意步骤超时/异常 → BREW_ERROR；另有录入态 EntryState_t(IDLE→OPENING→WAIT_FILL→CLOSING→DONE)',
            fontsize=6.5, fontname=FONT_MAIN, color='#78909C')
    return save_chart(fig, 'brew_sm')


def draw_temp_control():
    fig, ax = plt.subplots(figsize=(8.5, 4.2))
    ax.set_xlim(0, 8.5); ax.set_ylim(-0.3, 4.0); ax.axis('off')
    ax.set_title('图2-12  温度滞回闭环控制流程图', fontsize=12, fontname=FONT_MAIN,
                 fontweight='bold', color=COLOR_TITLE, pad=10)

    MCX = 4.25
    draw_box(ax, MCX, 3.55, 3.0, 0.42, 'heater_task() 200ms 周期唤醒', fc=COLOR_BOX, fs=9)
    draw_box(ax, MCX, 2.90, 3.0, 0.42, 'read_temp_c(TANK_BOIL) 读 DS18B20 水温', fc=COLOR_BOX, fs=9)
    draw_diamond(ax, MCX, 2.15, 0.48, '温度合法?\n-20~130℃', fs=7.5)
    draw_box(ax, 7.0, 2.15, 1.9, 0.42, '置传感器异常\n本次不控制', fc='#FFCDD2', fs=8)

    draw_diamond(ax, 1.4, 1.35, 0.52, '≥98℃\nTARGET', fs=7.5)
    draw_box(ax, 1.4, 0.55, 1.7, 0.42, 'gpio_set(OFF)\nSSR 断电', fc='#C8E6C9', fs=8)
    draw_box(ax, 3.6, 1.35, 1.6, 0.42, '80~98℃\n保持现状', fc='#ECEFF1', fs=8)
    draw_diamond(ax, 3.6, 0.55, 0.50, '≤80℃\nHYST_ON', fs=7.5)
    draw_box(ax, 5.9, 0.55, 1.7, 0.42, 'gpio_set(ON)\nSSR 通电', fc='#FFF9C4', fs=8)
    draw_diamond(ax, 7.5, 1.35, 0.55, '连续通电\n>15min?', fs=7.5)
    draw_box(ax, 7.5, 0.55, 1.7, 0.42, 's_heater_fault\n=true 锁定', fc='#FFCDD2', fs=8)

    draw_arrow(ax, MCX, 3.34, MCX, 3.11)
    draw_arrow(ax, MCX, 2.69, MCX, 2.55)
    draw_arrow(ax, MCX + 0.48, 2.15, 7.0 - 0.95, 2.15)
    ax.text(5.6, 2.28, '否', fontsize=7, fontname=FONT_MAIN, color='#C62828')
    draw_arrow(ax, MCX, 1.67, 1.4, 1.55)
    ax.text(2.6, 1.75, '是', fontsize=7, fontname=FONT_MAIN, color='#2E7D32')

    draw_arrow(ax, 1.4, 0.90, 1.4, 0.76)
    ax.text(1.55, 1.0, '是', fontsize=7, fontname=FONT_MAIN, color='#C62828')
    draw_arrow(ax, 1.92, 1.35, 3.6 - 0.55, 1.35)
    ax.text(2.7, 1.48, '介于', fontsize=6.5, fontname=FONT_MAIN, color='#546E7A')
    draw_arrow(ax, 3.6, 0.90, 3.6, 0.76)
    draw_arrow(ax, 4.1, 0.55, 5.9 - 0.85, 0.55)
    ax.text(4.9, 0.68, '是', fontsize=7, fontname=FONT_MAIN, color='#2E7D32')
    draw_arrow(ax, 4.16, 1.35, 7.5 - 0.55, 1.35)
    ax.text(5.9, 1.5, '通电中→检故障', fontsize=6.5, fontname=FONT_MAIN, color='#546E7A')
    draw_arrow(ax, 7.5, 0.90, 7.5, 0.76)
    ax.text(7.65, 1.0, '是', fontsize=7, fontname=FONT_MAIN, color='#C62828')

    ax.annotate('', xy=(0.5, 3.55), xytext=(0.5, 0.3),
                arrowprops=dict(arrowstyle='->', color=COLOR_ARROW, lw=1.2,
                                connectionstyle='arc3,rad=0'))
    ax.text(0.35, 1.9, '周期循环\n200ms', fontsize=7, fontname=FONT_MAIN, color=COLOR_ARROW, rotation=90, va='center')
    ax.annotate('', xy=(0.5, 0.3), xytext=(1.4, 0.34),
                arrowprops=dict(arrowstyle='-', color=COLOR_ARROW, lw=1.0))
    ax.annotate('', xy=(0.5, 0.3), xytext=(7.5, 0.34),
                arrowprops=dict(arrowstyle='-', color=COLOR_ARROW, lw=1.0, ls=':'))

    ax.text(0.30, 3.92,
            '关键函数: heater_task() | heater_gpio_set() | read_temp_c() | heater_clear_fault()；即热模组 hw_heater 经 RS485 Modbus 独立控制',
            fontsize=6.5, fontname=FONT_MAIN, color='#78909C')
    return save_chart(fig, 'temp_control')


def draw_weighing():
    fig, ax = plt.subplots(figsize=(8.5, 4.0))
    ax.set_xlim(0, 8.5); ax.set_ylim(-0.2, 4.0); ax.axis('off')
    ax.set_title('图2-13  定量送茶与称重闭环流程图', fontsize=12, fontname=FONT_MAIN,
                 fontweight='bold', color=COLOR_TITLE, pad=10)

    MCX = 3.1
    steps = [
        (3.55, 'BREW_DISPENSE  SV_LID_OUT_OPEN=20° 开输出盖', COLOR_BOX),
        (2.95, '等待 1500ms 下料 → 关盖', COLOR_BOX),
        (2.35, 'BREW_TO_CUP 机械臂多步搬运 (4步×1.8s)', COLOR_BOX3),
        (1.75, 'BREW_FILL: scale_tare(TANK_BREW) 去皮', COLOR_BOX3),
        (1.15, 'read_temp_c(TANK_BOIL)≥90℃ → pump_cmd(PUMP_FILL,ON) 泵0注水', COLOR_BOX3),
        (0.55, '实时 read_weight_g(TANK_BREW) vs s_brew_tgt_g(250g) → 达标停泵', COLOR_BOX3),
    ]
    for y, txt, fc in steps:
        draw_box(ax, MCX, y, 4.5, 0.42, txt, fc=fc, fs=8)
    for i in range(len(steps)-1):
        draw_arrow(ax, MCX, steps[i][0] - 0.22, MCX, steps[i+1][0] + 0.22)

    draw_box(ax, MCX, 0.05, 3.4, 0.28, '超30s 未达标 → BREW_ERROR', fc='#FFCDD2', fs=8)
    ax.annotate('超时', xy=(MCX + 2.3, 0.55), xytext=(MCX + 2.3, 0.15),
                fontsize=7, fontname=FONT_MAIN, color='#C62828',
                arrowprops=dict(arrowstyle='->', color='#EF9A9A', lw=1.0))

    draw_box(ax, 7.0, 3.35, 2.3, 0.42, 'BREW_STEEP 浸泡 30s', COLOR_BOX5, fs=8)
    draw_box(ax, 7.0, 2.75, 2.3, 0.42, 'BREW_POUR 漏勺反转180°', COLOR_BOX5, fs=8)
    draw_box(ax, 7.0, 2.15, 2.3, 0.42, 'BREW_PARK 回home\nch2=140 ch3=240', COLOR_BOX5, fs=8)
    draw_arrow(ax, 7.0, 3.14, 7.0, 2.97)
    draw_arrow(ax, 7.0, 2.54, 7.0, 2.37)
    draw_arrow(ax, MCX + 2.25, 1.75, 7.0 - 1.15, 3.35)

    draw_box(ax, 7.0, 1.45, 2.3, 0.42, 'BREW_WAIT_OUTPUT\n等 tea_brew_output()', '#E8F0FE', fs=8)
    draw_box(ax, 7.0, 0.75, 2.3, 0.48, 'BREW_OUTPUT 泵1出茶\n≤15g 或 60s', '#E8F0FE', fs=8)
    draw_arrow(ax, 7.0, 1.94, 7.0, 1.67)
    draw_arrow(ax, 7.0, 1.24, 7.0, 0.99)

    ax.text(0.30, 3.92,
            '输入: s_brew_tgt_g / SV_ARM_xx 序列   反馈: g_peri.weight_g[TANK_BREW](HX711)   输出: g_servo_setpoint[] / pump_cmd() 队列',
            fontsize=6.3, fontname=FONT_MAIN, color='#78909C')
    ax.text(0.30, -0.12,
            '关键函数: tea_brew_start() servo_set() pump_cmd() scale_tare() read_weight_g()；HX711 由 weight_pump_task 管理(标定/去皮/闭环)',
            fontsize=6.3, fontname=FONT_MAIN, color='#78909C')
    return save_chart(fig, 'weighing')


def draw_ai():
    fig, ax = plt.subplots(figsize=(8.5, 4.0))
    ax.set_xlim(0, 8.5); ax.set_ylim(-0.2, 4.0); ax.axis('off')
    ax.set_title('图2-14  AI NPU 茶叶识别流程图', fontsize=12, fontname=FONT_MAIN,
                 fontweight='bold', color=COLOR_TITLE, pad=10)

    MCX = 3.3
    items = [
        (3.55, 'ai_task_trigger_infer() 发事件 → 唤醒 ai_task', COLOR_BOX),
        (2.95, 'SCB_InvalidateDCache 刷新 cam_read_buf (800×480 RGB565)', COLOR_BOX),
        (2.35, 'preprocess_frame(): 中心裁480×480→缩放256×256→RGB888→NCHW uint8', COLOR_BOX3),
        (1.75, 'ai_model_run(): memcpy 196608B → NPU input buffer', COLOR_BOX3),
        (1.15, 'LL_ATON_RT_Main() NPU 推理 → float32 [9×1344] chfirst', COLOR_BOX4),
        (0.55, 'postprocess(): 遍历1344框×类别增益 → 取 top-1', COLOR_BOX4),
    ]
    for y, txt, fc in items:
        draw_box(ax, MCX, y, 5.2, 0.42, txt, fc=fc, fs=7.6)
    for i in range(len(items)-1):
        draw_arrow(ax, MCX, items[i][0] - 0.22, MCX, items[i+1][0] + 0.22)

    draw_box(ax, MCX, 0.02, 4.0, 0.28, '多帧投票平滑 (VOTE_N=7, VOTE_MIN=3)', fc='#E1BEE7', fs=8)
    draw_arrow(ax, MCX, 0.34, MCX, 0.16)

    draw_box(ax, 7.2, 0.30, 2.2, 0.62,
             'g_t3_result:\nclass_name / confidence / box\n→ g_t3_result_ready=1',
             fc='#C8E6C9', fs=7.6)
    draw_arrow(ax, MCX + 2.05, 0.02, 7.2 - 1.05, 0.30)

    draw_box(ax, 7.2, 2.5, 2.2, 0.5, 'ai_fault_recover()\nAI 超时/异常恢复', fc='#FFCDD2', fs=8)
    ax.annotate('', xy=(7.2, 1.75), xytext=(7.2, 2.24),
                arrowprops=dict(arrowstyle='<-', color='#EF9A9A', lw=1.0, ls='--'))

    ax.text(0.30, 3.92,
            '输入: cam_read_buf(OV5640 800×480 RGB565)   模型: network_atonbuf.xSPI2.raw @0x70700000',
            fontsize=6.3, fontname=FONT_MAIN, color='#78909C')
    ax.text(0.30, -0.12,
            '输出: g_t3_result(绿茶/红茶/乌龙/普洱/花茶 5类之一)   关键函数: ai_task_trigger_infer() preprocess_frame() ai_model_run() postprocess()',
            fontsize=6.3, fontname=FONT_MAIN, color='#78909C')
    return save_chart(fig, 'ai_npu')


def draw_hmi():
    fig, ax = plt.subplots(figsize=(8.5, 4.0))
    ax.set_xlim(0, 8.5); ax.set_ylim(-0.2, 4.0); ax.axis('off')
    ax.set_title('图2-15  人机交互与界面刷新流程图', fontsize=12, fontname=FONT_MAIN,
                 fontweight='bold', color=COLOR_TITLE, pad=10)

    LBB = 2.1
    left = [
        (3.35, 'GT9xx 触摸中断 → LVGL indev 读坐标', COLOR_BOX),
        (2.70, '事件分发: on_mode_auto / on_brew_pick / water_slider_cb', COLOR_BOX),
        (2.05, '更新参数(水量/茶仓/状态) 或切换页面', COLOR_BOX),
    ]
    for y, txt, fc in left:
        draw_box(ax, LBB, y, 3.6, 0.44, txt, fc=fc, fs=7.6)
    for i in range(len(left)-1):
        draw_arrow(ax, LBB, left[i][0] - 0.23, LBB, left[i+1][0] + 0.23)

    RBB = 6.4
    draw_box(ax, RBB, 3.35, 3.4, 0.44, '50ms LVGL Timer → tick_cb()', COLOR_BOX3, fs=8)
    draw_box(ax, RBB, 2.70, 3.4, 0.44, 'tea_machine_tick() 推进\nBrewState_t / EntryState_t', COLOR_BOX3, fs=8)
    draw_box(ax, RBB, 1.95, 3.4, 0.62,
             '按 s_flow 刷新界面:\n泡茶进度条 / 录入提示 / 设置实时值\nGod面板 god_refresh()', COLOR_BOX3, fs=7.6)
    draw_arrow(ax, RBB, 3.13, RBB, 2.92)
    draw_arrow(ax, RBB, 2.48, RBB, 2.26)

    ax.annotate('', xy=(RBB - 1.75, 2.20), xytext=(LBB + 1.85, 2.10),
                arrowprops=dict(arrowstyle='<->', color='#78909C', lw=1.2))
    ax.text(4.25, 2.42, '状态更新 / 界面查询', ha='center', fontsize=7, fontname=FONT_MAIN, color='#78909C')

    draw_box(ax, RBB, 1.20, 3.4, 0.44, 'WiFi 模拟链路命令 (AT/Modbus 解析)', '#F3E5F5', fs=8)
    draw_arrow(ax, RBB, 1.64, RBB, 1.42)

    draw_box(ax, LBB, 1.35, 3.6, 0.44, 'God 模式 g_god_mode=1\n悬浮调试面板刷新', '#FFF9C4', fs=8)

    draw_box(ax, 4.25, 0.55, 8.0, 0.55,
             '4 屏: s_app_scr(主界面+设置 tileview) | s_entry_scr(录入) | s_brew_scr(泡茶) | s_debug_scr(调试)',
             fc='#ECEFF1', fs=8)
    draw_arrow(ax, RBB, 1.20 - 0.24, 4.25 + 2.0, 0.83)
    draw_arrow(ax, LBB, 1.35 - 0.24, 4.25 - 2.0, 0.83)

    ax.text(0.30, 3.92,
            '关键函数: tea_ui_init() tick_cb() tea_machine_tick() god_refresh()；平台: GT9xx + LVGL v8 + 800×480 RGB565 LCD',
            fontsize=6.5, fontname=FONT_MAIN, color='#78909C')
    return save_chart(fig, 'hmi')


def build_doc():
    doc = Document()
    for section in doc.sections:
        section.page_width = Cm(21.0)
        section.page_height = Cm(29.7)
        section.top_margin = Cm(2.54)
        section.bottom_margin = Cm(2.54)
        section.left_margin = Cm(3.18)
        section.right_margin = Cm(3.18)

    style = doc.styles['Normal']
    style.font.name = '微软雅黑'
    style.font.size = Pt(10.5)
    style._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')

    def add_heading(text, level=2):
        h = doc.add_heading(text, level=level)
        for run in h.runs:
            run.font.name = '微软雅黑'
            run._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')
        return h

    def add_para(text, bold=False, fs=10.5, indent=False):
        p = doc.add_paragraph()
        p.paragraph_format.line_spacing = 1.5
        if indent:
            p.paragraph_format.first_line_indent = Cm(0.74)
        run = p.add_run(text)
        run.font.name = '微软雅黑'
        run.font.size = Pt(fs)
        run.bold = bold
        run._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')
        return p

    def add_image(fig_name, caption, width=Cm(15.5)):
        path = os.path.join(OUT_DIR, f'fig_{fig_name}.png')
        if os.path.exists(path):
            doc.add_picture(path, width=width)
            doc.paragraphs[-1].alignment = WD_ALIGN_PARAGRAPH.CENTER
            cp = doc.add_paragraph()
            cp.alignment = WD_ALIGN_PARAGRAPH.CENTER
            cr = cp.add_run(caption)
            cr.font.name = '微软雅黑'
            cr.font.size = Pt(9)
            cr.bold = True
            cr._element.rPr.rFonts.set(qn('w:eastAsia'), '微软雅黑')
        else:
            add_para(f'[图片占位] {caption} (文件未找到)', fs=9)

    add_heading('2.3 软件设计', level=2)
    add_heading('2.3.1 软件整体架构', level=3)
    add_para('本系统软件采用分层架构设计，自上而下划分为应用层（Application Layer）、'
             '中间件层（Middleware Layer）和驱动层（Driver Layer）三大层次，辅以底层硬件抽象。'
             '各层之间通过明确的 API 接口进行调用，上层不直接访问硬件寄存器，保证了模块的独立性与可移植性。', indent=True)
    add_image('architecture', '图2-10  软件整体架构框图（驱动层/中间件层/应用层，标注主要模块）')

    add_heading('2.3.2 软件各模块介绍', level=3)
    add_para('依据总体框图，本节对各核心模块给出详细设计说明，并从顶层到底层给出关键函数的流程图，'
             '标明关键输入/输出变量。每个流程图旁附文字流程描述，变量名和函数名以源码中的实际命名为准。', indent=True)

    add_heading('（1）泡茶主流程状态机', level=3)
    add_para('泡茶主流程是软件的核心逻辑，由有限状态机（FSM）统一调度。状态枚举定义于 BrewState_t'
             '（tea_machine.h），共包含 12 个状态：'
             'BREW_IDLE → BREW_PICKUP → BREW_WAIT_CONFIRM → BREW_DISPENSE → BREW_TO_CUP → '
             'BREW_FILL → BREW_STEEP → BREW_POUR → BREW_PARK → BREW_WAIT_OUTPUT → '
             'BREW_OUTPUT → BREW_DONE / BREW_ERROR。'
             '状态机由 tea_machine_tick() 函数推进，该函数在 LVGL 的 50ms 定时器中周期调用，'
             '依据各步骤的计时到位或用户确认事件进行状态切换。', indent=True)
    add_image('brew_sm', '图2-11  泡茶主流程状态机（BrewState_t 枚举、切换条件、异常分支）')
    add_para('流程描述（对应图2-11）：', bold=True)
    add_para('BREW_IDLE 待机 → 调用 tea_brew_start(bin) 进入 BREW_PICKUP（机械臂移至接茶位）→ '
             'BREW_WAIT_CONFIRM 等待用户调用 tea_brew_confirm() 确认就绪 → '
             'BREW_DISPENSE 开输出盖 SV_LID_OUT_OPEN=20°，停留 STEP_MS_DISPENSE=1500ms 下料后关盖 → '
             'BREW_TO_CUP 机械臂多步搬运（ch3→185/ch2→125 → ch3→145/ch2→145，每步 STEP_MS_ARM=1800ms）→ '
             'BREW_FILL 先等待煮水温度 read_temp_c(TANK_BOIL)≥90℃，然后 pump_cmd(PUMP_FILL, ON) 开启泵0注水，'
             '实时读取 read_weight_g(TANK_BREW) 与 s_brew_tgt_g（默认 250g）比较，达标停泵；若注水超过 30 秒未达标则进入 BREW_ERROR → '
             'BREW_STEEP 浸泡计时 BREW_STEEP_MS=30000ms（30s）→ '
             'BREW_POUR 漏勺反转 SV_SCOOP_DUMP=180° 倒茶渣 → '
             'BREW_PARK 机械臂回 home 位（ch2=140, ch3=240）→ '
             'BREW_WAIT_OUTPUT 等待用户调用 tea_brew_output() → '
             'BREW_OUTPUT 泵1出茶，直到 weight_g≤15g 或超 60 秒 → BREW_DONE 完成。'
             '任意步骤发生超时或异常均进入 BREW_ERROR。', indent=True)
    add_para('此外，系统包含独立的录入状态机 EntryState_t（IDLE → OPENING → WAIT_FILL → CLOSING → DONE），'
             '由 tea_entry_open() 触发。关键函数：tea_machine_tick()、tea_brew_start()、tea_brew_confirm()、'
             'tea_brew_output()、tea_brew_abort()。', indent=True)

    add_heading('（2）温度滞回闭环控制', level=3)
    add_para('温度控制由独立任务 heater_task() 以 200ms 为周期运行。采用滞回 ON/OFF 控制策略而非 PID 算法，'
             '通过 DS18B20 数字温度传感器读取当前水温，与目标温度及滞回阈值进行比较，输出 SSR 继电器通断信号，'
             '避免继电器在阈值附近频繁切换。同时内建故障检测与锁定机制，确保安全运行。', indent=True)
    add_image('temp_control', '图2-12  温度闭环控制流程（DS18B20 采样 → 阈值比较 → SSR 通断 → 循环）')
    add_para('流程描述（对应图2-12，非 PID，采用滞回 ON/OFF 控制）：', bold=True)
    add_para('heater_task 以 200ms 周期执行：① 调用 read_temp_c(TANK_BOIL) 读取 DS18B20 当前水温。'
             '② 温度合理性校验：若测量值超出合理范围（-20℃～130℃），置传感器故障标志，本次不控制。'
             '③ 将当前温度与 HEATER_TARGET_C（98℃）比较：若温度 ≥ 98℃，调用 heater_gpio_set(OFF) 断开 SSR 断电。'
             '④ 若温度 ≤ HEATER_HYST_ON_C（80℃），调用 heater_gpio_set(ON) 接通 SSR 加热。'
             '⑤ 若温度在 80℃～98℃ 滞回区间内，保持 SSR 当前通断状态不变，防止继电器频繁切换。'
             '⑥ 故障检测：若 SSR 连续通电时间超过 HEATER_MAX_ON_MS（15 分钟）仍未达到 98℃ 目标，'
             '则置 s_heater_fault=true 进入故障锁定状态，需调用 heater_clear_fault() 手动恢复。', indent=True)
    add_para('另外，系统包含即热模组 hw_heater（通过 RS485 Modbus 协议的独立热水模块，挂载于 USART3），'
             '支持设定目标水温与水量直接出热水，通过寄存器读写命令控制。'
             '关键函数：heater_task()、heater_gpio_set()、read_temp_c()、HAL_GPIO_WritePin(GPIOQ, PIN3)。', indent=True)

    add_heading('（3）定量送茶与称重闭环', level=3)
    add_para('定量送茶与称重环节涵盖从输出盖下料到泡茶罐注水的完整机械-流体操作流程，'
             '核心是通过 HX711 称重传感器实现注水精确定量控制。整体流程由 tea_machine 的 brew_tick 推进，'
             '各阶段动作由舵机和泵协同完成。', indent=True)
    add_image('weighing', '图2-13  定量送茶流程（输出盖 → 漏勺 → 搬运 → 注水称重 → 浸泡）')
    add_para('流程描述（对应图2-13，由 tea_machine brew_tick 推进）：', bold=True)
    add_para('BREW_DISPENSE 阶段：设置 SV_LID_OUT_OPEN=20° 打开输出盖，停留 STEP_MS_DISPENSE=1500ms 等待落料后关盖。→ '
             'BREW_TO_CUP 阶段：机械臂按预定位姿序列（SV_ARM_xx 宏）多步搬运茶叶到泡茶罐上方。→ '
             'BREW_FILL 阶段：先调用 scale_tare(TANK_BREW) 对泡茶罐称重传感器去皮（归零），'
             '然后调用 read_temp_c(TANK_BOIL) 等待煮水温度 ≥ 90℃（BREW_FILL_TEMP_MIN_C），'
             '满足后调用 pump_cmd(PUMP_FILL, ON) 开启泵0，将热水打入泡茶罐；注水过程中实时调用 read_weight_g(TANK_BREW) '
             '读取当前重量，与 s_brew_tgt_g（默认 250g）比较，重量达标后立即停泵；若连续泵水超过 30s 仍未达标，进入 BREW_ERROR。→ '
             'BREW_STEEP 阶段：浸泡计时 BREW_STEEP_MS=30s。→ BREW_POUR 阶段：漏勺翻转 SV_SCOOP_DUMP=180° 倒出茶渣。→ '
             'BREW_PARK 阶段：机械臂回 home 中立位（ch2=SV_ARM_UD_PARK=140, ch3=SV_ARM_LR_PARK=240）。', indent=True)
    add_para('输入变量：s_brew_tgt_g（目标水量）、SV_ARM_xx 位姿序列。反馈变量：g_peri.weight_g[TANK_BREW]（HX711 称重读值）。'
             '输出变量：g_servo_setpoint[]（舵机角度）、pump_cmd() 命令队列。'
             'HX711 称重由独立任务 weight_pump_task 管理，包含量程标定、去皮校准和自动称重闭环功能。'
             '关键函数：tea_brew_start()、servo_set()、pump_cmd()、scale_tare()、read_weight_g()。', indent=True)

    add_heading('（4）AI 茶叶识别', level=3)
    add_para('AI 茶叶识别利用 STM32N6 内置 ATON NPU（神经网络处理单元）加速推理，'
             '基于 YOLO 类检测模型对摄像头采集的茶叶图像进行实时分类识别。'
             '识别流程从图像采集开始，经预处理、模型推理、后处理和多帧投票平滑，最终输出识别结果。', indent=True)
    add_image('ai_npu', '图2-14  AI NPU 识别流程（ai_task_trigger_infer → 预处理 → ai_model_run → YOLO postprocess → g_t3_result）')
    add_para('流程描述（对应图2-14）：', bold=True)
    add_para('① 触发：调用 ai_task_trigger_infer() 向 ai_task 发送事件，唤醒 AI 推理任务。'
             '② 获取图像：调用 SCB_InvalidateDCache 刷新 cam_read_buf 缓存，读取 OV5640 最新帧（800×480 RGB565）。'
             '③ 预处理 preprocess_frame()：将 800×480 图像中心裁剪为 480×480 → 缩放到 256×256 → RGB565 转 RGB888 → 重排为 NCHW uint8。'
             '④ 推理 ai_model_run(s_net_input, &output)：memcpy 196608 字节（256×256×3）到 NPU 输入 buffer → 调用 LL_ATON_RT_Main() 执行 NPU 硬件推理。'
             '⑤ 输出格式：产生 float32、chfirst 排列的 [9×1344] 张量，每个检测框含 cx, cy, w, h 定位参数及 cls4～cls8 共 5 类置信度。'
             '⑥ 后处理 postprocess()：遍历 1344 个检测框，乘上类别置信度增益因子，找出置信度最高的 top-1。'
             '⑦ 多帧投票平滑：采用滑动窗口投票（VOTE_N=7 帧历史，VOTE_MIN=3 票阈值），过滤偶然误识别。'
             '⑧ 输出结果：填写 g_t3_result（class_name、confidence、box 坐标），并置 g_t3_result_ready=1 通知上层。', indent=True)
    add_para('输入：cam_read_buf（OV5640 800×480 RGB565）。输出：g_t3_result（5 类之一）。'
             '模型权重存储于 network_atonbuf.xSPI2.raw，加载到地址 0x70700000。'
             '关键函数：ai_task_trigger_infer()、preprocess_frame()、ai_model_run()、postprocess()、ai_fault_recover()。', indent=True)

    add_heading('（5）人机交互与界面刷新', level=3)
    add_para('人机交互以 LVGL（轻量级图形库）为基础，通过触摸屏 GT9xx 接收用户输入，'
             '由事件回调驱动页面切换和参数修改，同时 50ms 定时器周期刷新界面状态。'
             '系统共设计了 4 个主要屏幕页面，覆盖泡茶、录入、设置和调试场景。', indent=True)
    add_image('hmi', '图2-15  人机交互/界面刷新流程（LVGL 事件 → tea_ui 页面/参数 → 50ms timer → tea_machine_tick 刷新）')
    add_para('流程描述（对应图2-15）：', bold=True)
    add_para('① 触摸输入：GT9xx 触摸 IC 检测到触摸后产生中断，LVGL 的 indev（输入设备）驱动读取触摸坐标。'
             '② 事件分发：LVGL 将触摸事件路由到当前活动页面注册的回调函数，如 on_mode_auto、on_brew_pick、water_slider_cb 等。'
             '③ 参数更新：回调函数内部修改全局参数（水量/茶仓/状态），并按需调用页面切换函数。'
             '④ 定时刷新：50ms LVGL 定时器触发 tick_cb()，内部调用 tea_machine_tick() 推进 BrewState_t 和 EntryState_t 状态机。'
             '⑤ 界面绘制：根据当前状态变量 s_flow 刷新对应界面——泡茶进度条调用 tea_brew_progress() 获取 0~100 数值，录入操作显示状态提示，设置页面实时显示传感器数值。'
             '⑥ WiFi 命令：同时处理 WiFi 模拟链路中的 AT 命令或 Modbus 解析命令。'
             '⑦ God 模式：当上帝模式开启时刷新调试悬浮面板（god_refresh），叠加显示内部调试信息。', indent=True)
    add_para('4 屏定义：s_app_scr（主界面 + 设置 tileview）、s_entry_scr（录入页面）、'
             's_brew_scr（泡茶进度页面）、s_debug_scr（调试页面）。'
             '关键函数：tea_ui_init()、tick_cb()、tea_machine_tick()、god_refresh()。', indent=True)

    docx_path = os.path.join(OUT_DIR, '02_软件设计_流程图.docx')
    doc.save(docx_path)
    return docx_path


if __name__ == '__main__':
    print('Generating flowcharts...')
    for c in [draw_architecture(), draw_brew_sm(), draw_temp_control(),
              draw_weighing(), draw_ai(), draw_hmi()]:
        print(f'  OK: {c}')
    print('\nBuilding Word document...')
    print(f'  OK: {build_doc()}')
    print('\nDone!')
