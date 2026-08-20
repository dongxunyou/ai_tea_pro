/* ============================================================
 * ui_theme.h — 全局「茶韵」主题色板（宣纸底 + 松绿/朱砂/琥珀）
 * 与 Web 控制页（tea_preview.html）同一套配色。
 * 高饱和版：针对 RGB 屏泛白特性整体加深提纯。
 * 各 GUI 文件 include 本头，不要再各自硬编码颜色。
 * ============================================================ */
#ifndef UI_THEME_H
#define UI_THEME_H

#define UI_C_BG     0xEBE2C8   /* 背景：宣纸（压深一档，补偿屏泛白偏冷）*/
#define UI_C_PANEL  0xFBF4DE   /* 面板：暖白 */
#define UI_C_PANEL2 0xE2D7B6   /* 次级 / 按钮底：深宣纸 */
#define UI_C_ACCENT 0x2E5535   /* 松绿（主题色）*/
#define UI_C_OK     0x5C955A   /* 茶汤绿（正常/成功）*/
#define UI_C_WARN   0xBF4030   /* 朱砂（警示/停止）*/
#define UI_C_AMBER  0xCE8A1E   /* 琥珀（出茶/数值/CTA）*/
#define UI_C_TXT    0x292520   /* 正文：墨色 */
#define UI_C_DIM    0x7E7663   /* 次要文字：淡墨 */
#define UI_C_LINE   0xCFC2A0   /* 细线 */
#define UI_C_BTNTXT 0xF4F0E3   /* 实色按钮上的米白字 */
#define UI_C_MIST   0xD2E0C8   /* 松绿雾（选中底/徽标底）*/
#define UI_C_DARK   0x16211A   /* 深色数据卡（AI 核心卡）*/

#endif /* UI_THEME_H */
