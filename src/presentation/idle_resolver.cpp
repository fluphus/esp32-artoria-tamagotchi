// src/presentation/idle_resolver.cpp
// 待机动画多态选择器实现

#include "idle_resolver.h"
#include "pet_state_query.h"
#include <Arduino.h>
#include <esp_random.h>

// 静态成员初始化
IdleAnimConfig IdleResolver::_config = DEFAULT_IDLE_CONFIG;
uint32_t IdleResolver::_lastRandomCheckMs = 0;

// ============================================================================
//  配置
// ============================================================================

void IdleResolver::setConfig(const IdleAnimConfig& config) {
    _config = config;
}

const IdleAnimConfig& IdleResolver::getConfig() {
    return _config;
}

// ============================================================================
//  主入口
// ============================================================================

IdleAnimId IdleResolver::resolve(const PetState& pet) {
    // 优先级 1: 终态
    IdleAnimId id = checkTerminalStates(pet);
    if (id != IDLE_NONE) return id;

    // 优先级 2: 白 Lancer 狮子王特殊
    id = checkLionKingSpecial(pet);
    if (id != IDLE_NONE) return id;

    // 优先级 3: 形态 × SR × HP 组合
    return checkFormSrHp(pet);
}

// ============================================================================
//  随机待机
// ============================================================================

RandomIdleAnimId IdleResolver::resolveRandom(const PetState& pet, uint32_t idleDurationMs) {
    // 未达到最小待机时间
    if (idleDurationMs < _config.minIdleBeforeRandomMs) {
        return RANDOM_IDLE_NONE;
    }

    // 检查间隔
    uint32_t now = millis();
    if (now - _lastRandomCheckMs < _config.randomCheckIntervalMs) {
        return RANDOM_IDLE_NONE;
    }
    _lastRandomCheckMs = now;

    // 概率判定
    uint8_t roll = esp_random() % 100;
    if (roll >= _config.randomChancePercent) {
        return RANDOM_IDLE_NONE;
    }

    // 根据形态选择随机动画池
    switch (pet.form) {
        case FORM_LILY:
            return (esp_random() % 2 == 0) ? RANDOM_IDLE_LILY_YAWN : RANDOM_IDLE_LILY_STRETCH;

        case FORM_WHITE_SABER:
            return (esp_random() % 2 == 0) ? RANDOM_IDLE_WHITE_SABER_LOOK_AROUND
                                           : RANDOM_IDLE_WHITE_SABER_SIGH;

        case FORM_BLACK_SABER:
            return (esp_random() % 2 == 0) ? RANDOM_IDLE_BLACK_SABER_SMIRK
                                           : RANDOM_IDLE_BLACK_SABER_CROSS_ARMS;

        // 其他形态暂时返回 NONE，待资源就绪后添加
        default:
            return RANDOM_IDLE_NONE;
    }
}

// ============================================================================
//  判定链
// ============================================================================

IdleAnimId IdleResolver::checkTerminalStates(const PetState& pet) {
    if (PetStateQuery::isRhongomyniad(pet)) return IDLE_RHONGOMYNIAD;
    if (PetStateQuery::isBlackRhongomyniad(pet)) return IDLE_BLACK_RHONGOMYNIAD;
    if (PetStateQuery::isOdaNobunaga(pet)) return IDLE_ODA_NOBUNAGA_NORMAL;
    if (PetStateQuery::isNobu(pet)) return IDLE_NOBU_NORMAL;
    return IDLE_NONE;
}

IdleAnimId IdleResolver::checkLionKingSpecial(const PetState& pet) {
    // 只对白 Lancer 生效
    if (pet.form != FORM_WHITE_LANCER) return IDLE_NONE;

    if (PetStateQuery::isLionKingSrMax(pet)) {
        return IDLE_WHITE_LANCER_LION_KING_SR_MAX;
    }
    if (PetStateQuery::isLionKingSrCritical(pet)) {
        return IDLE_WHITE_LANCER_LION_KING_SR_CRITICAL;
    }
    return IDLE_NONE;
}

IdleAnimId IdleResolver::checkFormSrHp(const PetState& pet) {
    HPTier hp = PetStateQuery::getHPTier(pet);
    bool srHigh = PetStateQuery::isSRHigh(pet);

    // HP 优先级高于 SR (濒危/低血量优先表现)
    switch (pet.form) {
        case FORM_LILY:
            if (hp == HP_TIER_CRITICAL) return IDLE_LILY_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_LILY_HP_LOW;
            return IDLE_LILY_NORMAL;

        case FORM_WHITE_SABER:
            if (hp == HP_TIER_CRITICAL) return IDLE_WHITE_SABER_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_WHITE_SABER_HP_LOW;
            if (srHigh) return IDLE_WHITE_SABER_SR_HIGH;
            return IDLE_WHITE_SABER_NORMAL;

        case FORM_BLACK_SABER:
            if (hp == HP_TIER_CRITICAL) return IDLE_BLACK_SABER_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_BLACK_SABER_HP_LOW;
            if (srHigh) return IDLE_BLACK_SABER_SR_HIGH;
            return IDLE_BLACK_SABER_NORMAL;

        case FORM_WHITE_LANCER:
            if (hp == HP_TIER_CRITICAL) return IDLE_WHITE_LANCER_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_WHITE_LANCER_HP_LOW;
            if (srHigh) return IDLE_WHITE_LANCER_SR_HIGH;
            return IDLE_WHITE_LANCER_NORMAL;

        case FORM_BLACK_LANCER:
            if (hp == HP_TIER_CRITICAL) return IDLE_BLACK_LANCER_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_BLACK_LANCER_HP_LOW;
            if (srHigh) return IDLE_BLACK_LANCER_SR_HIGH;
            return IDLE_BLACK_LANCER_NORMAL;

        case FORM_WHITE_ARCHER:
            if (hp == HP_TIER_CRITICAL) return IDLE_WHITE_ARCHER_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_WHITE_ARCHER_HP_LOW;
            if (srHigh) return IDLE_WHITE_ARCHER_SR_HIGH;
            return IDLE_WHITE_ARCHER_NORMAL;

        case FORM_BLACK_RIDER:
            if (hp == HP_TIER_CRITICAL) return IDLE_BLACK_RIDER_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_BLACK_RIDER_HP_LOW;
            if (srHigh) return IDLE_BLACK_RIDER_SR_HIGH;
            return IDLE_BLACK_RIDER_NORMAL;

        case FORM_WHITE_RULER:
            if (hp == HP_TIER_CRITICAL) return IDLE_WHITE_RULER_HP_CRITICAL;
            if (hp == HP_TIER_LOW) return IDLE_WHITE_RULER_HP_LOW;
            if (srHigh) return IDLE_WHITE_RULER_SR_HIGH;
            return IDLE_WHITE_RULER_NORMAL;

        default:
            return IDLE_LILY_NORMAL;  // fallback
    }
}
