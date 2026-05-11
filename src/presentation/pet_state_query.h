// src/presentation/pet_state_query.h
// 宠物状态查询接口 - 逻辑层 Getter
// 渲染层/演出层通过此接口获取判定结果，绝不直接读取数值做比较
// 所有阈值定义在 game_config.h，此处只封装判定逻辑

#ifndef PET_STATE_QUERY_H
#define PET_STATE_QUERY_H

#include <stdint.h>
#include "../core/game_state.h"
#include "../config/game_config.h"
#include "../pet/feeding.h"
#include "../pet/seriousness.h"
#include "presentation_types.h"

// ============================================================================
//  HP 分级
// ============================================================================

enum HPTier : uint8_t {
    HP_TIER_CRITICAL = 0,   // HP < HP_TIER_CRITICAL_MAX
    HP_TIER_LOW,            // HP < HP_TIER_LOW_MAX
    HP_TIER_MID,            // HP < HP_TIER_MID_MAX
    HP_TIER_HIGH            // HP >= HP_TIER_MID_MAX
};

// ============================================================================
//  PetStateQuery - 静态查询接口
// ============================================================================

class PetStateQuery {
public:
    // --- 阵营判定 ---
    static bool isWhiteAlignment(const PetState& pet);
    static bool isBlackAlignment(const PetState& pet);

    // --- SR 判定 ---
    static SeriousnessTier getSRTier(const PetState& pet);
    static bool isSRMax(const PetState& pet);           // SR == SERIOUSNESS_MAX
    static bool isSRHigh(const PetState& pet);          // SR >= LION_KING_SR_CRITICAL_MIN

    // --- HP 判定 ---
    static HPTier getHPTier(const PetState& pet);
    static bool isHPCritical(const PetState& pet);      // HP < HP_TIER_CRITICAL_MAX
    static bool isHPLow(const PetState& pet);           // HP < HP_TIER_LOW_MAX

    // --- 狮子王特殊判定 ---
    static bool isLionKingTimerActive(const PetState& pet);     // rhongo_timer_start > 0
    static bool isLionKingSrCritical(const PetState& pet);      // timer active && 100 > SR >= 80
    static bool isLionKingSrMax(const PetState& pet);           // SR == 100

    // --- 终态判定 ---
    static bool isTerminalState(const PetState& pet);           // 任何不可逆终态
    static bool isRhongomyniad(const PetState& pet);
    static bool isBlackRhongomyniad(const PetState& pet);
    static bool isNobu(const PetState& pet);
    static bool isOdaNobunaga(const PetState& pet);

    // --- 投喂反应判定 ---
    // 根据阵营和食物属性，返回单食物反应类型
    static ReactionType getPerFoodReaction(const PetState& pet, bool foodIsHealthy);

    // 根据阵营和整轮投喂结果，返回整轮总结反应
    // healthyCount: 本轮 3 份食物中 healthy 的数量
    static ReactionType getRoundSummaryReaction(const PetState& pet, uint8_t healthyCount);

    // 根据阵营和 combo 类型，返回 satisfy/abhor
    static ReactionType getComboReaction(const PetState& pet, ComboType combo);

    // --- Perfect 判定 ---
    // 当天第三次投喂且满足每日条件
    static bool shouldPlayPerfect(const PetState& pet);

    // --- 形态是否属于白线/黑线 ---
    static bool isWhiteLineForm(Form form);
    static bool isBlackLineForm(Form form);
};

#endif // PET_STATE_QUERY_H
