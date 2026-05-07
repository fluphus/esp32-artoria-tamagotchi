// src/display/display_renderer.cpp
// 显示渲染器实现
// 当前后端: Serial 占位输出 (DISPLAY_BACKEND_SERIAL_PLACEHOLDER)
// 切换到真实屏幕时, 替换此文件中的绘制实现

#include "display_renderer.h"
#include "../config/food_table.h"
#include "../config/game_config.h"
#include <Arduino.h>

// ============================================================================
//  Serial Placeholder Backend
// ============================================================================

#if DISPLAY_BACKEND_SERIAL_PLACEHOLDER

void DisplayRenderer::init() {
    Serial.println("[Display] Renderer init (Serial placeholder)");
    Serial.printf("[Display] Virtual screen: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
}

void DisplayRenderer::clear() {
    // Serial placeholder: no-op
}

void DisplayRenderer::drawBoot(const DisplayModel& model) {
    Serial.println("[Display] === BOOT SCREEN ===");
    Serial.println("[Display]   Fate Tamagotchi");
    Serial.println("[Display]   Loading...");
}

void DisplayRenderer::drawIdle(const DisplayModel& model) {
    const PetState& p = model.petSnapshot;
    Serial.println("[Display] --- IDLE ---");
    Serial.printf("[Display]   %s  %s  HP:%d  SR:%d\n",
        FORM_NAMES[p.form], model.timeStr, p.health, p.seriousness);
    Serial.printf("[Display]   Day %d  %s\n", model.ageDay, model.dateStr);
}

void DisplayRenderer::drawStatus(const DisplayModel& model) {
    const PetState& p = model.petSnapshot;
    Serial.println("[Display] --- STATUS ---");
    Serial.printf("[Display]   Form: %s\n", FORM_NAMES[p.form]);
    Serial.printf("[Display]   HP: %d  SR: %d\n", p.health, p.seriousness);
    Serial.printf("[Display]   Age: Day %d  Stage: %s\n", p.age_days + 1,
        p.stage == STAGE_CHILD ? "Child" : "Adult");
    Serial.printf("[Display]   Align: %s\n", ALIGNMENT_NAMES[p.alignment]);
}

void DisplayRenderer::drawFeedDraw(const DisplayModel& model) {
    Serial.println("[Display] --- FEED DRAW ---");
    for (uint8_t i = 0; i < FEED_DRAW_COUNT; i++) {
        uint8_t id = model.feedDraw.food_ids[i];
        Serial.printf("[Display]   [%d] %s\n", i, FOOD_TABLE[id].name);
    }
}

void DisplayRenderer::drawFeedPick(const DisplayModel& model) {
    Serial.println("[Display] --- FEED PICK ---");
    for (uint8_t i = 0; i < FEED_DRAW_COUNT; i++) {
        uint8_t id = model.feedDraw.food_ids[i];
        char marker = ' ';
        if (model.feedSelected[i]) marker = 'X';
        if (i == model.feedCursor) marker = (model.feedSelected[i]) ? '#' : '>';
        Serial.printf("[Display]   %c [%d] %s\n", marker, i, FOOD_TABLE[id].name);
    }
}

void DisplayRenderer::drawFeedResult(const DisplayModel& model) {
    Serial.println("[Display] --- FEED RESULT ---");
    Serial.printf("[Display]   HP: %d -> %d\n",
        model.feedOutcome.health_before, model.feedOutcome.health_after);
    Serial.printf("[Display]   SR after: %d\n", model.feedSrAfter);
    if (model.feedOutcome.combo_triggered) {
        Serial.printf("[Display]   COMBO: %s\n", COMBO_NAMES[model.feedOutcome.combo]);
    }
}

void DisplayRenderer::drawSpecialFood(const DisplayModel& model) {
    Serial.println("[Display] --- SPECIAL FOOD ---");
    for (uint8_t i = 0; i < model.specialFoodCount && i < SFOOD_COUNT; i++) {
        char marker = (i == model.specialFoodCursor) ? '>' : ' ';
        Serial.printf("[Display]   %c %s\n", marker, SPECIAL_FOOD_TABLE[i].name);
    }
    if (model.mapoTriggered) {
        Serial.printf("[Display]   !! MAPO TOFU (%d/%d) !!\n",
            model.mapoCount, MAPO_TOFU_CURSE_THRESHOLD);
    }
}

void DisplayRenderer::drawPoke(const DisplayModel& model) {
    Serial.println("[Display] --- POKE ---");
    if (model.pokeValueChanged) {
        Serial.printf("[Display]   SR: %d -> %d\n", model.pokeSrBefore, model.pokeSrAfter);
    } else {
        Serial.println("[Display]   (cooldown - animation only)");
    }
}

void DisplayRenderer::drawEvolution(const DisplayModel& model) {
    Serial.println("[Display] --- EVOLUTION ---");
    Serial.printf("[Display]   %s -> %s\n",
        FORM_NAMES[model.evolution.form_before],
        FORM_NAMES[model.evolution.form_after]);
    Serial.printf("[Display]   Event: %s\n", EVO_EVENT_NAMES[model.evolution.event]);
}

void DisplayRenderer::drawDestroyConfirm(const DisplayModel& model) {
    Serial.println("[Display] --- DESTROY CONFIRM ---");
    Serial.printf("[Display]   %s YES   %s NO\n",
        model.destroyCursor == 0 ? "[>]" : "[ ]",
        model.destroyCursor == 1 ? "[>]" : "[ ]");
}

void DisplayRenderer::drawDayEnd(const DisplayModel& model) {
    Serial.println("[Display] --- DAY END ---");
    if (model.dayEnd.terminalState) {
        Serial.println("[Display]   Terminal state reached.");
        return;
    }
    Serial.printf("[Display]   Idle SR: %d -> %d\n",
        model.dayEnd.idleSrBefore, model.dayEnd.idleSrAfter);
    if (model.dayEnd.windowBonusApplied)
        Serial.printf("[Display]   Window bonus: +%d HP\n", model.dayEnd.windowBonusHP);
    if (model.dayEnd.windowPenaltyApplied)
        Serial.printf("[Display]   Window penalty: -%d HP\n", model.dayEnd.windowPenaltyHP);
    if (model.dayEnd.missedFeedPenalty)
        Serial.printf("[Display]   Missed feeds: %d/%d\n", model.dayEnd.fedCount, model.dayEnd.feedLimit);
    Serial.printf("[Display]   Day %d complete.\n", model.dayEnd.dayNumber);
}

void DisplayRenderer::drawToast(const DisplayModel& model) {
    if (model.toast[0] != '\0') {
        Serial.printf("[Display] TOAST: %s\n", model.toast);
    }
}

void DisplayRenderer::drawSprite(int16_t x, int16_t y, const SpriteAsset& sprite, const char* label) {
    if (hasSprite(sprite)) {
        Serial.printf("[Display]   Sprite @(%d,%d) %dx%d\n", x, y, sprite.width, sprite.height);
    } else {
        Serial.printf("[Display]   [placeholder] @(%d,%d) %s\n", x, y, label ? label : "?");
    }
}

void DisplayRenderer::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                       int16_t value, int16_t maxValue, uint16_t color) {
    int pct = (maxValue > 0) ? (value * 100 / maxValue) : 0;
    Serial.printf("[Display]   Bar @(%d,%d) %d%%\n", x, y, pct);
}

