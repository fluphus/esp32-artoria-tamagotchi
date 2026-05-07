// src/display/display_renderer.cpp
// 显示渲染器实现
// 支持多后端: Serial 占位 / TFT_eSPI (SSD1351 128x128 65K OLED)

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
    if (model.bootMessage[0] != '\0')
        Serial.printf("[Display]   %s\n", model.bootMessage);
    else
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

void DisplayRenderer::drawWaitTimeSet(const DisplayModel& model) {
    Serial.println("[Display] --- WAITING FOR TIME ---");
    Serial.println("[Display]   Please set time via serial:");
    Serial.println("[Display]   SET_TIME <unix_timestamp>");
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
//  TFT_eSPI Backend (SSD1351 128x128 65K Color OLED)
// ============================================================================

#if DISPLAY_BACKEND_TFT_ESPI

#include <TFT_eSPI.h>
#include "DisplayManager.h"

static TFT_eSPI tft = TFT_eSPI();

// --- Helper: get HP bar color based on value ---
static uint16_t getHPColor(int16_t hp) {
    if (hp <= 20) return COLOR_HP_CRIT;
    if (hp <= 40) return COLOR_HP_LOW;
    return COLOR_HP;
}

// --- Helper: get SR bar color based on value ---
static uint16_t getSRColor(int16_t sr) {
    if (sr >= 80) return COLOR_SR_HIGH;
    return COLOR_SR;
}

void DisplayRenderer::init() {
    tft.init();
    tft.setRotation(SCREEN_ROTATION);
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);

    // SSD1351 OLED has no backlight pin typically
    if (TFT_BL_PIN >= 0) {
        pinMode(TFT_BL_PIN, OUTPUT);
        digitalWrite(TFT_BL_PIN, HIGH);
    }
}

void DisplayRenderer::clear() {
    tft.fillScreen(COLOR_BG);
}

