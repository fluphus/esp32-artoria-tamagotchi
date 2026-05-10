// src/display/display_model.h
// 显示数据模型 - DisplayManager 的 show*() 函数只更新此模型
// DisplayRenderer 根据此模型绘制屏幕

#ifndef DISPLAY_MODEL_H
#define DISPLAY_MODEL_H

#include <stdint.h>
#include <string.h>
#include "../core/game_state.h"
#include "../pet/feeding.h"
#include "../pet/seriousness.h"
#include "../pet/evolution.h"
#include "../input/input_map.h"

// ============================================================================
//  DisplayModel - 当前帧需要显示的所有数据
// ============================================================================

struct DisplayModel {
    // --- 宠物快照 ---
    PetState petSnapshot;
    uint16_t deviceRounds;          // 设备轮次 (从 DeviceState 同步)
    bool isVisiting;                // 是否在串门状态

    // --- 投喂流程 ---
    FeedDraw feedDraw;
    bool feedSelected[4];
    uint8_t feedCursor;
    FeedOutcome feedOutcome;
    int16_t feedSrAfter;

    // --- 特殊食物 ---
    uint8_t specialFoodCount;
    uint8_t specialFoodCursor;
    uint8_t specialFoodSelectedId;
    bool mapoTriggered;
    uint8_t mapoCount;
    bool mapoCurseActivated;

    // --- 进化 ---
    EvolutionResult evolution;
    Form destroyedForm;

    // --- 严肃值变化 ---
    int16_t srBefore;
    int16_t srAfter;
    SeriousnessTier tierBefore;
    SeriousnessTier tierAfter;

    // --- 戳一戳 ---
    bool pokeValueChanged;
    int16_t pokeSrBefore;
    int16_t pokeSrAfter;

    // --- 日结算 ---
    struct DayEndData {
        int16_t idleSrBefore;
        int16_t idleSrAfter;
        SeriousnessTier idleTierBefore;
        SeriousnessTier idleTierAfter;
        bool windowBonusApplied;
        int16_t windowBonusHP;
        bool windowPenaltyApplied;
        int16_t windowPenaltyHP;
        bool missedFeedPenalty;
        uint8_t fedCount;
        uint8_t feedLimit;
        uint16_t dayNumber;
        bool terminalState;
    } dayEnd;

    // --- 销毁确认 ---
    uint8_t destroyCursor;          // 0=yes, 1=no

    // --- 图鉴 ---
    uint8_t galleryPage;            // 当前页码
    uint8_t gallerySelectedIndex;   // 当前页内选中格子索引
    uint8_t galleryTotalForms;      // 总形态数
    uint8_t galleryItemsThisPage;   // 当前页实际有效格子数

    // --- Toast 消息 ---
    char toast[64];
    uint32_t toastUntilMs;

    // --- Boot 消息 ---
    char bootMessage[32];

    // --- 时间显示 ---
    char timeStr[6];                // "HH:MM"
    char dateStr[6];                // "MM/DD"
    uint16_t ageDay;

    // --- 初次时间设置 ---
    uint16_t setupYear;
    uint8_t setupMonth;
    uint8_t setupDay;
    uint8_t setupHour;
    uint8_t setupMinute;
    uint8_t setupFieldIndex;        // 0=Y 1=M 2=D 3=H 4=m
    bool setupAwaitingConfirm;

    // --- 动画帧状态 (供 renderer 使用) ---
    uint8_t animState;              // 当前 AnimState 枚举值
    uint32_t animElapsedMs;         // 动画已播放时间
    uint8_t animFrameIndex;         // 当前帧索引 (由 update 计算)

    // --- 初始化 ---
    void clear() {
        memset(this, 0, sizeof(DisplayModel));
        destroyCursor = 1;  // 默认 no
    }
};

#endif // DISPLAY_MODEL_H