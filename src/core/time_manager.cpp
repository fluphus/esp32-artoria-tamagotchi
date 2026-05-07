// src/core/time_manager.cpp

#include "time_manager.h"
#include "../config/game_config.h"
#include "../display/DisplayManager.h"
#include <Arduino.h>

TimeManager timeManager;

// 每月天数 (非闰年)
static const uint8_t DAYS_IN_MONTH[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static bool isLeapYear(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint16_t daysInYear(uint16_t year) {
    return isLeapYear(year) ? 366 : 365;
}

static uint8_t daysInMonth(uint16_t year, uint8_t month) {
    if (month == 2 && isLeapYear(year)) return 29;
    if (month >= 1 && month <= 12) return DAYS_IN_MONTH[month - 1];
    return 30;
}

void TimeManager::init() {
    // 模拟模式: 从 2025-01-01 08:00:00 开始
    _simulated_epoch = timeInfoToEpoch(2025, 1, 1, 8, 0, 0);
    _sim_start_millis = millis();
    _advance_offset = 0;

    // 初始化为当前值, 防止首次 loop 误触发 checkNewMinute/checkNewDay
    TimeInfo t = getTimeInfo();
    _last_minute = t.minute;
    _last_day = t.day;

    Serial.println("[Time] TimeManager initialized (simulated mode)");

    char buf[24];
    getFormattedFull(buf, sizeof(buf));
    Serial.printf("[Time] Start time: %s\n", buf);
}

uint32_t TimeManager::now() {
    uint32_t real_elapsed_sec = (millis() - _sim_start_millis) / 1000;

#if SIM_MINUTES_PER_REAL_SEC > 0
    // 自动快进模式
    real_elapsed_sec *= (SIM_MINUTES_PER_REAL_SEC * 60);
#endif

    return _simulated_epoch + real_elapsed_sec + _advance_offset;
}

TimeInfo TimeManager::getTimeInfo() {
    return epochToTimeInfo(now());
}

uint8_t TimeManager::getHour() {
    return getTimeInfo().hour;
}

uint8_t TimeManager::getMinute() {
    return getTimeInfo().minute;
}

uint8_t TimeManager::getDay() {
    return getTimeInfo().day;
}

uint8_t TimeManager::getMonth() {
    return getTimeInfo().month;
}

void TimeManager::getFormattedTime(char* buf, size_t len) {
    TimeInfo t = getTimeInfo();
    snprintf(buf, len, "%02d:%02d", t.hour, t.minute);
}

void TimeManager::getFormattedDate(char* buf, size_t len) {
    TimeInfo t = getTimeInfo();
    snprintf(buf, len, "%02d/%02d", t.month, t.day);
}

void TimeManager::getFormattedFull(char* buf, size_t len) {
    TimeInfo t = getTimeInfo();
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
             t.year, t.month, t.day, t.hour, t.minute, t.second);
}

void TimeManager::advanceMinutes(uint32_t minutes) {
    _advance_offset += minutes * 60;

    char buf[24];
    getFormattedFull(buf, sizeof(buf));
    Serial.printf("[Time] Advanced %lu min -> %s\n", minutes, buf);
    DisplayManager::showTimeAdvanced(minutes);
}

void TimeManager::advanceDays(uint32_t days) {
    _advance_offset += days * 86400;

    char buf[24];
    getFormattedFull(buf, sizeof(buf));
    Serial.printf("[Time] Advanced %lu day(s) -> %s\n", days, buf);
    DisplayManager::showDayAdvanced(days);
}

void TimeManager::setSimulatedTime(uint16_t year, uint8_t month, uint8_t day,
                                    uint8_t hour, uint8_t minute) {
    _simulated_epoch = timeInfoToEpoch(year, month, day, hour, minute, 0);
    _sim_start_millis = millis();
    _advance_offset = 0;

    char buf[24];
    getFormattedFull(buf, sizeof(buf));
    Serial.printf("[Time] Set to: %s\n", buf);
    DisplayManager::showTimeSet();
}

bool TimeManager::checkNewMinute() {
    uint8_t currentMinute = getMinute();
    if (currentMinute != _last_minute) {
        _last_minute = currentMinute;
        return true;
    }
    return false;
}

bool TimeManager::checkNewDay() {
    uint8_t currentDay = getDay();
    if (currentDay != _last_day) {
        _last_day = currentDay;
        return true;
    }
    return false;
}

uint32_t TimeManager::elapsedSince(uint32_t pastTimestamp) {
    uint32_t current = now();
    if (current > pastTimestamp) {
        return current - pastTimestamp;
    }
    return 0;
}

uint32_t TimeManager::daysBetween(uint32_t t1, uint32_t t2) {
    uint32_t diff = (t2 > t1) ? (t2 - t1) : (t1 - t2);
    return diff / 86400;
}

// --- Unix Epoch 转换 ---

TimeInfo TimeManager::epochToTimeInfo(uint32_t epoch) {
    TimeInfo t;

    uint32_t remaining = epoch;

    // 秒 -> 时分秒
    t.second = remaining % 60;
    remaining /= 60;
    t.minute = remaining % 60;
    remaining /= 60;
    t.hour = remaining % 24;
    remaining /= 24;

    // remaining = 从 epoch 起的天数
    // Unix epoch = 1970-01-01, weekday = Thursday (4)
    t.weekday = (remaining + 4) % 7;

    // 年
    t.year = 1970;
    while (true) {
        uint16_t diy = daysInYear(t.year);
        if (remaining < diy) break;
        remaining -= diy;
        t.year++;
    }

    // 月
    t.month = 1;
    while (true) {
        uint8_t dim = daysInMonth(t.year, t.month);
        if (remaining < dim) break;
        remaining -= dim;
        t.month++;
    }

    // 日
    t.day = remaining + 1;

    return t;
}

uint32_t TimeManager::timeInfoToEpoch(uint16_t year, uint8_t month, uint8_t day,
                                       uint8_t hour, uint8_t minute, uint8_t second) {
    uint32_t totalDays = 0;

    // 累计年
    for (uint16_t y = 1970; y < year; y++) {
        totalDays += daysInYear(y);
    }

    // 累计月
    for (uint8_t m = 1; m < month; m++) {
        totalDays += daysInMonth(year, m);
    }

    // 累计日
    totalDays += (day - 1);

    return totalDays * 86400UL + hour * 3600UL + minute * 60UL + second;
}
