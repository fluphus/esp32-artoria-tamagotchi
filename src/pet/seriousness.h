// src/pet/seriousness.h

#ifndef SERIOUSNESS_H
#define SERIOUSNESS_H

#include <stdint.h>
#include "../core/game_state.h"

// ============================================
// 严肃值系统
// 管理严肃值的增减、区间判定、狮子王计时
// ============================================

// 严肃值区间
enum SeriousnessTier : uint8_t {
    TIER_LOW = 0,       // 0-32:   娱乐态
    TIER_MID,           // 33-65:  基础态
    TIER_HIGH,          // 66-99:  lancer态
    TIER_MAX            // 100:    狮子王候选
};

inline const char* TIER_NAMES[] = {
    "LOW (0-32, fun)",
    "MID (33-65, base)",
    "HIGH (66-99, lancer)",
    "MAX (100, rhongo candidate)"
};

// 互动类型
enum InteractType : uint8_t {
    INTERACT_FEED = 0,
    INTERACT_POKE
};

// 狮子王计时状态
enum RhongoTimerState : uint8_t {
    RHONGO_INACTIVE = 0,    // 未触发 (严肃值 < 100)
    RHONGO_COUNTING,        // 计时中 (严肃值 == 100, 未满48h)
    RHONGO_TRIGGERED,       // 已触发 (48h已满, 不可逆)
    RHONGO_RESET            // 本次 tick 中计时被重置 (降到80以下)
};

// 待机 tick 结果
struct IdleTickResult {
    int16_t seriousness_before;
    int16_t seriousness_after;
    SeriousnessTier tier_before;
    SeriousnessTier tier_after;
    bool tier_changed;
    RhongoTimerState rhongo_state;
    uint32_t rhongo_elapsed_sec;        // 狮子王计时已过秒数
    uint32_t rhongo_remaining_sec;      // 狮子王计时剩余秒数
};

// 互动结果
struct InteractResult {
    int16_t seriousness_before;
    int16_t seriousness_after;
    SeriousnessTier tier_before;
    SeriousnessTier tier_after;
    bool tier_changed;
    RhongoTimerState rhongo_state;
};

class SeriousnessSystem {
public:
    // 待机 tick: 每分钟调用一次, 增加严肃值
    IdleTickResult onIdleTick(PetState& pet, uint32_t currentTime);

    // 批量待机: 快进 N 分钟 (调试用)
    IdleTickResult onIdleBatch(PetState& pet, uint32_t minutes, uint32_t currentTime);

    // 互动: 投喂或戳一戳时调用, 降低严肃值
    InteractResult onInteract(PetState& pet, InteractType type, uint32_t currentTime);

    // 未喂满惩罚: 日结算时调用
    void applyMissedFeedPenalty(PetState& pet);

    // 查询当前区间
    SeriousnessTier getTier(int16_t seriousness);
    SeriousnessTier getCurrentTier(const PetState& pet);

    // 查询狮子王计时状态
    RhongoTimerState getRhongoState(const PetState& pet, uint32_t currentTime);
    uint32_t getRhongoElapsed(const PetState& pet, uint32_t currentTime);
    uint32_t getRhongoRemaining(const PetState& pet, uint32_t currentTime);

private:
    int16_t clamp(int16_t value);
    void updateRhongoTimer(PetState& pet, uint32_t currentTime);
};

// 全局单例
extern SeriousnessSystem seriousnessSystem;

#endif // SERIOUSNESS_H
