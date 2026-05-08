// src/display/display_config.h
// 显示系统配置 - 屏幕参数、后端选择、动画时长、布局常量

#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include <stdint.h>

// ============================================================================
//  显示后端选择 (只启用一个)
// ============================================================================

#define DISPLAY_BACKEND_SERIAL_PLACEHOLDER  0   // Serial 占位输出 (无真实屏幕)
#define DISPLAY_BACKEND_TFT_ESPI            1   // TFT_eSPI 驱动 (SSD1351 128x128)
#define DISPLAY_BACKEND_LOVYANGFX           0   // LovyanGFX 驱动
#define DISPLAY_BACKEND_U8G2                0   // U8g2 驱动

// ============================================================================
//  屏幕物理参数 (SSD1351 1.5" 128x128 65K Color OLED)
// ============================================================================

#define SCREEN_WIDTH            128          // 屏幕宽度 (像素)
#define SCREEN_HEIGHT           128          // 屏幕高度 (像素)
#define SCREEN_ROTATION         0            // 旋转方向 (0-3)
#define SCREEN_BG_COLOR         0x0000       // 背景色 (黑色)

// SPI 引脚 (TFT_eSPI 使用, 需与 User_Setup.h 一致)
#define TFT_CS_PIN              10
#define TFT_DC_PIN              8
#define TFT_RST_PIN             9
#define TFT_MOSI_PIN            11
#define TFT_SCLK_PIN            12
#define TFT_BL_PIN              -1          // SSD1351 OLED 无背光引脚

// RGB565 资源字节序:
// 0 = 数据按本机 uint16_t 顺序 (通常工具直接输出 0xF800 这类)
// 1 = 每个像素高低字节交换后再绘制 (用于某些外部工具导出的字节序)
#define DISPLAY_RGB565_SWAP_BYTES 0

// ============================================================================
//  颜色定义 (RGB565)
// ============================================================================

#define COLOR_BG                0x0000      // 黑色背景
#define COLOR_TEXT              0xFFFF      // 白色文字
#define COLOR_TEXT_DIM          0x7BEF      // 灰色文字
#define COLOR_HP                0x07E0      // 绿色 HP 条
#define COLOR_HP_LOW            0xFBE0      // 黄色 HP 低
#define COLOR_HP_CRIT           0xF800      // 红色 HP 危险
#define COLOR_SR                0x001F      // 蓝色 SR 条
#define COLOR_SR_HIGH           0xF81F      // 紫色 SR 高
#define COLOR_WARN              0xFD20      // 橙色警告
#define COLOR_CURSOR            0xFFE0      // 黄色光标
#define COLOR_SELECTED          0x07FF      // 青色已选
#define COLOR_COMBO             0xF81F      // 品红连携
#define COLOR_EVOLUTION         0xFFE0      // 金色进化
#define COLOR_DESTROY           0xF800      // 红色销毁
#define COLOR_TOAST_BG          0x18E3      // 深灰 Toast 背景
#define COLOR_TOAST_TEXT        0xFFFF      // 白色 Toast 文字
#define COLOR_BAR_BG            0x2104      // 进度条背景 (深灰)
#define COLOR_PLACEHOLDER       0x4208      // 占位矩形颜色
#define COLOR_HIGHLIGHT         0xFFE0      // 高亮边框 (黄色, 图鉴光标等)

// ============================================================================
//  页面布局常量 (128x128 屏幕)
// ============================================================================

// --- Idle 页面 ---
#define IDLE_SPRITE_X           32          // 形态图 X
#define IDLE_SPRITE_Y           8           // 形态图 Y
#define IDLE_SPRITE_SIZE        64          // 形态图占位大小
#define IDLE_HP_BAR_X           4           // HP 条 X
#define IDLE_HP_BAR_Y           80          // HP 条 Y
#define IDLE_HP_BAR_W           120         // HP 条宽度
#define IDLE_HP_BAR_H           8           // HP 条高度
#define IDLE_SR_BAR_X           4           // SR 条 X
#define IDLE_SR_BAR_Y           92          // SR 条 Y
#define IDLE_SR_BAR_W           120         // SR 条宽度
#define IDLE_SR_BAR_H           8           // SR 条高度
#define IDLE_TIME_Y             106         // 时间文字 Y
#define IDLE_DAY_Y              118         // Day 文字 Y
#define IDLE_HINT_Y             118         // 按键提示 Y