void DisplayRenderer::drawTextCentered(int16_t y, const char* text) {
    Serial.printf("[Display]   [center@%d] %s\n", y, text);
}

#endif // DISPLAY_BACKEND_SERIAL_PLACEHOLDER

// ============================================================================
//  TFT_eSPI Backend (stub - user fills in)
// ============================================================================

#if DISPLAY_BACKEND_TFT_ESPI

// #include <TFT_eSPI.h>
// static TFT_eSPI tft;

void DisplayRenderer::init() {
    // tft.init();
    // tft.setRotation(SCREEN_ROTATION);
    // tft.fillScreen(SCREEN_BG_COLOR);
    // if (TFT_BL_PIN >= 0) { pinMode(TFT_BL_PIN, OUTPUT); digitalWrite(TFT_BL_PIN, HIGH); }
}

void DisplayRenderer::clear() {
    // tft.fillScreen(SCREEN_BG_COLOR);
}

void DisplayRenderer::drawBoot(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawIdle(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawStatus(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawFeedDraw(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawFeedPick(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawFeedResult(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawSpecialFood(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawPoke(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawEvolution(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawDestroyConfirm(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawDayEnd(const DisplayModel& model) { /* TODO */ }
void DisplayRenderer::drawToast(const DisplayModel& model) { /* TODO */ }

void DisplayRenderer::drawSprite(int16_t x, int16_t y, const SpriteAsset& sprite, const char* label) { /* TODO */ }
void DisplayRenderer::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                       int16_t value, int16_t maxValue, uint16_t color) { /* TODO */ }
void DisplayRenderer::drawTextCentered(int16_t y, const char* text) { /* TODO */ }

#endif // DISPLAY_BACKEND_TFT_ESPI
