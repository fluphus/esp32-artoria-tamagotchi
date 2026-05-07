// src/pet/feeding.cpp

#include "feeding.h"
#include "../config/game_config.h"
#include <Arduino.h>
#include <esp_random.h>

FeedingSystem feedingSystem;

int16_t FeedingSystem::clampHealth(int16_t value) {
    if (value > HEALTH_MAX) return HEALTH_MAX;
    if (value < HEALTH_MIN) return HEALTH_MIN;
    return value;
}

bool FeedingSystem::isInFeedWindow(uint8_t hour) {
    return (hour >= FEED_WINDOW_BREAKFAST_START && hour < FEED_WINDOW_BREAKFAST_END) ||
           (hour >= FEED_WINDOW_LUNCH_START     && hour < FEED_WINDOW_LUNCH_END) ||
           (hour >= FEED_WINDOW_DINNER_START    && hour < FEED_WINDOW_DINNER_END);
}

int8_t FeedingSystem::getWindowIndex(uint8_t hour) {
    if (hour >= FEED_WINDOW_BREAKFAST_START && hour < FEED_WINDOW_BREAKFAST_END) return 0;
    if (hour >= FEED_WINDOW_LUNCH_START     && hour < FEED_WINDOW_LUNCH_END)     return 1;
    if (hour >= FEED_WINDOW_DINNER_START    && hour < FEED_WINDOW_DINNER_END)    return 2;
    return -1;
}

uint32_t FeedingSystem::secondsUntilNextFeed(const PetState& pet, uint32_t currentTime) {
    if (pet.daily_feed.last_feed_time == 0) return 0;
    uint32_t elapsed = currentTime - pet.daily_feed.last_feed_time;
    if (elapsed >= FEED_INTERVAL_MIN_SEC) return 0;
    return FEED_INTERVAL_MIN_SEC - elapsed;
}

FeedResult FeedingSystem::canFeed(const PetState& pet, uint32_t currentTime) {
    if (pet.is_rhongomyniad || pet.is_black_rhongomyniad) return FEED_ERR_RHONGOMYNIAD;
    if (pet.daily_feed.feed_count >= DAILY_FEED_LIMIT) return FEED_ERR_DAILY_LIMIT;
    if (pet.daily_feed.last_feed_time > 0) {
        uint32_t elapsed = currentTime - pet.daily_feed.last_feed_time;
        if (elapsed < FEED_INTERVAL_MIN_SEC) return FEED_ERR_TOO_SOON;
    }
    return FEED_OK;
}

FeedDraw FeedingSystem::drawFood() {
    FeedDraw draw = {};
    uint8_t pool[FOOD_COUNT];
    for (uint8_t i = 0; i < FOOD_COUNT; i++) pool[i] = i;
    for (uint8_t i = 0; i < FEED_DRAW_COUNT; i++) {
        uint8_t remaining = FOOD_COUNT - i;
        uint8_t pick = esp_random() % remaining;
        draw.food_ids[i] = pool[pick];
        pool[pick] = pool[remaining - 1];
    }
    return draw;
}

ComboType FeedingSystem::checkCombo(const uint8_t picked[3]) {
    bool allHealthy = true, allJunk = true;
    for (uint8_t i = 0; i < FEED_PICK_COUNT; i++) {
        if (picked[i] >= FOOD_COUNT) return COMBO_NONE;
        if (FOOD_TABLE[picked[i]].is_healthy) allJunk = false;
        else allHealthy = false;
    }
    if (allHealthy) return COMBO_ALL_HEALTHY;
    if (allJunk) return COMBO_ALL_JUNK;
    return COMBO_NONE;
}

int16_t FeedingSystem::calcDislikeSeriousness(const PetState& pet, const uint8_t picked[3]) {
    if (pet.stage != STAGE_ADULT) return 0;
    int16_t penalty = 0;
    for (uint8_t i = 0; i < FEED_PICK_COUNT; i++) {
        if (picked[i] >= FOOD_COUNT) continue;
        bool healthy = FOOD_TABLE[picked[i]].is_healthy;
        if (pet.alignment == ALIGN_WHITE && !healthy) penalty += DISLIKE_FOOD_SERIOUSNESS;
        else if (pet.alignment == ALIGN_BLACK && healthy) penalty += DISLIKE_FOOD_SERIOUSNESS;
    }
    return penalty;
}

