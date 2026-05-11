// src/presentation/presentation_types.h
// 演出系统类型定义 - 动画节点、反应类型、资源 ID
// 此文件不包含业务逻辑判定，只定义数据结构

#ifndef PRESENTATION_TYPES_H
#define PRESENTATION_TYPES_H

#include <stdint.h>
#include "../core/game_state.h"
#include "../config/food_table.h"
#include "../pet/feeding.h"
#include "../pet/evolution.h"

// ============================================================================
//  反应类型 (逻辑层输出，渲染层根据 Form 映射到具体动画资源)
// ============================================================================

enum ReactionType : uint8_t {
    REACTION_NONE = 0,
    REACTION_LIKE,          // 单食物喜欢 (白线吃 healthy / 黑线吃 junk)
    REACTION_DISLIKE,       // 单食物讨厌 (白线吃 junk / 黑线吃 healthy)
    REACTION_UMU,           // 整轮总结: 阵营匹配的投喂 (非 combo)
    REACTION_EWW,           // 整轮总结: 阵营不匹配的投喂 (非 combo)
    REACTION_SATISFY,       // combo 触发且阵营匹配 (黑线 all junk / 白线 all healthy)
    REACTION_ABHOR,         // combo 触发且阵营不匹配 (黑线 all healthy / 白线 all junk)
    REACTION_PERFECT,       // 每日投喂条件达标
    REACTION_COUNT
};

// ============================================================================
//  动画节点类型
// ============================================================================

enum AnimNodeType : uint8_t {
    NODE_NONE = 0,

    // --- 宠物动作 ---
    NODE_PET_EATING,        // 宠物进食动画 (形态决定动作，食物 ID 决定食物层)
    NODE_PET_REACTION,      // 宠物反应表情 (ReactionType × Form → 具体资源)
    NODE_PET_POKE,          // 戳一戳反应

    // --- 叠加/独立演出 ---
    NODE_OVERLAY_SPRITE,    // 在指定坐标绘制静态/动画精灵 (食物图标、特效等)

    // --- Combo 特殊食物演出 ---
    // 模式 A: 在屏幕某坐标绘制给予者角色 + 宠物接受动画
    NODE_COMBO_GIVER,
    // 模式 B: 替换为完整交互动画 (如梅林递金苹果的全屏演出)
    // NODE_COMBO_FULLSCREEN,   // [预留] 未来实现时取消注释
    // 模式 C: 分屏演出 (左右各一个角色)
    // NODE_COMBO_SPLIT,        // [预留] 未来实现时取消注释

    // --- 麻婆豆腐 ---
    NODE_MAPO_TOFU,         // 麻婆豆腐专属演出 (含给予者 + 宠物进食)

    // --- 进化/终态 ---
    NODE_EVOLUTION,         // 进化光效
    NODE_RHONGOMYNIAD,      // 狮子王终态演出
    NODE_BLACK_RHONGO,      // 黑狮子王终态演出
    NODE_NOBU_EVENT,        // Nobu 彩蛋事件

    // --- UI 弹出 ---
    NODE_UI_FEED_CARDS,     // 食物卡片选择 UI (队列暂停等待输入)
    NODE_UI_COMBO_SELECT,   // Combo 特殊食物选择 UI (队列暂停等待输入)

    // --- 流程控制 ---
    NODE_WAIT_INPUT,        // 暂停队列，等待外部 resume
    NODE_CALLBACK,          // 纯回调节点 (在队列中插入逻辑触发点)
    NODE_DELAY,             // 纯延时节点 (节点间的间隔)

    NODE_TYPE_COUNT
};

// ============================================================================
//  动画节点数据
// ============================================================================

// 回调函数类型
typedef void (*AnimNodeCallback)();

struct AnimNode {
    AnimNodeType type;

    // 时长 (ms)。0 = 由资源帧数×帧延迟自动计算
    uint16_t durationMs;

    // --- 宠物进食参数 ---
    Form form;                  // 当前宠物形态 (决定动作精灵集)
    uint8_t foodId;             // 食物 ID (决定食物层精灵)

    // --- 反应参数 ---
    ReactionType reaction;      // 反应类型

    // --- 叠加精灵参数 ---
    uint8_t overlayAssetId;     // 叠加精灵资源 ID
    int16_t x, y;               // 绘制坐标

    // --- Combo 给予者参数 ---
    uint8_t specialFoodId;      // 特殊食物 ID (决定给予者角色和动画)

    // --- 回调 ---
    AnimNodeCallback onStart;   // 节点开始时触发 (可为 nullptr)
    AnimNodeCallback onComplete;// 节点结束时触发 (可为 nullptr)

    // --- 进化参数 ---
    EvolutionResult evoResult;

    // 默认构造
    AnimNode() : type(NODE_NONE), durationMs(0), form(FORM_LILY), foodId(0),
                 reaction(REACTION_NONE), overlayAssetId(0), x(0), y(0),
                 specialFoodId(0), onStart(nullptr), onComplete(nullptr), evoResult{} {}
};

// ============================================================================
//  队列容量 (ESP32 内存约束下的合理上限)
//  最长序列: 3×进食 + 3×反应 + 1×总结 + 1×perfect + 1×combo_ui + 1×combo_eat
//           + 1×mapo + 1×evolution = 12 节点
//  预留 16 个足够覆盖所有场景
// ============================================================================

constexpr uint8_t ANIM_QUEUE_CAPACITY = 16;

// ============================================================================
//  待机动画配置
// ============================================================================

struct IdleAnimConfig {
    uint32_t minIdleBeforeRandomMs;     // 最少待机多久才可能触发随机动画
    uint16_t randomCheckIntervalMs;     // 检查间隔
    uint8_t randomChancePercent;        // 每次检查的触发概率 (0-100)
};

// 默认配置
constexpr IdleAnimConfig DEFAULT_IDLE_CONFIG = {
    30000,  // 30 秒后才可能触发随机待机
    5000,   // 每 5 秒检查一次
    15      // 15% 概率
};

#endif // PRESENTATION_TYPES_H
