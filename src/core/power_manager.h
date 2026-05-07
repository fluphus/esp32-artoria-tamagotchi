// src/core/power_manager.h
// 电源管理器 - 屏幕亮度控制、待机降亮度、熄屏、深度睡眠

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>

// ============================================================================
//  默认配置 (可通过串口命令修改, 保存到 NVS)
// ============================================================================

#define DEFAULT_BRIGHTNESS          15      // 默认亮度 (0-15, SSD1351 master contrast)
#define DEFAULT_DIM_BRIGHTNESS      4       // 待机降低亮度值
#define DEFAULT_DIM_TIMEOUT_SEC     60      // 无操作后降低亮度的时间 (秒)
#define DEFAULT_OFF_TIMEOUT_SEC     120     // 降低亮度后熄屏的时间 (秒)

// ============================================================================
//  电源状态
// ============================================================================

enum PowerState : uint8_t {
    POWER_ACTIVE = 0,       // 正常活跃
    POWER_DIM,              // 已降低亮度
    POWER_OFF               // 已熄屏 (即将进入 deep sleep)
};

// ============================================================================
//  PowerManager 类
// ============================================================================

class PowerManager {
public:
    void init();

    // 每帧调用, 检查超时并切换状态
    void update(uint32_t nowMs);

    // 用户活动通知 (按键、串口命令等)
    void onUserActivity();

    // 亮度控制
    void setBrightness(uint8_t level);      // 0-15
    uint8_t getBrightness() const { return _brightness; }

    // 配置设置
    void setDimBrightness(uint8_t level);   // 0-15
    void setDimTimeout(uint32_t seconds);
    void setOffTimeout(uint32_t seconds);

    uint8_t getDimBrightness() const { return _dimBrightness; }
    uint32_t getDimTimeout() const { return _dimTimeoutSec; }
    uint32_t getOffTimeout() const { return _offTimeoutSec; }

    // 当前电源状态
    PowerState getState() const { return _state; }

    // 是否需要进入 deep sleep (由 main loop 检查)
    bool shouldEnterDeepSleep() const { return _shouldSleep; }
    void clearSleepFlag() { _shouldSleep = false; }

    // 从 deep sleep 唤醒后调用
    void onWakeFromSleep();

    // 保存/加载配置到 NVS
    void saveConfig();
    void loadConfig();

    // 屏幕开关
    void screenOn();
    void screenOff();

private:
    PowerState _state = POWER_ACTIVE;
    uint8_t _brightness = DEFAULT_BRIGHTNESS;
    uint8_t _dimBrightness = DEFAULT_DIM_BRIGHTNESS;
    uint32_t _dimTimeoutSec = DEFAULT_DIM_TIMEOUT_SEC;
    uint32_t _offTimeoutSec = DEFAULT_OFF_TIMEOUT_SEC;

    uint32_t _lastActivityMs = 0;       // 上次用户活动时间 (millis)
    uint32_t _dimEnteredMs = 0;         // 进入 dim 状态的时间
    bool _shouldSleep = false;
    bool _screenIsOff = false;

    // 硬件控制
    void applyBrightness(uint8_t level);
    void displaySleep();
    void displayWake();
};

extern PowerManager powerManager;

#endif // POWER_MANAGER_H
