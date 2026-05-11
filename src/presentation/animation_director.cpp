// src/presentation/animation_director.cpp
// 动画导演实现 - 事件→动画序列构建

#include "animation_director.h"
#include "pet_state_query.h"
#include "../config/food_table.h"
#include "../config/game_config.h"

// ============================================================================
//  默认动画时长 (ms) - 资源就绪后由 asset_loader 提供精确值
// ============================================================================

static constexpr uint16_t DEFAULT_EATING_DURATION = 800;
static constexpr uint16_t DEFAULT_REACTION_DURATION = 500;
static constexpr uint16_t DEFAULT_COMBO_GIVER_DURATION = 1000;
static constexpr uint16_t DEFAULT_MAPO_DURATION = 1500;
static constexpr uint16_t DEFAULT_POKE_DURATION = 500;
static constexpr uint16_t DEFAULT_EVOLUTION_DURATION = 2000;
static constexpr uint16_t DEFAULT_PERFECT_DURATION = 1000;
static constexpr uint16_t DEFAULT_INTER_NODE_DELAY = 100;

// ============================================================================
//  初始化
// ============================================================================

void AnimationDirector::init() {
    // 未来可在此加载动画时长配置表
}

// ============================================================================
//  Feed 视觉流构建
// ============================================================================

void AnimationDirector::buildFeedSequence(
    const PetState& pet,
    const uint8_t picked[3],
    const FeedOutcome& outcome
) {
    animQueue.clear();

    // === Phase 1: 逐个进食 + 单食物反应 ===
    uint8_t healthyCount = 0;
    for (uint8_t i = 0; i < FEED_PICK_COUNT; i++) {
        uint8_t foodId = picked[i];
        bool foodIsHealthy = FOOD_TABLE[foodId].is_healthy;
        if (foodIsHealthy) healthyCount++;

        // 进食动画
        animQueue.enqueue(makeEatingNode(pet.form, foodId));

        // 单食物反应
        ReactionType perFood = PetStateQuery::getPerFoodReaction(pet, foodIsHealthy);
        animQueue.enqueue(makeReactionNode(pet.form, perFood));
    }

    // === Phase 2: 整轮总结反应 ===
    if (outcome.combo_triggered) {
        // combo → satisfy 或 abhor (替代 umu/eww)
        ReactionType comboReaction = PetStateQuery::getComboReaction(pet, outcome.combo);
        animQueue.enqueue(makeReactionNode(pet.form, comboReaction));
    } else {
        // 非 combo → umu 或 eww
        ReactionType summary = PetStateQuery::getRoundSummaryReaction(pet, healthyCount);
        animQueue.enqueue(makeReactionNode(pet.form, summary));
    }

    // === Phase 3: Perfect 动画 (条件触发) ===
    if (PetStateQuery::shouldPlayPerfect(pet)) {
        AnimNode perfectNode;
        perfectNode.type = NODE_PET_REACTION;
        perfectNode.form = pet.form;
        perfectNode.reaction = REACTION_PERFECT;
        perfectNode.durationMs = DEFAULT_PERFECT_DURATION;
        animQueue.enqueue(perfectNode);
    }

    // === Phase 4: Combo 选择 UI (条件触发) ===
    if (outcome.combo_triggered) {
        // 插入等待输入节点 → 弹出 combo 选择 UI
        animQueue.enqueue(makeWaitInputNode(NODE_UI_COMBO_SELECT));
        // combo 进食动画在用户选择后由 buildComboEatingSequence() 追加
    }

    // 注意: 如果没有 combo，队列播完后自动触发 onQueueComplete → 恢复待机
    // 如果有 combo，队列会在 NODE_UI_COMBO_SELECT 处暂停等待用户输入
}

// ============================================================================
//  Combo 特殊食物选择后
// ============================================================================

void AnimationDirector::buildComboEatingSequence(
    const PetState& pet,
    uint8_t specialFoodId,
    const FeedOutcome& outcome
) {
    // Combo 进食动画: 给予者角色 + 宠物接受
    AnimNode comboNode;
    comboNode.type = NODE_COMBO_GIVER;
    comboNode.form = pet.form;
    comboNode.specialFoodId = specialFoodId;
    comboNode.durationMs = DEFAULT_COMBO_GIVER_DURATION;
    // 坐标由渲染层根据 specialFoodId 查表决定
    comboNode.x = 0;
    comboNode.y = 0;
    animQueue.enqueue(comboNode);

    // 如果触发了麻婆豆腐，追加麻婆演出
    if (outcome.mapo_tofu_triggered) {
        buildMapoTofuSequence(pet, outcome.mapo_tofu_total, outcome.mapo_tofu_curse_activated);
    }
}