void DisplayRenderer::drawBoot(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    // Title
    tft.setTextColor(COLOR_EVOLUTION, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Fate Tamagotchi", SCREEN_WIDTH / 2, 30);

    // Subtitle
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("Artoria Edition", SCREEN_WIDTH / 2, 50);

    // Boot message
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    if (model.bootMessage[0] != '\0') {
        tft.drawString(model.bootMessage, SCREEN_WIDTH / 2, 80);
    } else {
        tft.drawString("Loading...", SCREEN_WIDTH / 2, 80);
    }

    // Version indicator
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("v1.0", SCREEN_WIDTH / 2, 110);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawIdle(const DisplayModel& model) {
    const PetState& p = model.petSnapshot;
    tft.fillScreen(COLOR_BG);

    // Form sprite or placeholder
    drawSprite(IDLE_SPRITE_X, IDLE_SPRITE_Y, SPRITE_FORM[p.form], FORM_NAMES[p.form]);

    // HP bar
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(IDLE_HP_BAR_X, IDLE_HP_BAR_Y - 9);
    tft.print("HP");
    drawProgressBar(IDLE_HP_BAR_X + 16, IDLE_HP_BAR_Y, IDLE_HP_BAR_W - 16, IDLE_HP_BAR_H,
                    p.health, HEALTH_MAX, getHPColor(p.health));

    // SR bar
    tft.setCursor(IDLE_SR_BAR_X, IDLE_SR_BAR_Y - 9);
    tft.print("SR");
    drawProgressBar(IDLE_SR_BAR_X + 16, IDLE_SR_BAR_Y, IDLE_SR_BAR_W - 16, IDLE_SR_BAR_H,
                    p.seriousness, SERIOUSNESS_MAX, getSRColor(p.seriousness));

    // Time and Day
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(4, IDLE_TIME_Y);
    tft.printf("%s  Day%d", model.timeStr, model.ageDay);

    // Key hints
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString("L:Feed M:Status R:Poke", SCREEN_WIDTH - 2, IDLE_HINT_Y);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawStatus(const DisplayModel& model) {
    const PetState& p = model.petSnapshot;
    tft.fillScreen(COLOR_BG);

    tft.setTextColor(COLOR_EVOLUTION, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("- STATUS -", SCREEN_WIDTH / 2, 2);
    tft.setTextDatum(TL_DATUM);

    int16_t y = STATUS_START_Y + STATUS_LINE_H;
    tft.setTextColor(COLOR_TEXT, COLOR_BG);

    tft.setCursor(STATUS_LABEL_X, y); tft.print("Form:");
    tft.setCursor(STATUS_VALUE_X, y); tft.print(FORM_NAMES[p.form]);
    y += STATUS_LINE_H;

    tft.setCursor(STATUS_LABEL_X, y); tft.print("Stage:");
    tft.setCursor(STATUS_VALUE_X, y); tft.print(p.stage == STAGE_CHILD ? "Child" : "Adult");
    y += STATUS_LINE_H;

    tft.setCursor(STATUS_LABEL_X, y); tft.print("Align:");
    tft.setCursor(STATUS_VALUE_X, y); tft.print(ALIGNMENT_NAMES[p.alignment]);
    y += STATUS_LINE_H;

    tft.setCursor(STATUS_LABEL_X, y); tft.printf("HP: %d/%d", p.health, HEALTH_MAX);
    y += STATUS_LINE_H;

    tft.setCursor(STATUS_LABEL_X, y); tft.printf("SR: %d/%d", p.seriousness, SERIOUSNESS_MAX);
    y += STATUS_LINE_H;

    tft.setCursor(STATUS_LABEL_X, y); tft.printf("Age: Day %d", p.age_days + 1);
    y += STATUS_LINE_H;

    tft.setCursor(STATUS_LABEL_X, y); tft.printf("Fed: %d/%d", p.daily_feed.feed_count, DAILY_FEED_LIMIT);
    y += STATUS_LINE_H;

    if (p.mapo_tofu_count > 0) {
        tft.setTextColor(COLOR_WARN, COLOR_BG);
        tft.setCursor(STATUS_LABEL_X, y); tft.printf("Mapo: %d/%d", p.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
    }

    // Hint at bottom
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("[M] Close", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 2);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawFeedDraw(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("~ FOOD DRAW ~", SCREEN_WIDTH / 2, 4);
    tft.setTextDatum(TL_DATUM);

    // Draw 4 food cards in a row
    int16_t startX = (SCREEN_WIDTH - (FEED_CARD_SIZE * 4 + FEED_CARD_GAP * 3)) / 2;
    for (uint8_t i = 0; i < FEED_DRAW_COUNT; i++) {
        uint8_t id = model.feedDraw.food_ids[i];
        int16_t cx = startX + i * (FEED_CARD_SIZE + FEED_CARD_GAP);

        // Card background
        tft.drawRect(cx, FEED_CARD_Y, FEED_CARD_SIZE, FEED_CARD_SIZE, COLOR_TEXT_DIM);

        // Food sprite or placeholder
        drawSprite(cx + 2, FEED_CARD_Y + 2, SPRITE_FOOD[id], nullptr);

        // Food name below card
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.setTextDatum(TC_DATUM);
        // Truncate name to fit
        char shortName[6];
        strncpy(shortName, FOOD_TABLE[id].name, 5);
        shortName[5] = '\0';
        tft.drawString(shortName, cx + FEED_CARD_SIZE / 2, FEED_CARD_Y + FEED_CARD_SIZE + 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Animating indicator
    uint8_t frame = model.animFrameIndex % 4;
    const char* dots[] = {".", "..", "...", "...."};
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(dots[frame], SCREEN_WIDTH / 2, 90);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawFeedPick(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Pick 3 of 4", SCREEN_WIDTH / 2, 4);
    tft.setTextDatum(TL_DATUM);

    int16_t startX = (SCREEN_WIDTH - (FEED_CARD_SIZE * 4 + FEED_CARD_GAP * 3)) / 2;
    for (uint8_t i = 0; i < FEED_DRAW_COUNT; i++) {
        uint8_t id = model.feedDraw.food_ids[i];
        int16_t cx = startX + i * (FEED_CARD_SIZE + FEED_CARD_GAP);

        // Cursor indicator
        if (i == model.feedCursor) {
            tft.fillTriangle(cx + FEED_CARD_SIZE / 2, FEED_CURSOR_Y,
                             cx + FEED_CARD_SIZE / 2 - 4, FEED_CURSOR_Y - 6,
                             cx + FEED_CARD_SIZE / 2 + 4, FEED_CURSOR_Y - 6,
                             COLOR_CURSOR);
        }

        // Card border color based on selection
        uint16_t borderColor = COLOR_TEXT_DIM;
        if (model.feedSelected[i]) borderColor = COLOR_SELECTED;
        if (i == model.feedCursor) borderColor = COLOR_CURSOR;

        tft.drawRect(cx, FEED_CARD_Y, FEED_CARD_SIZE, FEED_CARD_SIZE, borderColor);
        if (model.feedSelected[i]) {
            tft.drawRect(cx + 1, FEED_CARD_Y + 1, FEED_CARD_SIZE - 2, FEED_CARD_SIZE - 2, borderColor);
        }

        // Food sprite or placeholder
        drawSprite(cx + 2, FEED_CARD_Y + 2, SPRITE_FOOD[id], nullptr);

        // Food name
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.setTextDatum(TC_DATUM);
        char shortName[6];
        strncpy(shortName, FOOD_TABLE[id].name, 5);
        shortName[5] = '\0';
        tft.drawString(shortName, cx + FEED_CARD_SIZE / 2, FEED_CARD_Y + FEED_CARD_SIZE + 2);
        tft.setTextDatum(TL_DATUM);

        // Selected mark
        if (model.feedSelected[i]) {
            tft.setTextColor(COLOR_SELECTED, COLOR_BG);
            tft.setTextDatum(TC_DATUM);
            tft.drawString("*", cx + FEED_CARD_SIZE / 2, FEED_CARD_Y + FEED_CARD_SIZE + 12);
            tft.setTextDatum(TL_DATUM);
        }
    }

    // Hints
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("L/R:Move M:Pick (3=Go)", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 2);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawFeedResult(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("~ FEED RESULT ~", SCREEN_WIDTH / 2, 4);
    tft.setTextDatum(TL_DATUM);

    int16_t y = 24;

    // HP change
    int16_t hpDelta = model.feedOutcome.health_after - model.feedOutcome.health_before;
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(8, y);
    tft.printf("HP: %d -> %d", model.feedOutcome.health_before, model.feedOutcome.health_after);
    y += 14;

    tft.setCursor(8, y);
    tft.setTextColor(hpDelta >= 0 ? COLOR_HP : COLOR_HP_CRIT, COLOR_BG);
    tft.printf("  (%s%d)", hpDelta >= 0 ? "+" : "", hpDelta);
    y += 16;

    // SR after
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(8, y);
    tft.printf("SR: %d", model.feedSrAfter);
    y += 16;

    // Combo
    if (model.feedOutcome.combo_triggered) {
        tft.setTextColor(COLOR_COMBO, COLOR_BG);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("** COMBO **", SCREEN_WIDTH / 2, y);
        y += 12;
        tft.drawString(COMBO_NAMES[model.feedOutcome.combo], SCREEN_WIDTH / 2, y);
        tft.setTextDatum(TL_DATUM);
    }
}

void DisplayRenderer::drawSpecialFood(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    tft.setTextColor(COLOR_EVOLUTION, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Special Food", SCREEN_WIDTH / 2, 2);
    tft.setTextDatum(TL_DATUM);

    for (uint8_t i = 0; i < model.specialFoodCount && i < SFOOD_COUNT; i++) {
        int16_t iy = SFOOD_START_Y + i * SFOOD_ITEM_H;
        bool isCursor = (i == model.specialFoodCursor);

        if (isCursor) {
            tft.fillRect(0, iy, SCREEN_WIDTH, SFOOD_ITEM_H, 0x1082);
            tft.setTextColor(COLOR_CURSOR, 0x1082);
            tft.setCursor(4, iy + 4);
            tft.print(">");
        } else {
            tft.setTextColor(COLOR_TEXT, COLOR_BG);
        }

        tft.setCursor(SFOOD_LABEL_X, iy + 4);
        tft.print(SPECIAL_FOOD_TABLE[i].name);

        // Description on second line
        tft.setTextColor(COLOR_TEXT_DIM, isCursor ? (uint16_t)0x1082 : COLOR_BG);
        tft.setCursor(SFOOD_LABEL_X, iy + 12);
        tft.print(SPECIAL_FOOD_TABLE[i].description);
    }

    // Mapo tofu easter egg
    if (model.mapoTriggered) {
        tft.setTextColor(COLOR_DESTROY, COLOR_BG);
        tft.setTextDatum(BC_DATUM);
        char buf[32];
        snprintf(buf, sizeof(buf), "MAPO TOFU! (%d/%d)", model.mapoCount, MAPO_TOFU_CURSE_THRESHOLD);
        tft.drawString(buf, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 4);
        tft.setTextDatum(TL_DATUM);
    }

    // Hints
    if (!model.mapoTriggered) {
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
        tft.setTextDatum(BC_DATUM);
        tft.drawString("L/R:Move M:Select", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 2);
        tft.setTextDatum(TL_DATUM);
    }
}

void DisplayRenderer::drawPoke(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    // Poke animation: flashing circle or sprite frames
    uint8_t frame = model.animFrameIndex;
    bool flash = (frame % 2 == 0);

    // Draw poke animation frames if available
    if (hasAnimFrames(ANIM_POKE_FRAMES)) {
        uint8_t fi = frame % ANIM_POKE_FRAMES.frameCount;
        drawSprite(32, 16, ANIM_POKE_FRAMES.frames[fi], "Poke");
    } else {
        // Placeholder: flashing circle
        uint16_t color = flash ? COLOR_CURSOR : COLOR_TEXT_DIM;
        tft.fillCircle(SCREEN_WIDTH / 2, 50, 20 + (frame % 4) * 3, color);
        tft.setTextColor(COLOR_TEXT, color);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Poke!", SCREEN_WIDTH / 2, 50);
        tft.setTextDatum(TL_DATUM);
    }

    // Result text
    int16_t y = 85;
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextDatum(TC_DATUM);
    if (model.pokeValueChanged) {
        char buf[32];
        snprintf(buf, sizeof(buf), "SR: %d -> %d", model.pokeSrBefore, model.pokeSrAfter);
        tft.drawString(buf, SCREEN_WIDTH / 2, y);
    } else {
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
        tft.drawString("(cooldown)", SCREEN_WIDTH / 2, y);
    }
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawEvolution(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    // Determine animation type
    AnimState anim = (AnimState)model.animState;
    uint8_t frame = model.animFrameIndex;
    bool flash = (frame % 2 == 0);

    // Title
    tft.setTextColor(COLOR_EVOLUTION, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);

    if (anim == ANIM_RHONGOMYNIAD || model.evolution.event == EVO_RHONGOMYNIAD) {
        tft.drawString("RHONGOMYNIAD", SCREEN_WIDTH / 2, 4);
        // Use rhongo frames if available
        if (hasAnimFrames(ANIM_RHONGO_FRAMES)) {
            uint8_t fi = frame % ANIM_RHONGO_FRAMES.frameCount;
            drawSprite(32, 20, ANIM_RHONGO_FRAMES.frames[fi], "Rhongo");
        } else {
            // Placeholder: golden expanding rect
            uint16_t sz = 20 + (frame % 8) * 4;
            int16_t cx = SCREEN_WIDTH / 2 - sz / 2;
            int16_t cy = 40;
            tft.drawRect(cx, cy, sz, sz, flash ? COLOR_EVOLUTION : COLOR_WARN);
            tft.setTextColor(COLOR_TEXT, COLOR_BG);
            tft.drawString("Rhongomyniad", SCREEN_WIDTH / 2, 80);
        }
    } else if (anim == ANIM_BLACK_RHONGOMYNIAD || model.evolution.event == EVO_BLACK_RHONGOMYNIAD) {
        tft.setTextColor(COLOR_DESTROY, COLOR_BG);
        tft.drawString("BLACK RHONGO", SCREEN_WIDTH / 2, 4);
        // Use rhongo frames (shared) if available
        if (hasAnimFrames(ANIM_RHONGO_FRAMES)) {
            uint8_t fi = frame % ANIM_RHONGO_FRAMES.frameCount;
            drawSprite(32, 20, ANIM_RHONGO_FRAMES.frames[fi], "B.Rhongo");
        } else {
            uint16_t sz = 20 + (frame % 8) * 4;
            int16_t cx = SCREEN_WIDTH / 2 - sz / 2;
            tft.drawRect(cx, 40, sz, sz, flash ? COLOR_DESTROY : COLOR_SR_HIGH);
            tft.setTextColor(COLOR_DESTROY, COLOR_BG);
            tft.drawString("Mapo Curse!", SCREEN_WIDTH / 2, 80);
        }
    } else if (anim == ANIM_DESTROY) {
        tft.setTextColor(COLOR_DESTROY, COLOR_BG);
        tft.drawString("DESTROYED", SCREEN_WIDTH / 2, 4);
        if (hasAnimFrames(ANIM_DESTROY_FRAMES)) {
            uint8_t fi = frame % ANIM_DESTROY_FRAMES.frameCount;
            drawSprite(32, 20, ANIM_DESTROY_FRAMES.frames[fi], "Destroy");
        } else {
            // Placeholder: shrinking rect
            uint16_t sz = 60 - (frame % 15) * 4;
            if (sz < 4) sz = 4;
            int16_t cx = SCREEN_WIDTH / 2 - sz / 2;
            tft.drawRect(cx, 30, sz, sz, flash ? COLOR_DESTROY : COLOR_BG);
            tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
            tft.drawString(FORM_NAMES[model.destroyedForm], SCREEN_WIDTH / 2, 80);
        }
    } else {
        // Normal evolution
        tft.drawString("EVOLUTION!", SCREEN_WIDTH / 2, 4);
        if (hasAnimFrames(ANIM_EVOLUTION_FRAMES)) {
            uint8_t fi = frame % ANIM_EVOLUTION_FRAMES.frameCount;
            drawSprite(32, 20, ANIM_EVOLUTION_FRAMES.frames[fi], "Evo");
        } else {
            // Placeholder: flashing star pattern
            uint16_t color = flash ? COLOR_EVOLUTION : COLOR_TEXT_DIM;
            tft.drawCircle(SCREEN_WIDTH / 2, 50, 15 + (frame % 6) * 3, color);
            tft.drawCircle(SCREEN_WIDTH / 2, 50, 10 + (frame % 4) * 2, color);
        }
    }

    // Form transition text
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString(FORM_NAMES[model.evolution.form_before], SCREEN_WIDTH / 2, 96);
    tft.setTextColor(COLOR_CURSOR, COLOR_BG);
    tft.drawString("->", SCREEN_WIDTH / 2, 106);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString(FORM_NAMES[model.evolution.form_after], SCREEN_WIDTH / 2, 116);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawDestroyConfirm(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    // Warning header
    tft.setTextColor(COLOR_DESTROY, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("!! DESTROY !!", SCREEN_WIDTH / 2, 10);

    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString("Reset pet?", SCREEN_WIDTH / 2, 30);
    tft.drawString("This cannot be undone.", SCREEN_WIDTH / 2, 44);

    // YES button
    int16_t yesX = SCREEN_WIDTH / 2 - DESTROY_BTN_W - 8;
    int16_t noX = SCREEN_WIDTH / 2 + 8;

    uint16_t yesBorder = (model.destroyCursor == 0) ? COLOR_CURSOR : COLOR_TEXT_DIM;
    uint16_t noBorder = (model.destroyCursor == 1) ? COLOR_CURSOR : COLOR_TEXT_DIM;

    tft.drawRect(yesX, DESTROY_BTN_Y, DESTROY_BTN_W, DESTROY_BTN_H, yesBorder);
    tft.drawRect(noX, DESTROY_BTN_Y, DESTROY_BTN_W, DESTROY_BTN_H, noBorder);

    if (model.destroyCursor == 0) {
        tft.drawRect(yesX + 1, DESTROY_BTN_Y + 1, DESTROY_BTN_W - 2, DESTROY_BTN_H - 2, yesBorder);
    } else {
        tft.drawRect(noX + 1, DESTROY_BTN_Y + 1, DESTROY_BTN_W - 2, DESTROY_BTN_H - 2, noBorder);
    }

    tft.setTextColor(COLOR_DESTROY, COLOR_BG);
    tft.drawString("YES", yesX + DESTROY_BTN_W / 2, DESTROY_BTN_Y + 8);

    tft.setTextColor(COLOR_HP, COLOR_BG);
    tft.drawString("NO", noX + DESTROY_BTN_W / 2, DESTROY_BTN_Y + 8);

    // Hint
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("L/R:Move M:Confirm", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 10);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawDayEnd(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);

    tft.setTextColor(COLOR_EVOLUTION, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("~ DAY END ~", SCREEN_WIDTH / 2, 4);
    tft.setTextDatum(TL_DATUM);

    int16_t y = 20;

    if (model.dayEnd.terminalState) {
        tft.setTextColor(COLOR_WARN, COLOR_BG);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Terminal state", SCREEN_WIDTH / 2, 50);
        tft.drawString("reached.", SCREEN_WIDTH / 2, 64);
        tft.setTextDatum(TL_DATUM);
        return;
    }

    // Idle SR change
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(4, y);
    tft.printf("Idle SR: %d->%d", model.dayEnd.idleSrBefore, model.dayEnd.idleSrAfter);
    y += 14;

    // Window bonus
    if (model.dayEnd.windowBonusApplied) {
        tft.setTextColor(COLOR_HP, COLOR_BG);
        tft.setCursor(4, y);
        tft.printf("Window +%dHP", model.dayEnd.windowBonusHP);
        y += 12;
    }

    // Window penalty
    if (model.dayEnd.windowPenaltyApplied) {
        tft.setTextColor(COLOR_WARN, COLOR_BG);
        tft.setCursor(4, y);
        tft.printf("Window -%dHP", model.dayEnd.windowPenaltyHP);
        y += 12;
    }

    // Missed feed
    if (model.dayEnd.missedFeedPenalty) {
        tft.setTextColor(COLOR_DESTROY, COLOR_BG);
        tft.setCursor(4, y);
        tft.printf("Missed: %d/%d feeds", model.dayEnd.fedCount, model.dayEnd.feedLimit);
        y += 12;
    }

    // Day complete
    if (model.dayEnd.dayNumber > 0) {
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.setCursor(4, y);
        tft.printf("Day %d complete!", model.dayEnd.dayNumber);
    }
}

void DisplayRenderer::drawToast(const DisplayModel& model) {
    if (model.toast[0] == '\0') return;

    // Draw toast bar at bottom
    tft.fillRect(0, TOAST_Y, SCREEN_WIDTH, TOAST_HEIGHT, COLOR_TOAST_BG);
    tft.setTextColor(COLOR_TOAST_TEXT, COLOR_TOAST_BG);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(model.toast, SCREEN_WIDTH / 2, TOAST_Y + TOAST_HEIGHT / 2);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawSprite(int16_t x, int16_t y, const SpriteAsset& sprite, const char* label) {
    if (hasSprite(sprite)) {
        // Draw RGB565 bitmap
        tft.pushImage(x, y, sprite.width, sprite.height, sprite.data);
    } else {
        // Placeholder: colored rectangle with label
        uint16_t w = (sprite.width > 0) ? sprite.width : 24;
        uint16_t h = (sprite.height > 0) ? sprite.height : 24;
        tft.drawRect(x, y, w, h, COLOR_PLACEHOLDER);
        tft.drawLine(x, y, x + w - 1, y + h - 1, COLOR_PLACEHOLDER);
        tft.drawLine(x + w - 1, y, x, y + h - 1, COLOR_PLACEHOLDER);
        if (label) {
            tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
            tft.setTextDatum(TC_DATUM);
            tft.drawString(label, x + w / 2, y + h + 1);
            tft.setTextDatum(TL_DATUM);
        }
    }
}

void DisplayRenderer::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                                       int16_t value, int16_t maxValue, uint16_t color) {
    // Background
    tft.fillRect(x, y, w, h, COLOR_BAR_BG);
    // Filled portion
    if (maxValue > 0 && value > 0) {
        int16_t fillW = (int32_t)value * w / maxValue;
        if (fillW > w) fillW = w;
        if (fillW > 0) {
            tft.fillRect(x, y, fillW, h, color);
        }
    }
    // Border
    tft.drawRect(x, y, w, h, COLOR_TEXT_DIM);
}

void DisplayRenderer::drawTextCentered(int16_t y, const char* text) {
    tft.setTextDatum(TC_DATUM);
    tft.drawString(text, SCREEN_WIDTH / 2, y);
    tft.setTextDatum(TL_DATUM);
}

void DisplayRenderer::drawWaitTimeSet(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);

    tft.setTextColor(COLOR_WARN, COLOR_BG);
    tft.drawString("SET TIME", SCREEN_WIDTH / 2, 30);

    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString("Send via serial:", SCREEN_WIDTH / 2, 55);
    tft.drawString("SET_TIME <epoch>", SCREEN_WIDTH / 2, 70);

    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("Waiting...", SCREEN_WIDTH / 2, 100);

    tft.setTextDatum(TL_DATUM);
}

#endif // DISPLAY_BACKEND_TFT_ESPI