// --- Status 页面 ---
#define STATUS_LINE_H           12          // 每行高度
#define STATUS_START_Y          4           // 起始 Y
#define STATUS_LABEL_X          4           // 标签 X
#define STATUS_VALUE_X          64          // 值 X

// --- Feed 页面 ---
#define FEED_CARD_SIZE          28          // 食物卡片大小
#define FEED_CARD_GAP           4           // 卡片间距
#define FEED_CARD_Y             20          // 卡片 Y
#define FEED_LABEL_Y            52          // 食物名称 Y
#define FEED_CURSOR_Y           16          // 光标指示 Y

// --- Special Food 页面 ---
#define SFOOD_ITEM_H            20          // 每项高度
#define SFOOD_START_Y           10          // 起始 Y
#define SFOOD_LABEL_X           16          // 标签 X

// --- Toast ---
#define TOAST_HEIGHT            20          // Toast 高度
#define TOAST_Y                 (SCREEN_HEIGHT - TOAST_HEIGHT)  // 底部
#define TOAST_PADDING           4           // 内边距

// --- Destroy Confirm ---
#define DESTROY_BTN_Y           80          // YES/NO 按钮 Y
#define DESTROY_BTN_W           48          // 按钮宽度
#define DESTROY_BTN_H           24          // 按钮高度

// ============================================================================
//  动画时长配置 (ms)
// ============================================================================

#define ANIM_DURATION_BOOT          1500    // 启动画面 (稍长以显示 boot message)
#define ANIM_DURATION_FEED_DRAW     800     // 抽卡展示
#define ANIM_DURATION_EATING        600     // 吃饭动画
#define ANIM_DURATION_POKE          500     // 戳一戳
#define ANIM_DURATION_EVOLUTION     2000    // 进化光效
#define ANIM_DURATION_COMBO         1000    // 连携特效
#define ANIM_DURATION_MAPO_TOFU     1500    // 麻婆豆腐彩蛋
#define ANIM_DURATION_RHONGOMYNIAD  3000    // 狮子王终态
#define ANIM_DURATION_BLACK_RHONGO  3000    // 黑狮子王终态
#define ANIM_DURATION_NOBU_EVENT    1500    // nobu 彩蛋事件
#define ANIM_DURATION_DESTROY       2000    // 销毁动画
#define ANIM_DURATION_DAY_END       1500    // 日结算过场
#define ANIM_DURATION_SAVE          500     // 存档图标

// ============================================================================
//  页面停留时长 (ms)
// ============================================================================

#define PAGE_DURATION_FEED_RESULT   1500    // 投喂结果至少停留
#define PAGE_DURATION_TOAST         2000    // Toast 提示停留
#define PAGE_DURATION_DAY_END_STEP  1000    // 日结算每步停留

// ============================================================================
//  串口调试输入开关
// ============================================================================

#ifndef ENABLE_SERIAL_INPUT_DEBUG
#define ENABLE_SERIAL_INPUT_DEBUG   1       // 1=启用串口 btn/btnl/btnr 命令
#endif

#ifndef ENABLE_INPUT_DEBUG
#define ENABLE_INPUT_DEBUG          0       // 1=ButtonDriver 打印 raw/stable/event
#endif

// ============================================================================
//  帧率控制
// ============================================================================

#define DISPLAY_FPS_TARGET          30      // 目标帧率
#define DISPLAY_FRAME_MS            (1000 / DISPLAY_FPS_TARGET)

#endif // DISPLAY_CONFIG_H