// ============================================================================
//  麻婆豆腐演出
// ============================================================================

void AnimationDirector::buildMapoTofuSequence(
    const PetState& pet,
    uint8_t mapoCount,
    bool curseActivated
) {
    AnimNode mapoNode;
    mapoNode.type = NODE_MAPO_TOFU;
    mapoNode.form = pet.form;
    mapoNode.durationMs = DEFAULT_MAPO_DURATION;
    // 未来扩展: 给予者坐标、特殊图片等由渲染层根据 type 决定
    animQueue.enqueue(mapoNode);

    // 如果诅咒激活 (黑狮子王进化)，进化动画由逻辑层单独触发
    // 通过 pending evolution 机制处理，不在此处入队
}

// ============================================================================
//  Poke 视觉流
// ============================================================================

void AnimationDirector::buildPokeSequence(const PetState& pet) {
    animQueue.clear();

    AnimNode pokeNode;
    pokeNode.type = NODE_PET_POKE;
    pokeNode.form = pet.form;
    pokeNode.durationMs = DEFAULT_POKE_DURATION;
    animQueue.enqueue(pokeNode);
}

// ============================================================================
//  进化演出
// ============================================================================

void AnimationDirector::buildEvolutionSequence(
    const PetState& pet,
    const EvolutionResult& result
) {
    AnimNode evoNode;
    evoNode.form = pet.form;
    evoNode.evoResult = result;
    evoNode.durationMs = DEFAULT_EVOLUTION_DURATION;

    switch (result.event) {
        case EVO_RHONGOMYNIAD:
            evoNode.type = NODE_RHONGOMYNIAD;
            break;
        case EVO_BLACK_RHONGOMYNIAD:
            evoNode.type = NODE_BLACK_RHONGO;
            break;
        default:
            evoNode.type = NODE_EVOLUTION;
            break;
    }

    // 如果队列正在播放 (例如 feed 序列中触发进化)，追加到队尾
    // 如果队列为空，直接入队开始播放
    animQueue.enqueue(evoNode);
}

// ============================================================================
//  日结算演出
// ============================================================================

void AnimationDirector::buildDayEndSequence(const PetState& pet) {
    animQueue.clear();

    AnimNode dayEndNode;
    dayEndNode.type = NODE_EVOLUTION;  // 复用进化动画容器
    dayEndNode.form = pet.form;
    dayEndNode.durationMs = 2000;
    animQueue.enqueue(dayEndNode);
}

// ============================================================================
//  时长查询
// ============================================================================

uint16_t AnimationDirector::getEatingDuration(Form form, uint8_t foodId) {
    // 未来: 从 asset_loader 查询实际帧数 × 帧延迟
    // 当前返回默认值
    (void)form;
    (void)foodId;
    return DEFAULT_EATING_DURATION;
}

uint16_t AnimationDirector::getReactionDuration(Form form, ReactionType reaction) {
    (void)form;
    (void)reaction;
    return DEFAULT_REACTION_DURATION;
}

// ============================================================================
//  节点工厂方法
// ============================================================================

AnimNode AnimationDirector::makeEatingNode(Form form, uint8_t foodId) {
    AnimNode node;
    node.type = NODE_PET_EATING;
    node.form = form;
    node.foodId = foodId;
    node.durationMs = getEatingDuration(form, foodId);
    return node;
}

AnimNode AnimationDirector::makeReactionNode(Form form, ReactionType reaction) {
    AnimNode node;
    node.type = NODE_PET_REACTION;
    node.form = form;
    node.reaction = reaction;
    node.durationMs = getReactionDuration(form, reaction);
    return node;
}

AnimNode AnimationDirector::makeWaitInputNode(AnimNodeType uiType) {
    AnimNode node;
    node.type = uiType;
    node.durationMs = 0;  // 无固定时长，等待外部 resume
    return node;
}

AnimNode AnimationDirector::makeDelayNode(uint16_t ms) {
    AnimNode node;
    node.type = NODE_DELAY;
    node.durationMs = ms;
    return node;
}

AnimNode AnimationDirector::makeCallbackNode(AnimNodeCallback onStart, AnimNodeCallback onComplete) {
    AnimNode node;
    node.type = NODE_CALLBACK;
    node.durationMs = 1;  // 瞬间完成
    node.onStart = onStart;
    node.onComplete = onComplete;
    return node;
}
