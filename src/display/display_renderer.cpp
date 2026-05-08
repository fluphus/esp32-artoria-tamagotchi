// src/display/display_renderer.cpp
// 显示渲染器实现
// 支持多后端: Serial 占位 / TFT_eSPI (SSD1351 128x128 65K OLED)

#include "display_renderer.h"
#include "../config/food_table.h"
#include "../config/game_config.h"
#include "../pet/gallery.h"
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
    if (p.is_nobu) {
        Serial.println("[Display]   Name: nobu");
        Serial.println("[Display]   HP: ?  SR: ?");
        Serial.println("[Display]   Age: ?  Stage: ?");
        Serial.println("[Display]   Align: ?");
    } else {
        Serial.printf("[Display]   Form: %s\n", FORM_NAMES[p.form]);
        Serial.printf("[Display]   HP: %d  SR: %d\n", p.health, p.seriousness);
        Serial.printf("[Display]   Age: Day %d  Stage: %s\n", p.age_days + 1,
            p.stage == STAGE_CHILD ? "Child" : "Adult");
        Serial.printf("[Display]   Rounds: %d\n", p.rounds);
        Serial.printf("[Display]   Align: %s\n", ALIGNMENT_NAMES[p.alignment]);
    }
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
        if (model.petSnapshot.is_nobu) {
            Serial.printf("[Display]   !! [NOBU EVENT] (%d) !!\n", model.mapoCount);
        } else {
            Serial.printf("[Display]   !! MAPO TOFU (%d/%d) !!\n",
                model.mapoCount, MAPO_TOFU_CURSE_THRESHOLD);
        }
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

