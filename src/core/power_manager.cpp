// src/core/power_manager.cpp
// 电源管理器实现 - SSD1351 亮度控制 + deep sleep

#include "power_manager.h"
#include "../display/display_config.h"
#include <Arduino.h>
#include <Preferences.h>

#if DISPLAY_BACKEND_TFT_ESPI
#include <TFT_eSPI.h>
extern TFT_eSPI tft;  // 在 display_renderer.cpp 中定义
#endif

PowerManager powerManager;

static Preferences powerPrefs;

// SSD1351 命令定义
#define SSD1351_CMD_CONTRASTMASTER  0xC7    // Master contrast (0x00-0x0F)
#define SSD1351_CMD_DISPLAYOFF      0xAE    // Display OFF (sleep mode)
#define SSD1351_CMD_DISPLAYON       0xAF    // Display ON (normal mode)

void PowerManager::init() {
    _lastActivityMs = millis();
    _state = POWER_ACTIVE;
    _shouldSleep = false;
    _screenIsOff = false;
    loadConfig();
    applyBrightness(_brightness);
    Serial.printf("[Power] Init: brightness=%d, dim=%d, dimTimeout=%lus, offTimeout=%lus\n",
                  _brightness, _dimBrightness, _dimTimeoutSec, _offTimeoutSec);
}

void PowerManager::update(uint32_t nowMs) {
    if (_shouldSleep) return;  // 已标记要睡眠, 不再更新

    uint32_t idleMs = nowMs - _lastActivityMs;

    switch (_state) {
        case POWER_ACTIVE:
            if (_dimTimeoutSec > 0 && idleMs >= (_dimTimeoutSec * 1000UL)) {
                // 进入 dim 状态
                _state = POWER_DIM;
                _dimEnteredMs = nowMs;
                applyBrightness(_dimBrightness);
                Serial.println("[Power] -> DIM");
            }
            break;

        case POWER_DIM:
            if (_offTimeoutSec > 0) {
                uint32_t dimElapsed = nowMs - _dimEnteredMs;
                if (dimElapsed >= (_offTimeoutSec * 1000UL)) {
                    // 进入熄屏状态
                    _state = POWER_OFF;
                    screenOff();
                    _shouldSleep = true;
                    Serial.println("[Power] -> OFF (deep sleep pending)");
                }
            }
            break;

        case POWER_OFF:
            // 等待 main loop 处理 deep sleep
            break;
    }
}

void PowerManager::onUserActivity() {
    uint32_t nowMs = millis();
    _lastActivityMs = nowMs;

    if (_state != POWER_ACTIVE) {
        PowerState prevState = _state;
        _state = POWER_ACTIVE;

        if (_screenIsOff) {
            screenOn();
        }
        applyBrightness(_brightness);

        if (prevState == POWER_DIM)
            Serial.println("[Power] DIM -> ACTIVE (user activity)");
        else if (prevState == POWER_OFF)
            Serial.println("[Power] OFF -> ACTIVE (wake)");
    }
}

void PowerManager::setBrightness(uint8_t level) {
    if (level > 15) level = 15;
    _brightness = level;
    if (_state == POWER_ACTIVE) {
        applyBrightness(_brightness);
    }
    Serial.printf("[Power] Brightness set to %d\n", _brightness);
}

void PowerManager::setDimBrightness(uint8_t level) {
    if (level > 15) level = 15;
    _dimBrightness = level;
    Serial.printf("[Power] Dim brightness set to %d\n", _dimBrightness);
}

void PowerManager::setDimTimeout(uint32_t seconds) {
    _dimTimeoutSec = seconds;
    Serial.printf("[Power] Dim timeout set to %lu seconds\n", _dimTimeoutSec);
}

void PowerManager::setOffTimeout(uint32_t seconds) {
    _offTimeoutSec = seconds;
    Serial.printf("[Power] Off timeout set to %lu seconds\n", _offTimeoutSec);
}

void PowerManager::onWakeFromSleep() {
    _screenIsOff = false;
    _state = POWER_ACTIVE;
    _lastActivityMs = millis();
    _shouldSleep = false;
    displayWake();
    applyBrightness(_brightness);
    Serial.println("[Power] Woke from deep sleep");
}

void PowerManager::saveConfig() {
    powerPrefs.begin("pwr_cfg", false);
    powerPrefs.putUChar("brightness", _brightness);
    powerPrefs.putUChar("dim_bright", _dimBrightness);
    powerPrefs.putULong("dim_timeout", _dimTimeoutSec);
    powerPrefs.putULong("off_timeout", _offTimeoutSec);
    powerPrefs.end();
    Serial.println("[Power] Config saved to NVS");
}

void PowerManager::loadConfig() {
    powerPrefs.begin("pwr_cfg", true);  // read-only
    _brightness = powerPrefs.getUChar("brightness", DEFAULT_BRIGHTNESS);
    _dimBrightness = powerPrefs.getUChar("dim_bright", DEFAULT_DIM_BRIGHTNESS);
    _dimTimeoutSec = powerPrefs.getULong("dim_timeout", DEFAULT_DIM_TIMEOUT_SEC);
    _offTimeoutSec = powerPrefs.getULong("off_timeout", DEFAULT_OFF_TIMEOUT_SEC);
    powerPrefs.end();
}

void PowerManager::screenOn() {
    if (!_screenIsOff) return;
    _screenIsOff = false;
    displayWake();
}

void PowerManager::screenOff() {
    if (_screenIsOff) return;
    _screenIsOff = true;
    displaySleep();
}

void PowerManager::applyBrightness(uint8_t level) {
#if DISPLAY_BACKEND_TFT_ESPI
    // SSD1351 master contrast: 通过 TFT_eSPI 的 writecommand/writedata
    if (level > 15) level = 15;
    tft.writecommand(SSD1351_CMD_CONTRASTMASTER);
    tft.writedata(level);
#endif
    // Serial placeholder: 仅打印
#if DISPLAY_BACKEND_SERIAL_PLACEHOLDER
    Serial.printf("[Power] Apply brightness: %d\n", level);
#endif
}

void PowerManager::displaySleep() {
#if DISPLAY_BACKEND_TFT_ESPI
    tft.writecommand(SSD1351_CMD_DISPLAYOFF);
#endif
#if DISPLAY_BACKEND_SERIAL_PLACEHOLDER
    Serial.println("[Power] Display OFF");
#endif
}

void PowerManager::displayWake() {
#if DISPLAY_BACKEND_TFT_ESPI
    tft.writecommand(SSD1351_CMD_DISPLAYON);
#endif
#if DISPLAY_BACKEND_SERIAL_PLACEHOLDER
    Serial.println("[Power] Display ON");
#endif
}
