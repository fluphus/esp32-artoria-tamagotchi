// src/display/display_assets.h
// 资源引用声明 - 精灵图、动画帧
// 实际资源数据由用户放入后编译, 此处仅声明接口

#ifndef DISPLAY_ASSETS_H
#define DISPLAY_ASSETS_H

#include <stdint.h>
#include "../core/game_state.h"
#include "../config/food_table.h"

// ============================================================================
//  精灵资源结构
// ============================================================================

struct SpriteAsset {
    const uint16_t* data;       // RGB565 像素数据指针 (nullptr = 资源不存在)
    uint16_t width;
    uint16_t height;
};

// ============================================================================
//  动画帧序列
// ============================================================================

struct AnimFrameSet {
    const SpriteAsset* frames;  // 帧数组指针
    uint8_t frameCount;
    uint16_t frameDelayMs;      // 每帧间隔 (ms)
};

// ============================================================================
//  资源声明 (extern)
//  实际定义在 display_assets.cpp 中 (用户放入图片后生成)
//  如果资源不存在, renderer 画占位矩形和文字
// ============================================================================

// 宠物形态精灵 (每个形态一张静态图)
extern const SpriteAsset SPRITE_FORM[FORM_COUNT];

// 普通食物精灵
extern const SpriteAsset SPRITE_FOOD[FOOD_COUNT];

// 特殊食物精灵
extern const SpriteAsset SPRITE_SPECIAL_FOOD[SFOOD_COUNT];

// 动画帧集
extern const AnimFrameSet ANIM_POKE_FRAMES;
extern const AnimFrameSet ANIM_EVOLUTION_FRAMES;
extern const AnimFrameSet ANIM_DESTROY_FRAMES;
extern const AnimFrameSet ANIM_RHONGO_FRAMES;
extern const AnimFrameSet ANIM_MAPO_FRAMES;
extern const AnimFrameSet ANIM_COMBO_FRAMES;
extern const AnimFrameSet ANIM_NOBU_FRAMES;

// ============================================================================
//  占位资源 (编译时始终存在)
// ============================================================================

// 检查资源是否可用
inline bool hasSprite(const SpriteAsset& sprite) {
    return sprite.data != nullptr && sprite.width > 0 && sprite.height > 0;
}

inline bool hasAnimFrames(const AnimFrameSet& anim) {
    return anim.frames != nullptr && anim.frameCount > 0;
}

#endif // DISPLAY_ASSETS_H
