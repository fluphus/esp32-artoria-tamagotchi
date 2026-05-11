// src/presentation/pet_state_query.cpp
// 宠物状态查询接口实现

#include "pet_state_query.h"
#include "../pet/seriousness.h"

// ============================================================================
//  阵营判定
// ============================================================================

bool PetStateQuery::isWhiteAlignment(const PetState& pet) {
    return pet.alignment == ALIGN_WHITE;
}

bool PetStateQuery::isBlackAlignment(const PetState& pet) {
    return pet.alignment == ALIGN_BLACK;
}

// ============================================================================
//  SR 判定
// ============================================================================

SeriousnessTier PetStateQuery::getSRTier(const PetState& pet) {
    if (pet.seriousness >= SERIOUSNESS_MAX) return TIER_MAX;
    if (pet.seriousness > SERIOUSNESS_TIER_MID_UPPER) return TIER_HIGH;
    if (pet.seriousness > SERIOUSNESS_TIER_LOW_UPPER) return TIER_MID;
    return TIER_LOW;
}

bool PetStateQuery::isSRMax(const PetState& pet) {
    return pet.seriousness >= SERIOUSNESS_MAX;
}

bool PetStateQuery::isSRHigh(const PetState& pet) {
    return pet.seriousness >= LION_KING_SR_CRITICAL_MIN;
}

// ============================================================================
//  HP 判定
// ============================================================================

HPTier PetStateQuery::getHPTier(const PetState& pet) {
    if (pet.health < HP_TIER_CRITICAL_MAX) return HP_TIER_CRITICAL;
    if (pet.health < HP_TIER_LOW_MAX) return HP_TIER_LOW;
    if (pet.health < HP_TIER_MID_MAX) return HP_TIER_MID;
    return HP_TIER_HIGH;
}

bool PetStateQuery::isHPCritical(const PetState& pet) {
    return pet.health < HP_TIER_CRITICAL_MAX;
}

bool PetStateQuery::isHPLow(const PetState& pet) {
    return pet.health < HP_TIER_LOW_MAX;
}

// ============================================================================
//  狮子王特殊判定
// ============================================================================

bool PetStateQuery::isLionKingTimerActive(const PetState& pet) {
    return pet.rhongo_timer_start > 0 && !pet.is_rhongomyniad;
}

bool PetStateQuery::isLionKingSrCritical(const PetState& pet) {
    // timer active && 100 > SR >= 80
    return isLionKingTimerActive(pet) &&
           pet.seriousness >= LION_KING_SR_CRITICAL_MIN &&
           pet.seriousness < SERIOUSNESS_MAX;
}

bool PetStateQuery::isLionKingSrMax(const PetState& pet) {
    return pet.seriousness >= SERIOUSNESS_MAX;
}

// ============================================================================
//  终态判定
// ============================================================================

bool PetStateQuery::isTerminalState(const PetState& pet) {
    return pet.is_rhongomyniad || pet.is_black_rhongomyniad ||
           pet.is_nobu || pet.is_oda_nobunaga;
}

bool PetStateQuery::isRhongomyniad(const PetState& pet) {
    return pet.is_rhongomyniad;
}

bool PetStateQuery::isBlackRhongomyniad(const PetState& pet) {
    return pet.is_black_rhongomyniad;
}

bool PetStateQuery::isNobu(const PetState& pet) {
    return pet.is_nobu && !pet.is_oda_nobunaga;
}

bool PetStateQuery::isOdaNobunaga(const PetState& pet) {
    return pet.is_oda_nobunaga;
}

// ============================================================================
//  投喂反应判定
// ============================================================================

// 内部辅助: 判断是否为幼体组 (Lily 或 Nobu)
static bool isChildGroup(const PetState& pet) {
    return pet.form == FORM_LILY || (pet.is_nobu && !pet.is_oda_nobunaga);
}

// 内部辅助: 获取本次投喂的有效阵营偏好
// 幼体组: 基于当前 HP (投喂结算后) 决定, HP>=50 白线, HP<50 黑线
// 成体: 基于固定阵营
static bool effectivelyBlack(const PetState& pet) {
    if (isChildGroup(pet)) {
        return pet.health < 50;
    }
    return PetStateQuery::isBlackAlignment(pet);
}

ReactionType PetStateQuery::getPerFoodReaction(const PetState& pet, bool foodIsHealthy) {
    // 白线偏好: 喜欢 healthy, 讨厌 junk
    // 黑线偏好: 喜欢 junk, 讨厌 healthy
    if (effectivelyBlack(pet)) {
        return foodIsHealthy ? REACTION_DISLIKE : REACTION_LIKE;
    }
    return foodIsHealthy ? REACTION_LIKE : REACTION_DISLIKE;
}

ReactionType PetStateQuery::getRoundSummaryReaction(const PetState& pet, uint8_t healthyCount) {
    bool isHealthyRound = (healthyCount >= 2);

    if (effectivelyBlack(pet)) {
        return isHealthyRound ? REACTION_EWW : REACTION_UMU;
    }
    return isHealthyRound ? REACTION_UMU : REACTION_EWW;
}

ReactionType PetStateQuery::getComboReaction(const PetState& pet, ComboType combo) {
    if (combo == COMBO_NONE) return REACTION_NONE;

    if (effectivelyBlack(pet)) {
        return (combo == COMBO_ALL_JUNK) ? REACTION_SATISFY : REACTION_ABHOR;
    }
    return (combo == COMBO_ALL_HEALTHY) ? REACTION_SATISFY : REACTION_ABHOR;
}

// ============================================================================
//  Perfect 判定
// ============================================================================

bool PetStateQuery::shouldPlayPerfect(const PetState& pet) {
    // 当天第三次投喂 (feed_count 在投喂后已递增，所以此时 == 3)
    if (pet.daily_feed.feed_count < DAILY_FEED_LIMIT) return false;

    // 白线: healthy_in_window >= 2
    // 黑线: junk_outside_window >= 2
    if (isWhiteAlignment(pet)) {
        return pet.daily_feed.healthy_in_window >= 2;
    }
    if (isBlackAlignment(pet)) {
        return pet.daily_feed.junk_outside_window >= 2;
    }
    // 未确定阵营不触发 perfect
    return false;
}

// ============================================================================
//  形态线判定
// ============================================================================

bool PetStateQuery::isWhiteLineForm(Form form) {
    switch (form) {
        case FORM_LILY:  // Lily 视为白线 (初始形态)
        case FORM_WHITE_SABER:
        case FORM_WHITE_LANCER:
        case FORM_WHITE_ARCHER:
        case FORM_WHITE_RULER:
        case FORM_WHITE_LANCER_RHONGOMYNIAD:
            return true;
        default:
            return false;
    }
}

bool PetStateQuery::isBlackLineForm(Form form) {
    switch (form) {
        case FORM_BLACK_SABER:
        case FORM_BLACK_LANCER:
        case FORM_BLACK_RIDER:
        case FORM_BLACK_LANCER_RHONGOMYNIAD:
            return true;
        default:
            return false;
    }
}
