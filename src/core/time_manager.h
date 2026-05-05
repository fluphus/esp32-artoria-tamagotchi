// src/core/time_manager.h

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stdint.h>
#include <stddef.h>

// ============================================
// 时间管理器
// 模拟阶段: 基于 millis() 的虚拟时钟, 支持手动快进
// 实机阶段: 切换为 RTC + NTP
// ============================================

struct TimeInfo {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;    // 0=Sunday
};

class TimeManager {
public:
    void init();

    // 当前时间 (unix timestamp 秒)
    uint32_t now();

    // 解析为可读时间
    TimeInfo getTimeInfo();

    // 便捷方法
    uint8_t getHour();
    uint8_t getMinute();
    uint8_t getDay();           // 日 (1-31)
    uint8_t getMonth();

    // 格式化输出
    void getFormattedTime(char* buf, size_t len);   // "HH:MM"
    void getFormattedDate(char* buf, size_t len);   // "MM/DD"
    void getFormattedFull(char* buf, size_t len);   // "YYYY-MM-DD HH:MM:SS"

    // --- 模拟时间控制 (调试用) ---
    void advanceMinutes(uint32_t minutes);
    void advanceDays(uint32_t days);
    void setSimulatedTime(uint16_t year, uint8_t month, uint8_t day,
                          uint8_t hour, uint8_t minute);

    // --- 变化检测 ---
    // 调用后返回 true 一次, 直到下一分钟/天
    bool checkNewMinute();
    bool checkNewDay();

    // 经过时间计算
    uint32_t elapsedSince(uint32_t pastTimestamp);
    uint32_t daysBetween(uint32_t t1, uint32_t t2);

private:
    uint32_t _simulated_epoch = 0;      // 模拟的 unix 时间起点
    uint32_t _sim_start_millis = 0;     // 模拟开始时的 millis()
    uint32_t _advance_offset = 0;       // 手动快进累计秒数

    uint8_t _last_minute = 255;         // 上次检测的分钟
    uint8_t _last_day = 255;            // 上次检测的日

    // unix timestamp -> TimeInfo 转换
    TimeInfo epochToTimeInfo(uint32_t epoch);
    uint32_t timeInfoToEpoch(uint16_t year, uint8_t month, uint8_t day,
                             uint8_t hour, uint8_t minute, uint8_t second);
};

// 全局单例
extern TimeManager timeManager;

#endif // TIME_MANAGER_H
