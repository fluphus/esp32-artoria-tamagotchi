// src/presentation/idle_resolver.h
// 待机动画多态选择器
// 根据 (形态 × SR × HP × 特殊状态) 组合返回应播放的待机动画资源 ID
// 预留随机待机动画接口

#ifndef IDLE_RESOLVER_H
#define IDLE_RESOLVER_H

#include <stdint.h>
#include "../core/game_state.h"
#include "presentation_types.h"

// ============================================================================
//  待机动画 ID (资源索引)
//  命名规则: IDLE_{FORM}_{CONDITION}
//  资源文件名与此 ID 对应 (由构建脚本自动映射)
// ============================================================================

enum IdleAnimId : uint16_t {
    IDLE_NONE = 0,

    // --- Lily ---
    IDLE_LILY_NORMAL = 1,
    IDLE_LILY_HP_LOW,
    IDLE_LILY_HP_CRITICAL,

    // --- White Saber ---
    IDLE_WHITE_SABER_NORMAL = 10,
    IDLE_WHITE_SABER_SR_HIGH,
    IDLE_WHITE_SABER_HP_LOW,
    IDLE_WHITE_SABER_HP_CRITICAL,

    // --- Black Saber ---
    IDLE_BLACK_SABER_NORMAL = 20,
    IDLE_BLACK_SABER_SR_HIGH,
    IDLE_BLACK_SABER_HP_LOW,
    IDLE_BLACK_SABER_HP_CRITICAL,

    // --- White Lancer ---
    IDLE_WHITE_LANCER_NORMAL = 30,
    IDLE_WHITE_LANCER_SR_HIGH,
    IDLE_WHITE_LANCER_HP_LOW,
    IDLE_WHITE_LANCER_HP_CRITICAL,
    IDLE_WHITE_LANCER_LION_KING_SR_MAX,         // SR == 100 专属
    IDLE_WHITE_LANCER_LION_KING_SR_CRITICAL,    // timer active && 100 > SR >= 80

    // --- Black Lancer ---
    IDLE_BLACK_LANCER_NORMAL = 40,
    IDLE_BLACK_LANCER_SR_HIGH,
    IDLE_BLACK_LANCER_HP_LOW,
    IDLE_BLACK_LANCER_HP_CRITICAL,

    // --- White Archer ---
    IDLE_WHITE_ARCHER_NORMAL = 50,
    IDLE_WHITE_ARCHER_SR_HIGH,
    IDLE_WHITE_ARCHER_HP_LOW,
    IDLE_WHITE_ARCHER_HP_CRITICAL,

    // --- Black Rider ---
    IDLE_BLACK_RIDER_NORMAL = 60,
    IDLE_BLACK_RIDER_SR_HIGH,
    IDLE_BLACK_RIDER_HP_LOW,
    IDLE_BLACK_RIDER_HP_CRITICAL,

    // --- White Ruler ---
    IDLE_WHITE_RULER_NORMAL = 70,
    IDLE_WHITE_RULER_SR_HIGH,
    IDLE_WHITE_RULER_HP_LOW,
    IDLE_WHITE_RULER_HP_CRITICAL,

    // --- Rhongomyniad (终态) ---
    IDLE_RHONGOMYNIAD = 80,

    // --- Black Rhongomyniad (终态) ---
    IDLE_BLACK_RHONGOMYNIAD = 85,

    // --- Nobu ---
    IDLE_NOBU_NORMAL = 90,

    // --- Oda Nobunaga ---
    IDLE_ODA_NOBUNAGA_NORMAL = 95,

    IDLE_ID_COUNT = 100     // 预留空间
};

// ============================================================================
//  随机待机动画 ID (在普通待机基础上偶尔触发的特殊动作)
// ============================================================================

enum RandomIdleAnimId : uint16_t {
    RANDOM_IDLE_NONE = 0,

    // 每个形态可以有多个随机待机动画
    RANDOM_IDLE_LILY_YAWN = 1,
    RANDOM_IDLE_LILY_STRETCH,

    RANDOM_IDLE_WHITE_SABER_LOOK_AROUND = 10,
    RANDOM_IDLE_WHITE_SABER_SIGH,

    RANDOM_IDLE_BLACK_SABER_SMIRK = 20,
    RANDOM_IDLE_BLACK_SABER_CROSS_ARMS,

    // ... 其他形态的随机待机动画按需添加
    // 新形态只需在此处追加 ID，并在 assets 目录放入对应资源文件

    RANDOM_IDLE_ID_COUNT = 200  // 预留空间
};

// ============================================================================
//  IdleResolver 类
// ============================================================================

class IdleResolver {
public:
    // 主入口: 根据宠物状态返回当前应播放的待机动画 ID
    static IdleAnimId resolve(const PetState& pet);

    // 随机待机: 根据待机时长判断是否触发随机动画
    // 返回 RANDOM_IDLE_NONE 表示不触发
    static RandomIdleAnimId resolveRandom(const PetState& pet, uint32_t idleDurationMs);

    // 配置
    static void setConfig(const IdleAnimConfig& config);
    static const IdleAnimConfig& getConfig();

private:
    static IdleAnimConfig _config;
    static uint32_t _lastRandomCheckMs;

    // 判定链 (优先级从高到低)
    static IdleAnimId checkTerminalStates(const PetState& pet);
    static IdleAnimId checkLionKingSpecial(const PetState& pet);
    static IdleAnimId checkFormSrHp(const PetState& pet);
};

#endif // IDLE_RESOLVER_H
