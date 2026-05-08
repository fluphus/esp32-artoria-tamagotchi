// src/display/display_renderer.h
// 显示渲染器 - 负责真实屏幕绘制
// 条件编译支持多种后端: Serial占位 / TFT_eSPI / LovyanGFX / U8g2
// 用户切换屏幕驱动时, 只需修改此文件和 display_config.h

#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#include "display_config.h"
#include "display_model.h"
#include "display_assets.h"

// ============================================================================
//  DisplayRenderer 类 (静态方法)
// ============================================================================

class DisplayRenderer {
public:
    // 初始化屏幕硬件
    static void init();

    // 清屏
    static void clear();

    // --- 各页面绘制方法 ---
    static void drawBoot(const DisplayModel& model);
    static void drawIdle(const DisplayModel& model);
    static void drawStatus(const DisplayModel& model);
    static void drawFeedDraw(const DisplayModel& model);
    static void drawFeedPick(const DisplayModel& model);
    static void drawFeedResult(const DisplayModel& model);
    static void drawSpecialFood(const DisplayModel& model);
    static void drawPoke(const DisplayModel& model);
    static void drawEvolution(const DisplayModel& model);
    static void drawDestroyConfirm(const DisplayModel& model);
    static void drawDayEnd(const DisplayModel& model);
    static void drawWaitTimeSet(const DisplayModel& model);
    static void drawGallery(const DisplayModel& model);
    static void drawToast(const DisplayModel& model);

    // --- 辅助绘制 ---
    // 绘制精灵 (如果资源存在则绘制, 否则画占位矩形)
    static void drawSprite(int16_t x, int16_t y, const SpriteAsset& sprite, const char* label = nullptr);

    // 绘制进度条
    static void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                int16_t value, int16_t maxValue, uint16_t color);

    // 绘制文本 (居中)
    static void drawTextCentered(int16_t y, const char* text);
};

#endif // DISPLAY_RENDERER_H
