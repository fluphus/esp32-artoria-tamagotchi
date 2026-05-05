// src/pet/seriousness.cpp

#include "seriousness.h"
#include "../config/game_config.h"
#include <Arduino.h>

SeriousnessSystem seriousnessSystem;

// --- 工具 ---

int16_t SeriousnessSystem::clamp(int16_t value) {
    if (value > SERIOUSNESS_MAX) return SERIOUSNESS_MAX;
    if (value < SERIOUSNESS_MIN) return SERIOUSNESS_MIN;
    return value;
}

SeriousnessTier SeriousnessSystem::getTier(int16_t seriousness) {
    if (seriousness >= RHONGOMYNIAD_THRESHOLD) return TIER_MAX;
    if (seriousness > SERIOUSNESS_TIER_MID_UPPER) return TIER_HIGH;
    if (seriousness > SERIOUSNESS_TIER_LOW_UPPER) return TIER_MID;
    return TIER_LOW;
}

SeriousnessTier SeriousnessSystem::getCurrentTier(const PetState& pet) {
    return getTier(pet.seriousness);
}

// --- 狮子王计时 ---

void SeriousnessSystem::updateRhongoTimer(PetState& pet, uint32_t currentTime) {
    if (pet.alignment != ALIGN_WHITE) return;
    if (pet.stage != STAGE_ADULT) return;
    if (pet.is_rhongomyniad) return;

    if (pet.seriousness >= RHONGOMYNIAD_THRESHOLD) {
        if (pet.rhongo_timer_start == 0) {
            pet.rhongo_timer_start = currentTime;
            Serial.println("[Rhongo] Timer STARTED");
        }

        uint32_t elapsed = currentTime - pet.rhongo_timer_start;
        if (elapsed >= RHONGOMYNIAD_SUSTAIN_SEC) {
            pet.is_rhongomyniad = true;
            pet.form = FORM_WHITE_LANCER_RHONGOMYNIAD;
            Serial.println("[Rhongo] *** TRIGGERED ***");
        }
    } else if (pet.seriousness < RHONGOMYNIAD_SAFE_DROP) {
        if (pet.rhongo_timer_start > 0) {
            Serial.printf("[Rhongo] Timer RESET (SR=%d < %d)\n",
                          pet.seriousness, RHONGOMYNIAD_SAFE_DROP);
            pet.rhongo_timer_start = 0;
        }
    }
}

RhongoTimerState SeriousnessSystem::getRhongoState(const PetState& pet, uint32_t currentTime) {
    if (pet.is_rhongomyniad) return RHONGO_TRIGGERED;
    if (pet.alignment != ALIGN_WHITE) return RHONGO_INACTIVE;
    if (pet.rhongo_timer_start == 0) return RHONGO_INACTIVE;
    return RHONGO_COUNTING;
}

uint32_t SeriousnessSystem::getRhongoElapsed(const PetState& pet, uint32_t currentTime) {
    if (pet.rhongo_timer_start == 0 || pet.is_rhongomyniad) return 0;
    if (currentTime > pet.rhongo_timer_start)
        return currentTime - pet.rhongo_timer_start;
    return 0;
}

uint32_t SeriousnessSystem::getRhongoRemaining(const PetState& pet, uint32_t currentTime) {
    if (pet.is_rhongomyniad) return 0;
    if (pet.rhongo_timer_start == 0) return RHONGOMYNIAD_SUSTAIN_SEC;
    uint32_t elapsed = getRhongoElapsed(pet, currentTime);
    if (elapsed >= RHONGOMYNIAD_SUSTAIN_SEC) return 0;
    return RHONGOMYNIAD_SUSTAIN_SEC - elapsed;
}

// --- 待机 tick ---

