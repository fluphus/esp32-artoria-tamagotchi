// src/display/display_config.h
// 显示系统配置 - 屏幕参数、后端选择、动画时长

#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include <stdint.h>

// ============================================================================
//  显示后端选择 (只启用一个)
// ============================================================================

#define DISPLAY_BACKEND_SERIAL_PLACEHOLDER  1   // Serial 占位输出 (无真实屏幕)
#define DISPLAY_BACKEND_TFT_ESPI            0   // TFT_eSPI 驱动
#define DISPLAY_BACKEND_LOVYANGFX           0   // LovyanGFX 驱动
#define DISPLAY_BACKEND_U8G2                0   // U8g2 驱动

// ============================================================================
//  屏幕物理参数
// ============================================================================

#define SCREEN_WIDTH            240          // 屏幕宽度 (像素)
#define SCREEN_HEIGHT           240          // 屏幕高度 (像素)
#define SCREEN_ROTATION         0            // 旋转方向 (0-3)
#define SCREEN_BG_COLOR         0x0000       // 背景色 (黑色)

// SPI 引脚 (TFT_eSPI / LovyanGFX 使用)
#define TFT_CS_PIN              10
#define TFT_DC_PIN              8
#define TFT_RST_PIN             9
#define TFT_MOSI_PIN            11
#define TFT_SCLK_PIN            12
#define TFT_BL_PIN              7           // 背光引脚 (-1 = 无背光控制)

// ============================================================================
//  动画时长配置 (ms)
// ============================================================================

#define ANIM_DURATION_BOOT          1000    // 启动画面
#define ANIM_DURATION_FEED_DRAW     800     // 抽卡展示
#define ANIM_DURATION_EATING        600     // 吃饭动画
#define ANIM_DURATION_POKE          500     // 戳一戳
#define ANIM_DURATION_EVOLUTION     2000    // 进化光效
#define ANIM_DURATION_COMBO         1000    // 连携特效
#define ANIM_DURATION_MAPO_TOFU     1500    // 麻婆豆腐彩蛋
#define ANIM_DURATION_RHONGOMYNIAD  3000    // 狮子王终态
#define ANIM_DURATION_BLACK_RHONGO  3000    // 黑狮子王终态
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