void DisplayRenderer::drawGallery(const DisplayModel& model) {
    Serial.println("[Display] --- GALLERY ---");
    uint8_t page = model.galleryPage;
    uint8_t selected = model.gallerySelectedIndex;
    uint8_t itemsOnPage = model.galleryItemsThisPage;
    uint8_t totalPages = (model.galleryTotalForms + GALLERY_ITEMS_PER_PAGE - 1) / GALLERY_ITEMS_PER_PAGE;

    Serial.printf("[Display]   Page %d/%d | Items on page: %d\n",
                  page + 1, totalPages, itemsOnPage);

    // 计算本页包含的形态
    uint8_t startIdx = page * GALLERY_ITEMS_PER_PAGE;
    Serial.print("[Display]   Forms: [");
    for (uint8_t i = 0; i < itemsOnPage; i++) {
        Form f = (Form)(startIdx + i);
        bool unlocked = gallerySystem.isFormUnlocked(f);
        if (i > 0) Serial.print(", ");
        Serial.printf("%s(%s)", FORM_NAMES[f], unlocked ? "Unlocked" : "Locked");
    }
    Serial.println("]");

    // 当前光标选中
    Form selectedForm = (Form)(startIdx + selected);
    bool selectedUnlocked = gallerySystem.isFormUnlocked(selectedForm);
    Serial.printf("[Display]   Cursor -> [%d] %s (%s)\n",
                  selected, FORM_NAMES[selectedForm],
                  selectedUnlocked ? "Unlocked" : "Locked");
    Serial.printf("[Display]   Total unlocked: %d/%d\n",
                  gallerySystem.getData().getUnlockedCount(), model.galleryTotalForms);
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

void DisplayRenderer::sendCommand(uint8_t cmd) {
    (void)cmd;
}

void DisplayRenderer::sendCommandWithData(uint8_t cmd, uint8_t data) {
    (void)cmd;
    (void)data;
}

void DisplayRenderer::present() {}

#endif // DISPLAY_BACKEND_SERIAL_PLACEHOLDER

// ============================================================================
//  TFT_eSPI Backend (SSD1351 128x128 65K Color OLED)
// ============================================================================

#if DISPLAY_BACKEND_TFT_ESPI

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <stdarg.h>
#include "DisplayManager.h"

enum {
    TL_DATUM = 0,
    TC_DATUM = 1,
    TR_DATUM = 2,
    BC_DATUM = 3,
    MC_DATUM = 4
};

class TFTCompat {
public:
    TFTCompat()
        : _disp(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN),
          _canvas(SCREEN_WIDTH, SCREEN_HEIGHT) {}

    void init() { _disp.begin(); }
    void setRotation(uint8_t r) { _disp.setRotation(r); }
    void fillScreen(uint16_t c) {
        _canvas.fillScreen(c);
        _dirty = true;
    }
    void setTextColor(uint16_t fg, uint16_t bg) {
        _fg = fg;
        _bg = bg;
        _canvas.setTextColor(fg, bg);
    }
    void setTextSize(uint8_t s) { _canvas.setTextSize(s); }
    void setTextDatum(uint8_t d) { _datum = d; }
    void setCursor(int16_t x, int16_t y) { _canvas.setCursor(x, y); }
    void print(const char* s) {
        _canvas.print(s);
        _dirty = true;
    }
    void print(int v) {
        _canvas.print(v);
        _dirty = true;
    }
    void printf(const char* fmt, ...) {
        char buf[128];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        _canvas.print(buf);
        _dirty = true;
    }
    void drawString(const char* s, int16_t x, int16_t y) {
        int16_t x1, y1;
        uint16_t w, h;
        _canvas.getTextBounds((char*)s, 0, 0, &x1, &y1, &w, &h);
        int16_t tx = x;
        int16_t ty = y;
        if (_datum == TC_DATUM || _datum == BC_DATUM || _datum == MC_DATUM) tx = x - (int16_t)w / 2;
        if (_datum == TR_DATUM) tx = x - (int16_t)w;
        if (_datum == BC_DATUM) ty = y - (int16_t)h;
        if (_datum == MC_DATUM) ty = y - (int16_t)h / 2;
        _canvas.setCursor(tx, ty);
        _canvas.setTextColor(_fg, _bg);
        _canvas.print(s);
        _dirty = true;
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
        _canvas.drawRect(x, y, w, h, c);
        _dirty = true;
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
        _canvas.fillRect(x, y, w, h, c);
        _dirty = true;
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) {
        _canvas.drawLine(x0, y0, x1, y1, c);
        _dirty = true;
    }
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t c) {
        _canvas.drawCircle(x, y, r, c);
        _dirty = true;
    }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t c) {
        _canvas.fillCircle(x, y, r, c);
        _dirty = true;
    }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t c) {
        _canvas.fillTriangle(x0, y0, x1, y1, x2, y2, c);
        _dirty = true;
    }
    void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* data) {
        if (w <= 0 || h <= 0 || data == nullptr) return;
#if DISPLAY_RGB565_SWAP_BYTES
        if (w > SCREEN_WIDTH) w = SCREEN_WIDTH;
        static uint16_t rowBuf[SCREEN_WIDTH];
        for (int16_t row = 0; row < h; ++row) {
            const uint16_t* src = data + row * w;
            for (int16_t col = 0; col < w; ++col) {
                uint16_t p = src[col];
                rowBuf[col] = (uint16_t)((p << 8) | (p >> 8));
            }
            _canvas.drawRGBBitmap(x, y + row, rowBuf, w, 1);
        }
#else
        _canvas.drawRGBBitmap(x, y, data, w, h);
#endif
        _dirty = true;
    }
    void sendCommand(uint8_t cmd) {
        _disp.sendCommand(cmd);
    }
    void sendCommandWithData(uint8_t cmd, uint8_t data) {
        _disp.sendCommand(cmd, &data, 1);
    }
    void present() {
        if (!_dirty) return;
        _disp.startWrite();
        _disp.setAddrWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        _disp.writePixels(_canvas.getBuffer(), (uint32_t)SCREEN_WIDTH * SCREEN_HEIGHT, true, false);
        _disp.endWrite();
        _dirty = false;
    }

private:
    Adafruit_SSD1351 _disp;
    GFXcanvas16 _canvas;
    uint8_t _datum = TL_DATUM;
    uint16_t _fg = COLOR_TEXT;
    uint16_t _bg = COLOR_BG;
    bool _dirty = true;
};