IdleTickResult SeriousnessSystem::onIdleTick(PetState& pet, uint32_t currentTime) {
    IdleTickResult result = {};
    result.seriousness_before = pet.seriousness;
    result.tier_before = getTier(pet.seriousness);

    if (pet.is_rhongomyniad) {
        result.seriousness_after = pet.seriousness;
        result.tier_after = result.tier_before;
        result.tier_changed = false;
        result.rhongo_state = RHONGO_TRIGGERED;
        return result;
    }

    pet.idle_minute_remainder++;
    if (pet.idle_minute_remainder >= SERIOUSNESS_IDLE_INTERVAL_MIN) {
        pet.seriousness = clamp(pet.seriousness + SERIOUSNESS_IDLE_PER_TICK);
        pet.idle_minute_remainder = 0;
    }
    updateRhongoTimer(pet, currentTime);

    result.seriousness_after = pet.seriousness;
    result.tier_after = getTier(pet.seriousness);
    result.tier_changed = (result.tier_before != result.tier_after);
    result.rhongo_state = getRhongoState(pet, currentTime);
    result.rhongo_elapsed_sec = getRhongoElapsed(pet, currentTime);
    result.rhongo_remaining_sec = getRhongoRemaining(pet, currentTime);

    return result;
}

IdleTickResult SeriousnessSystem::onIdleBatch(PetState& pet, uint32_t minutes, uint32_t currentTime) {
    IdleTickResult result = {};
    result.seriousness_before = pet.seriousness;
    result.tier_before = getTier(pet.seriousness);

    if (pet.is_rhongomyniad) {
        result.seriousness_after = pet.seriousness;
        result.tier_after = result.tier_before;
        result.tier_changed = false;
        result.rhongo_state = RHONGO_TRIGGERED;
        return result;
    }

    uint32_t simTime = currentTime - (minutes * 60);
    for (uint32_t i = 0; i < minutes; i++) {
        if (pet.is_rhongomyniad) break;

        pet.idle_minute_remainder++;
        if (pet.idle_minute_remainder >= SERIOUSNESS_IDLE_INTERVAL_MIN) {
            pet.seriousness = clamp(pet.seriousness + SERIOUSNESS_IDLE_PER_TICK);
            pet.idle_minute_remainder = 0;
        }

        simTime += 60;
        updateRhongoTimer(pet, simTime);
    }

    result.seriousness_after = pet.seriousness;
    result.tier_after = getTier(pet.seriousness);
    result.tier_changed = (result.tier_before != result.tier_after);
    result.rhongo_state = getRhongoState(pet, currentTime);
    result.rhongo_elapsed_sec = getRhongoElapsed(pet, currentTime);
    result.rhongo_remaining_sec = getRhongoRemaining(pet, currentTime);

    return result;
}

// --- 互动 ---

InteractResult SeriousnessSystem::onInteract(PetState& pet, InteractType type, uint32_t currentTime) {
    InteractResult result = {};
    result.seriousness_before = pet.seriousness;
    result.tier_before = getTier(pet.seriousness);

    if (pet.is_rhongomyniad) {
        result.seriousness_after = pet.seriousness;
        result.tier_after = result.tier_before;
        result.tier_changed = false;
        result.rhongo_state = RHONGO_TRIGGERED;
        return result;
    }

    // 更新互动时间 (无论是否生效)
    pet.last_interact_time = currentTime;

    bool applyDelta = false;

    if (type == INTERACT_FEED) {
        // 投喂始终扣减严肃值
        applyDelta = true;
    } else if (type == INTERACT_POKE) {
        if (pet.stage == STAGE_CHILD) {
            // lily: 仅动画, 不影响数值
            applyDelta = false;
        } else if (!pet.daily_feed.poke_used) {
            // 成体: 每日首次生效
            applyDelta = true;
            pet.daily_feed.poke_used = true;
        } else {
            // 成体: 后续仅动画
            applyDelta = false;
        }
    }

    if (applyDelta) {
        pet.seriousness = clamp(pet.seriousness - SERIOUSNESS_INTERACT_DELTA);
    }

    updateRhongoTimer(pet, currentTime);

    result.seriousness_after = pet.seriousness;
    result.tier_after = getTier(pet.seriousness);
    result.tier_changed = (result.tier_before != result.tier_after);
    result.rhongo_state = getRhongoState(pet, currentTime);

    return result;
}

// --- 未喂满惩罚 ---

void SeriousnessSystem::applyMissedFeedPenalty(PetState& pet) {
    if (pet.is_rhongomyniad) return;

    int16_t old = pet.seriousness;
    pet.seriousness = clamp(pet.seriousness + MISSED_FEED_SERIOUSNESS);

    Serial.printf("[Seriousness] Missed feed penalty: %d -> %d (+%d)\n",
                  old, pet.seriousness, MISSED_FEED_SERIOUSNESS);
}
