// src/display/DisplayManager.h
// 显示接口层 - 所有面向玩家的视觉反馈统一入口
// 当前为空实现, 后续接入实际屏幕驱动

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include "../core/game_state.h"
#include "../pet/feeding.h"
#include "../pet/seriousness.h"
#include "../pet/evolution.h"
#include "../config/food_table.h"
#include "../input/input_map.h"

// ============================================================================
//  页面枚举 (与 UIContext 对应, 但用于显示层独立管理)
// ============================================================================

enum DisplayPage : uint8_t {
    PAGE_IDLE = 0,              // 待机主界面 (宠物动画)
    PAGE_STATUS,                // 状态面板
    PAGE_FEED_DRAW,             // 投喂: 抽卡展示
    PAGE_FEED_PICK,             // 投喂: 选择食物
    PAGE_FEED_RESULT,           // 投喂: 结果展示
    PAGE_SPECIAL_FOOD,          // 特殊食物选择
    PAGE_POKE_ANIM,             // 戳一戳动画
    PAGE_EVOLUTION,             // 进化演出
    PAGE_DESTROY_CONFIRM,       // 销毁确认
    PAGE_DAY_END,               // 日结算展示
    PAGE_BOOT,                  // 启动画面
    PAGE_COUNT
};

// ============================================================================
//  动画状态枚举
// ============================================================================

enum AnimState : uint8_t {
    ANIM_NONE = 0,              // 无动画
    ANIM_IDLE,                  // 待机呼吸/摇摆
    ANIM_EATING,                // 吃饭动画
    ANIM_POKE,                  // 戳一戳反应
    ANIM_EVOLUTION,             // 进化光效
    ANIM_COMBO,                 // 连携特效
    ANIM_MAPO_TOFU,             // 麻婆豆腐彩蛋
    ANIM_RHONGOMYNIAD,          // 狮子王终态演出
    ANIM_BLACK_RHONGOMYNIAD,    // 黑狮子王终态演出
    ANIM_DESTROY,               // 销毁动画
    ANIM_DAY_END,               // 日结算过场
    ANIM_SAVE,                  // 存档图标闪烁
    ANIM_COUNT
};

// ============================================================================
//  DisplayManager 类 (静态方法, 全局调用)
// ============================================================================

class DisplayManager {
public:
    // ==== 页面切换接口 ====
    static void switchPage(DisplayPage page);
    static DisplayPage getCurrentPage();

    // ==== 动画状态接口 ====
    static void playAnimation(AnimState anim);
    static void stopAnimation();
    static AnimState getCurrentAnimation();
    static bool isAnimationPlaying();

    // ==== 系统初始化 ====
    static void showBootScreen();
    static void showSaveLoaded();
    static void showSaveCorruptedNewGame();
    static void showNewGame();
    static void showSystemReady();

    // ==== 状态面板 ====
    static void showStatusPanel(const PetState& pet);
    static void hideStatusPanel();

    // ==== 投喂流程 ====
    static void showCannotInteract();
    static void showFeedCheckFailed(FeedResult result, uint32_t waitSeconds);
    static void showFeedDraw(const FeedDraw& draw);
    static void showFeedCursorMove(uint8_t cursor, const bool selected[4]);
    static void showFeedSlotToggle(uint8_t slot, bool isSelected);
    static void showFeedResult(const FeedOutcome& outcome, int16_t srAfter);
    static void showFeedComboTriggered(ComboType combo);
    static void showFeedCancel();

    // ==== 特殊食物 ====
    static void showSpecialFoodSelection(uint8_t count);
    static void showSpecialFoodCursor(uint8_t cursor);
    static void showSpecialFoodConfirm(uint8_t id);
    static void showSpecialFoodAnimation(uint8_t specialFoodId);

    // ==== 麻婆豆腐彩蛋 ====
    static void showMapoTofuTriggered(uint8_t currentCount, uint8_t threshold);
    static void showMapoTofuCurseActivated();

    // ==== 戳一戳 ====
    static void showPokeAnimation();
    static void showPokeResult(bool valueChanged, int16_t srBefore, int16_t srAfter);
    static void showPokeCooldown(uint32_t remainingSeconds);
    static void showPokeIdlePaused(uint32_t pauseMinutes);

    // ==== 日结算 ====
    static void showDayEndStart();
    static void showDayEndIdleSR(int16_t srBefore, int16_t srAfter,
                                  SeriousnessTier tierBefore, SeriousnessTier tierAfter);
    static void showDayEndTerminalState();
    static void showDayEndWindowBonus(int16_t bonusHP);
    static void showDayEndWindowPenalty(int16_t penaltyHP);
    static void showDayEndMissedFeed(uint8_t fedCount, uint8_t limit);
    static void showDayEndComplete(uint16_t dayNumber);
    static void showNewDayDetected();

    // ==== 进化系统 ====
    static void showChildGraduation(const EvolutionResult& result, Alignment alignment);
    static void showFormChange(Form formBefore, Form formAfter, int16_t seriousness,
                               SeriousnessTier tier);
    static void showRhongomyniadTriggered();
    static void showBlackRhongomyniadTriggered();
    static void showWhiteFunFormLocked(Form funForm);
    static void showEvolutionEvent(const EvolutionResult& result);

    // ==== 严肃值系统 ====
    static void showRhongoTimerStarted();
    static void showRhongoTimerTriggered();
    static void showRhongoTimerReset(int16_t seriousness, int16_t threshold);
    static void showMissedFeedPenalty(int16_t srBefore, int16_t srAfter, int16_t penalty);
    static void showIdleTierChange(SeriousnessTier tierBefore, SeriousnessTier tierAfter);
    static void showIdleFormChange(Form formBefore, Form formAfter);
    static void showIdleRhongoCountdown(uint32_t remainingHours);

    // ==== 销毁/重置 ====
    static void showDestroyConfirm(uint8_t cursor);
    static void showDestroyCursorMove(uint8_t cursor);
    static void showDestroyExecuted(Form destroyedForm);
    static void showDestroyReset();
    static void showDestroyCancelled();

    // ==== 存档系统 ====
    static void showSaveSuccess();
    static void showSaveFailed();
    static void showLoadSuccess();
    static void showLoadFailed();
    static void showSaveErased();
    static void showAutoSave();

    // ==== 时间系统 ====
    static void showTimeAdvanced(uint32_t minutes);
    static void showDayAdvanced(uint32_t days);
    static void showTimeSet();

    // ==== UI上下文切换 ====
    static void showContextChange(UIContext from, UIContext to);

private:
    static DisplayPage _currentPage;
    static AnimState _currentAnim;
};

#endif // DISPLAY_MANAGER_H