TFTCompat tft;

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
    // On ESP32-S3 with custom pin mapping, explicitly start SPI first.
    // This avoids rare null bus state inside TFT_eSPI::init()/writecommand().
    SPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);
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

    if (p.is_nobu) {
        // nobu 彩蛋: 仅显示姓名, 其余为 "?"
        tft.setCursor(STATUS_LABEL_X, y); tft.print("Name:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("nobu");
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.print("Stage:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("?");
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.print("Align:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("?");
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.print("HP:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("?");
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.print("SR:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("?");
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.print("Age:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("?");
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.print("Rounds:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("?");
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.print("Fed:");
        tft.setCursor(STATUS_VALUE_X, y); tft.print("?");
    } else {
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

        tft.setCursor(STATUS_LABEL_X, y); tft.printf("Rounds: %d", p.rounds);
        y += STATUS_LINE_H;

        tft.setCursor(STATUS_LABEL_X, y); tft.printf("Fed: %d/%d", p.daily_feed.feed_count, DAILY_FEED_LIMIT);
        y += STATUS_LINE_H;

        if (p.mapo_tofu_count > 0) {
            tft.setTextColor(COLOR_WARN, COLOR_BG);
            tft.setCursor(STATUS_LABEL_X, y); tft.printf("Mapo: %d/%d", p.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
        }
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

    // Mapo tofu easter egg / nobu event
    if (model.mapoTriggered) {
        tft.setTextDatum(BC_DATUM);
        char buf[32];
        if (model.petSnapshot.is_nobu) {
            // nobu 占位显示
            tft.setTextColor(COLOR_EVOLUTION, COLOR_BG);
            snprintf(buf, sizeof(buf), "* EVENT *");
        } else {
            tft.setTextColor(COLOR_DESTROY, COLOR_BG);
            snprintf(buf, sizeof(buf), "MAPO TOFU! (%d/%d)", model.mapoCount, MAPO_TOFU_CURSE_THRESHOLD);
        }
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

void DisplayRenderer::sendCommand(uint8_t cmd) {
    tft.sendCommand(cmd);
}

void DisplayRenderer::sendCommandWithData(uint8_t cmd, uint8_t data) {
    tft.sendCommandWithData(cmd, data);
}

void DisplayRenderer::present() {
    tft.present();
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

void DisplayRenderer::drawGallery(const DisplayModel& model) {
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);

    // 标题
    uint8_t totalPages = (model.galleryTotalForms + GALLERY_ITEMS_PER_PAGE - 1) / GALLERY_ITEMS_PER_PAGE;
    char titleBuf[32];
    snprintf(titleBuf, sizeof(titleBuf), "GALLERY %d/%d", model.galleryPage + 1, totalPages);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString(titleBuf, SCREEN_WIDTH / 2, 2);

    // 2x2 网格绘制占位
    uint8_t startIdx = model.galleryPage * GALLERY_ITEMS_PER_PAGE;
    const int16_t gridX = 8;
    const int16_t gridY = 16;
    const int16_t cellW = 56;
    const int16_t cellH = 52;
    const int16_t gapX = 8;
    const int16_t gapY = 6;

    for (uint8_t i = 0; i < model.galleryItemsThisPage; i++) {
        uint8_t row = i / 2;
        uint8_t col = i % 2;
        int16_t x = gridX + col * (cellW + gapX);
        int16_t y = gridY + row * (cellH + gapY);

        Form f = (Form)(startIdx + i);
        bool unlocked = gallerySystem.isFormUnlocked(f);

        // 高亮边框
        uint16_t borderColor = (i == model.gallerySelectedIndex) ? COLOR_HIGHLIGHT : COLOR_TEXT_DIM;
        tft.drawRect(x, y, cellW, cellH, borderColor);

        // 内容: 占位矩形 + 名称
        if (unlocked) {
            tft.fillRect(x + 2, y + 2, cellW - 4, cellH - 14, COLOR_TEXT_DIM);
            tft.setTextDatum(TC_DATUM);
            tft.setTextColor(COLOR_TEXT, COLOR_BG);
            tft.drawString(FORM_NAMES[f], x + cellW / 2, y + cellH - 10);
        } else {
            tft.fillRect(x + 2, y + 2, cellW - 4, cellH - 14, COLOR_BG);
            tft.setTextDatum(TC_DATUM);
            tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
            tft.drawString("???", x + cellW / 2, y + cellH - 10);
        }
    }

    // 底部: 解锁进度
    char progBuf[24];
    snprintf(progBuf, sizeof(progBuf), "%d/%d Unlocked",
             gallerySystem.getData().getUnlockedCount(), model.galleryTotalForms);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString(progBuf, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 10);

    tft.setTextDatum(TL_DATUM);
}

#endif // DISPLAY_BACKEND_TFT_ESPI
