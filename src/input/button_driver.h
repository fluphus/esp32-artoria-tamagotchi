// src/input/button_driver.h
// 物理按键驱动 - GPIO读取、消抖、长按检测、三键组合检测
// 不依赖任何游戏逻辑, 仅产出 ButtonEvent

#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <Arduino.h>
#include "input_map.h"

// ============================================================================
//  按键状态 (内部使用)
// ============================================================================

struct ButtonState {
    bool        raw;                    // 当前原始电平
    bool        stable;                 // 消抖后稳定状态 (true=按下)
    bool        last_stable;            // 上一帧稳定状态
    uint32_t    last_change_ms;         // 上次电平变化时间
    uint32_t    press_start_ms;         // 按下起始时间
    bool        long_press_fired;       // 本次按下是否已触发长按事件
    uint32_t    last_repeat_ms;         // 上次连按触发时间
    bool        repeat_started;         // 是否已进入连按模式
};

// ============================================================================
//  三键组合检测状态
// ============================================================================

struct ComboState {
    bool        all_pressed;            // 三键是否同时按下
    uint32_t    combo_start_ms;         // 三键同时按下的起始时间
    bool        combo_fired;            // 本次组合是否已触发
};

// ============================================================================
//  ButtonDriver 类
// ============================================================================

class ButtonDriver {
public:
    // 初始化GPIO引脚
    void init();

    // 每帧调用, 更新按键状态并产出事件
    // 返回本帧产出的事件数量 (0 = 无事件)
    uint8_t update();

    // 获取本帧产出的事件 (最多 BTN_COUNT + 1 个事件/帧)
    // 调用 update() 后有效, 下次 update() 前有效
    static const uint8_t MAX_EVENTS_PER_FRAME = 8;
    const ButtonEvent* getEvents() const { return _events; }
    uint8_t getEventCount() const { return _event_count; }

    // 查询按键当前是否按下
    bool isPressed(ButtonId btn) const;

    // 查询三键组合是否正在持续按下
    bool isComboHolding() const { return _combo.all_pressed && !_combo.combo_fired; }

    // 查询三键组合已持续时间 (ms)
    uint32_t comboHoldDuration() const;

    // 查询三键组合是否已触发 (5秒)
    bool isComboTriggered() const { return _combo.combo_fired; }

    // 重置组合状态 (处理完销毁事件后调用)
    void resetCombo();

    // 注入模拟按键事件 (调试用, 绕过GPIO读取)
    void injectEvent(ButtonId btn, ButtonEventType type, uint16_t duration = 0);

private:
    ButtonState _buttons[BTN_ID_COUNT];
    ComboState  _combo;
    ButtonEvent _events[MAX_EVENTS_PER_FRAME];
    uint8_t     _event_count;

    void readGPIO();
    void debounce(uint32_t now);
    void detectEvents(uint32_t now);
    void detectCombo(uint32_t now);
    void pushEvent(ButtonId btn, ButtonEventType type, uint32_t timestamp, uint16_t duration = 0);
};

// 全局单例
extern ButtonDriver buttonDriver;

#endif // BUTTON_DRIVER_H