int16_t FeedingSystem::calcComboSeriousness(const PetState& pet, ComboType combo) {
    if (pet.stage != STAGE_ADULT) return 0;
    if (combo == COMBO_NONE) return 0;
    if (pet.alignment == ALIGN_WHITE) {
        if (combo == COMBO_ALL_HEALTHY) return -COMBO_SERIOUSNESS_DELTA;
        if (combo == COMBO_ALL_JUNK)    return +COMBO_SERIOUSNESS_DELTA;
    } else if (pet.alignment == ALIGN_BLACK) {
        if (combo == COMBO_ALL_HEALTHY) return +COMBO_SERIOUSNESS_DELTA;
        if (combo == COMBO_ALL_JUNK)    return -COMBO_SERIOUSNESS_DELTA;
    }
    return 0;
}

int16_t FeedingSystem::calcComboHealth(const PetState& pet, ComboType combo) {
    if (pet.stage != STAGE_CHILD) return 0;
    if (combo == COMBO_NONE) return 0;
    if (combo == COMBO_ALL_HEALTHY) return +COMBO_HEALTH_BONUS;
    if (combo == COMBO_ALL_JUNK)    return -COMBO_HEALTH_BONUS;
    return 0;
}

bool FeedingSystem::rollMapoTofu() {
    return (esp_random() % 100) < MAPO_TOFU_CHANCE_PERCENT;
}

FeedOutcome FeedingSystem::feed(PetState& pet, const uint8_t picked[3],
                                uint32_t currentTime, uint8_t currentHour) {
    FeedOutcome outcome = {};
    outcome.result = FEED_OK;
    outcome.combo_triggered = false;
    outcome.special_food_id = -1;
    outcome.mapo_tofu_triggered = false;
    outcome.mapo_tofu_total = pet.mapo_tofu_count;
    outcome.mapo_tofu_curse_activated = false;

    FeedResult check = canFeed(pet, currentTime);
    if (check != FEED_OK) {
        outcome.result = check;
        outcome.wait_seconds = secondsUntilNextFeed(pet, currentTime);
        return outcome;
    }

    for (uint8_t i = 0; i < FEED_PICK_COUNT; i++) {
        if (picked[i] >= FOOD_COUNT) {
            outcome.result = FEED_ERR_INVALID_FOOD;
            return outcome;
        }
        outcome.picked_ids[i] = picked[i];
    }

    outcome.health_before = pet.health;
    outcome.seriousness_before = pet.seriousness;

    // 健康值结算
    int16_t totalHealthDelta = 0;
    for (uint8_t i = 0; i < FEED_PICK_COUNT; i++)
        totalHealthDelta += FOOD_TABLE[picked[i]].health_delta;
    outcome.health_from_food = totalHealthDelta;

    ComboType combo = checkCombo(picked);
    outcome.combo = combo;

    int16_t comboHealth = calcComboHealth(pet, combo);
    outcome.health_from_combo = comboHealth;

    pet.health = clampHealth(pet.health + totalHealthDelta + comboHealth);

    // 严肃值: 食物喜好
    int16_t dislikeSR = calcDislikeSeriousness(pet, picked);
    outcome.seriousness_from_dislike = dislikeSR;
    pet.seriousness += dislikeSR;
    if (pet.seriousness > SERIOUSNESS_MAX) pet.seriousness = SERIOUSNESS_MAX;
    if (pet.seriousness < SERIOUSNESS_MIN) pet.seriousness = SERIOUSNESS_MIN;

    // 严肃值: 连携
    int16_t comboSR = calcComboSeriousness(pet, combo);
    outcome.seriousness_from_combo = comboSR;
    pet.seriousness += comboSR;
    if (pet.seriousness > SERIOUSNESS_MAX) pet.seriousness = SERIOUSNESS_MAX;
    if (pet.seriousness < SERIOUSNESS_MIN) pet.seriousness = SERIOUSNESS_MIN;

    if (combo != COMBO_NONE)
        outcome.combo_triggered = true;

    // 投喂记录
    pet.daily_feed.feed_count++;
    pet.daily_feed.last_feed_time = currentTime;
    pet.last_interact_time = currentTime;

    // === 时间窗统计 (按投喂次数, 多数属性判定) ===
    uint8_t healthyCount = 0;
    for (uint8_t i = 0; i < FEED_PICK_COUNT; i++) {
        if (FOOD_TABLE[picked[i]].is_healthy)
            healthyCount++;
    }
    // 3份中 >= 2份健康 -> 本次为健康投喂, 否则为垃圾投喂
    bool feedIsHealthy = (healthyCount >= 2);

    bool inWindow = isInFeedWindow(currentHour);
    outcome.in_correct_window = inWindow;

    if (inWindow) {
        if (feedIsHealthy)
            pet.daily_feed.healthy_in_window++;
        else
            pet.daily_feed.junk_in_window++;
    } else {
        if (feedIsHealthy)
            pet.daily_feed.healthy_outside_window++;
        else
            pet.daily_feed.junk_outside_window++;
    }

    outcome.health_after = pet.health;
    outcome.seriousness_after = pet.seriousness;
    outcome.result = FEED_OK;

    return outcome;
}

