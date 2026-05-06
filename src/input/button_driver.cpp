// src/input/button_driver.cpp
// 物理按键驱动实现

#include "button_driver.h"

ButtonDriver buttonDriver;

void ButtonDriver::init() {
    for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
        uint8_t pin = BTN_PIN_MAP[i];
        if (BTN_ACTIVE_LOW) {
            pinMode(pin, INPUT_PULLUP);
        } else {
            pinMode(pin, INPUT_PULLDOWN);
        }
        _buttons[i].raw = false;
        _buttons[i].stable = false;
        _buttons[i].last_stable = false;
        _buttons[i].last_change_ms = 0;
        _buttons[i].press_start_ms = 0;
        _buttons[i].long_press_fired = false;
        _buttons[i].last_repeat_ms = 0;
        _buttons[i].repeat_started = false;
    }
    _combo.all_pressed = false;
    _combo.combo_start_ms = 0;
    _combo.combo_fired = false;
    _event_count = 0;
}

uint8_t ButtonDriver::update() {
    _event_count = 0;
    uint32_t now = millis();

    readGPIO();
    debounce(now);
    detectEvents(now);
    detectCombo(now);

    return _event_count;
}

void ButtonDriver::readGPIO() {
    for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
        int level = digitalRead(BTN_PIN_MAP[i]);
        if (BTN_ACTIVE_LOW) {
            _buttons[i].raw = (level == LOW);
        } else {
            _buttons[i].raw = (level == HIGH);
        }
    }
}

void ButtonDriver::debounce(uint32_t now) {
    for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
        ButtonState& btn = _buttons[i];
        if (btn.raw != btn.stable) {
            if (now - btn.last_change_ms >= BTN_DEBOUNCE_MS) {
                btn.last_stable = btn.stable;
                btn.stable = btn.raw;
                btn.last_change_ms = now;
            }
        } else {
            btn.last_change_ms = now;
        }
    }
}

void ButtonDriver::detectEvents(uint32_t now) {
    for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
        ButtonState& btn = _buttons[i];
        ButtonId id = (ButtonId)i;

        // 按下边沿
        if (btn.stable && !btn.last_stable) {
            btn.press_start_ms = now;
            btn.long_press_fired = false;
            btn.repeat_started = false;
            pushEvent(id, BTN_EVENT_PRESS, now);
            btn.last_stable = btn.stable;
        }
        // 释放边沿
        else if (!btn.stable && btn.last_stable) {
            uint16_t duration = (uint16_t)(now - btn.press_start_ms);
            pushEvent(id, BTN_EVENT_RELEASE, now, duration);
            btn.last_stable = btn.stable;
        }
        // 持续按住
        else if (btn.stable) {
            uint32_t held = now - btn.press_start_ms;

            // 长按检测
            if (!btn.long_press_fired && held >= BTN_LONG_PRESS_MS) {
                btn.long_press_fired = true;
                pushEvent(id, BTN_EVENT_LONG_PRESS, now, (uint16_t)held);
            }

            // 连按检测
            if (btn.long_press_fired) {
                if (!btn.repeat_started) {
                    if (held >= BTN_LONG_PRESS_MS + BTN_REPEAT_DELAY_MS) {
                        btn.repeat_started = true;
                        btn.last_repeat_ms = now;
                        pushEvent(id, BTN_EVENT_REPEAT, now, (uint16_t)held);
                    }
                } else {
                    if (now - btn.last_repeat_ms >= BTN_REPEAT_INTERVAL_MS) {
                        btn.last_repeat_ms = now;
                        pushEvent(id, BTN_EVENT_REPEAT, now, (uint16_t)held);
                    }
                }
            }
        }
    }
}

void ButtonDriver::detectCombo(uint32_t now) {
    bool all = true;
    for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
        if (!_buttons[i].stable) {
            all = false;
            break;
        }
    }

    if (all) {
        if (!_combo.all_pressed) {
            // 三键刚同时按下
            _combo.all_pressed = true;
            _combo.combo_start_ms = now;
            _combo.combo_fired = false;
        } else if (!_combo.combo_fired) {
            // 检查是否达到5秒
            if (now - _combo.combo_start_ms >= BTN_DESTROY_HOLD_MS) {
                _combo.combo_fired = true;
                // 不产出普通按键事件, 由上层通过 isComboTriggered() 检测
            }
        }
    } else {
        if (_combo.all_pressed) {
            _combo.all_pressed = false;
            // 如果未达到触发时间就松开, 重置
            if (!_combo.combo_fired) {
                _combo.combo_start_ms = 0;
            }
        }
    }
}

void ButtonDriver::pushEvent(ButtonId btn, ButtonEventType type, uint32_t timestamp, uint16_t duration) {
    if (_event_count >= MAX_EVENTS_PER_FRAME) return;
    _events[_event_count].button = btn;
    _events[_event_count].type = type;
    _events[_event_count].timestamp = timestamp;
    _events[_event_count].duration = duration;
    _event_count++;
}

bool ButtonDriver::isPressed(ButtonId btn) const {
    if (btn >= BTN_ID_COUNT) return false;
    return _buttons[btn].stable;
}

uint32_t ButtonDriver::comboHoldDuration() const {
    if (!_combo.all_pressed) return 0;
    return millis() - _combo.combo_start_ms;
}

void ButtonDriver::resetCombo() {
    _combo.all_pressed = false;
    _combo.combo_start_ms = 0;
    _combo.combo_fired = false;
}

void ButtonDriver::injectEvent(ButtonId btn, ButtonEventType type, uint16_t duration) {
    _event_count = 0;  // 清空之前的事件
    pushEvent(btn, type, millis(), duration);
}
