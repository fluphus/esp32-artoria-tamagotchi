// src/display/display_assets.cpp
// 资源数据定义 - 当前全部为空占位
// 用户放入真实图片资源后, 替换此文件中的 nullptr 为实际数据指针

#include "display_assets.h"

// ============================================================================
//  宠物形态精灵 (占位: 全部为空)
// ============================================================================

const SpriteAsset SPRITE_FORM[FORM_COUNT] = {
    { nullptr, 0, 0 },  // FORM_LILY
    { nullptr, 0, 0 },  // FORM_WHITE_SABER
    { nullptr, 0, 0 },  // FORM_BLACK_SABER
    { nullptr, 0, 0 },  // FORM_WHITE_LANCER
    { nullptr, 0, 0 },  // FORM_BLACK_LANCER
    { nullptr, 0, 0 },  // FORM_WHITE_ARCHER
    { nullptr, 0, 0 },  // FORM_BLACK_RIDER
    { nullptr, 0, 0 },  // FORM_WHITE_RULER
    { nullptr, 0, 0 },  // FORM_WHITE_LANCER_RHONGOMYNIAD
    { nullptr, 0, 0 },  // FORM_BLACK_LANCER_RHONGOMYNIAD
    { nullptr, 0, 0 },  // FORM_NOBU
    { nullptr, 0, 0 },  // FORM_ODA_NOBUNAGA
};

// ============================================================================
//  普通食物精灵 (占位)
// ============================================================================

const SpriteAsset SPRITE_FOOD[FOOD_COUNT] = {
    { nullptr, 0, 0 },  // 0
    { nullptr, 0, 0 },  // 1
    { nullptr, 0, 0 },  // 2
    { nullptr, 0, 0 },  // 3
    { nullptr, 0, 0 },  // 4
    { nullptr, 0, 0 },  // 5
    { nullptr, 0, 0 },  // 6
    { nullptr, 0, 0 },  // 7
};

// ============================================================================
//  特殊食物精灵 (占位)
// ============================================================================

const SpriteAsset SPRITE_SPECIAL_FOOD[SFOOD_COUNT] = {
    { nullptr, 0, 0 },  // 0: Golden Apple
    { nullptr, 0, 0 },  // 1: Holy Grail Mug
    { nullptr, 0, 0 },  // 2: Emiya Cooking
    { nullptr, 0, 0 },  // 3: Jaguar Snack
};

// ============================================================================
//  动画帧集 (占位)
// ============================================================================

const AnimFrameSet ANIM_POKE_FRAMES      = { nullptr, 0, 100 };
const AnimFrameSet ANIM_EVOLUTION_FRAMES  = { nullptr, 0, 150 };
const AnimFrameSet ANIM_DESTROY_FRAMES    = { nullptr, 0, 100 };
const AnimFrameSet ANIM_RHONGO_FRAMES     = { nullptr, 0, 200 };
const AnimFrameSet ANIM_MAPO_FRAMES       = { nullptr, 0, 100 };
const AnimFrameSet ANIM_COMBO_FRAMES      = { nullptr, 0, 100 };

const AnimFrameSet ANIM_NOBU_FRAMES       = { nullptr, 0, 100 };