void FeedingSystem::applySpecialFood(PetState& pet, FeedOutcome& outcome, uint8_t specialFoodId) {
    if (!outcome.combo_triggered) return;
    if (specialFoodId >= SFOOD_COUNT) return;

    outcome.special_food_id = specialFoodId;

    Serial.printf("[Feed] Special: %s - %s\n",
                  SPECIAL_FOOD_TABLE[specialFoodId].name,
                  SPECIAL_FOOD_TABLE[specialFoodId].description);
    Serial.println("[Feed] (Special animation placeholder)");

    // 麻婆豆腐彩蛋判定
    if (rollMapoTofu()) {
        pet.mapo_tofu_count++;
        outcome.mapo_tofu_triggered = true;
        outcome.mapo_tofu_total = pet.mapo_tofu_count;

        Serial.println("[Feed] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.printf("[Feed] *** %s ***\n", MAPO_TOFU.name);
        Serial.printf("[Feed] \"%s\"\n", MAPO_TOFU.description);
        Serial.printf("[Feed] Mapo Tofu count: %d / %d\n",
                      pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);

        if (pet.mapo_tofu_count >= MAPO_TOFU_CURSE_THRESHOLD) {
            outcome.mapo_tofu_curse_activated = true;
            Serial.println("[Feed] *** CURSE ACTIVATED ***");
        }

        Serial.println("[Feed] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
}

DayEndOutcome FeedingSystem::processDayEnd(PetState& pet) {
    DayEndOutcome outcome = {};
    outcome.health_before = pet.health;
    outcome.seriousness_before = pet.seriousness;
    outcome.window_bonus_applied = false;
    outcome.window_penalty_applied = false;
    outcome.missed_feed_penalty = false;

    // 奖励: 正确时间窗内 >= 2 次健康投喂
    if (pet.daily_feed.healthy_in_window >= 2) {
        pet.health = clampHealth(pet.health + CORRECT_WINDOW_BONUS);
        outcome.window_bonus_applied = true;
    }

    // 惩罚: 非正确时间窗内 >= 2 次垃圾投喂
    if (pet.daily_feed.junk_outside_window >= 2) {
        pet.health = clampHealth(pet.health - WRONG_WINDOW_PENALTY);
        outcome.window_penalty_applied = true;
    }

    // 未喂满
    if (pet.daily_feed.feed_count < DAILY_FEED_LIMIT)
        outcome.missed_feed_penalty = true;

    outcome.health_after = pet.health;
    outcome.seriousness_after = pet.seriousness;

    return outcome;
}

void FeedingSystem::resetDaily(PetState& pet, uint8_t newDay) {
    pet.daily_feed.reset(newDay);
}
