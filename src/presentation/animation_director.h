// src/presentation/animation_director.h
// 动画导演 - 将逻辑层事件翻译为动画序列
// 这是"编剧→导演"的桥梁：接收游戏事件，构建分镜表压入队列
// 不直接操作渲染层，只操作 AnimationQueue

#ifndef ANIMATION_DIRECTOR_H
#define ANIMATION_DIRECTOR_H

#include <stdint.h>
#include "../core/game_state.h"
#include "../pet/feeding.h"
#include "../pet/evolution.h"
#include "presentation_types.h"
#include "animation_queue.h"

// ============================================================================
//  AnimationDirector 类 (静态方法)
// ============================================================================

class AnimationDirector {
public:
    // --- 初始化 ---
    static void init();

    // --- Feed 视觉流 ---
    // 由 MenuController::confirmFeed() 完成逻辑计算后调用
    // 构建完整的进食动画序列并压入队列
    static void buildFeedSequence(
        const PetState& pet,
        const uint8_t picked[3],
        const FeedOutcome& outcome
    );

    // --- Combo 特殊食物选择后 ---
    // 用户选完 combo 食物后，追加 combo 进食动画到队列
    static void buildComboEatingSequence(
        const PetState& pet,
        uint8_t specialFoodId,
        const FeedOutcome& outcome
    );

    // --- 麻婆豆腐演出 ---
    // combo 食物触发麻婆豆腐后，追加麻婆动画到队列
    static void buildMapoTofuSequence(
        const PetState& pet,
        uint8_t mapoCount,
        bool curseActivated
    );

    // --- Poke 视觉流 ---
    static void buildPokeSequence(const PetState& pet);

    // --- 进化演出 ---
    static void buildEvolutionSequence(
        const PetState& pet,
        const EvolutionResult& result
    );

    // --- 日结算演出 ---
    static void buildDayEndSequence(const PetState& pet);

    // --- 工具方法 ---
    // 获取进食动画时长 (由资源系统提供，此处给默认值)
    static uint16_t getEatingDuration(Form form, uint8_t foodId);
    static uint16_t getReactionDuration(Form form, ReactionType reaction);

private:
    // 构建单个进食节点
    static AnimNode makeEatingNode(Form form, uint8_t foodId);

    // 构建反应节点
    static AnimNode makeReactionNode(Form form, ReactionType reaction);

    // 构建等待输入节点
    static AnimNode makeWaitInputNode(AnimNodeType uiType);

    // 构建延时节点
    static AnimNode makeDelayNode(uint16_t ms);

    // 构建回调节点
    static AnimNode makeCallbackNode(AnimNodeCallback onStart, AnimNodeCallback onComplete = nullptr);
};

#endif // ANIMATION_DIRECTOR_H
