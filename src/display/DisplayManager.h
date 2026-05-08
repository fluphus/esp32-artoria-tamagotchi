// src/display/DisplayManager.h
// 显示管理器 - UI 状态机
// 不直接写绘制代码, 只管理页面/动画状态和 DisplayModel
// 实际绘制委托给 DisplayRenderer

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include "display_config.h"
#include "display_model.h"
#include "display_renderer.h"
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
    PAGE_BOOT = 0,              // 启动画面
    PAGE_IDLE,                  // 待机主界面 (宠物动画)
    PAGE_STATUS,                // 状态面板
    PAGE_FEED_DRAW,             // 投喂: 抽卡展示
    PAGE_FEED_PICK,             // 投喂: 选择食物
    PAGE_FEED_RESULT,           // 投喂: 结果展示
    PAGE_SPECIAL_FOOD,          // 特殊食物选择
    PAGE_POKE_ANIM,             // 戳一戳动画
    PAGE_EVOLUTION,             // 进化演出
    PAGE_DESTROY_CONFIRM,       // 销毁确认
    PAGE_DAY_END,               // 日结算展示
    PAGE_WAIT_TIME_SET,         // 等待串口设置时间
    PAGE_GALLERY,               // 图鉴浏览
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
    ANIM_NOBU_EVENT,            // nobu 彩蛋事件 (占位动画)
    ANIM_SAVE,                  // 存档图标闪烁
    ANIM_GALLERY_COMPLETE,      // 全图鉴解锁特殊动画 (占位)
    ANIM_COUNT
};

// ============================================================================
//  DisplayManager 类 (静态方法, 全局 UI 状态机)
// ============================================================================

class DisplayManager {
public:
    // ==== 生命周期 ====
    static void init();
    static void update(uint32_t nowMs);
    static void renderIfDirty();

    // ==== 页面/动画控制 ====
    static void switchPage(DisplayPage page);
    static DisplayPage getCurrentPage();
    static void setAnimation(AnimState anim, uint32_t durationMs, UIContext completeContext);
    static void stopAnimation();
    static AnimState getCurrentAnimation();
    static bool isAnimationPlaying();
    static bool isPageBlockingInput();
    static bool isPageHoldActive();

    // ==== 数据更新接口 (show* 函数只更新 model + dirty) ====

    // 系统
    static void showBootScreen();
    static void showSaveLoaded();
    static void showSaveCorruptedNewGame();
    static void showNewGame();
    static void showSystemReady();

    // 状态面板
    static void showStatusPanel(const PetState& pet);
    static void hideStatusPanel();

    // 投喂流程
    static void showCannotInteract();
    static void showFeedCheckFailed(FeedResult result, uint32_t waitSeconds);
    static void showFeedDraw(const FeedDraw& draw);
    static void showFeedCursorMove(uint8_t cursor, const bool selected[4]);
    static void showFeedSlotToggle(uint8_t slot, bool isSelected);
    static void showFeedResult(const FeedOutcome& outcome, int16_t srAfter);
    static void showFeedComboTriggered(ComboType combo);
    static void showFeedCancel();

    // 特殊食物
    static void showSpecialFoodSelection(uint8_t count);
    static void showSpecialFoodCursor(uint8_t cursor);
    static void showSpecialFoodConfirm(uint8_t id, const FeedOutcome& outcome);

    // 麻婆豆腐彩蛋
    static void showMapoTofuTriggered(uint8_t currentCount, uint8_t threshold);
    static void showMapoTofuCurseActivated();

    // 戳一戳
    static void showPokeAnimation();
    static void showPokeResult(bool valueChanged, int16_t srBefore, int16_t srAfter);

    // 日结算
    static void showDayEndStart();
    static void showDayEndIdleSR(int16_t srBefore, int16_t srAfter,
                                  SeriousnessTier tierBefore, SeriousnessTier tierAfter);
    static void showDayEndTerminalState();
    static void showDayEndWindowBonus(int16_t bonusHP);
    static void showDayEndWindowPenalty(int16_t penaltyHP);
    static void showDayEndMissedFeed(uint8_t fedCount, uint8_t limit);
    static void showDayEndComplete(uint16_t dayNumber);
    static void showNewDayDetected();

    // 进化系统
    static void showChildGraduation(const EvolutionResult& result, Alignment alignment);
    static void showFormChange(Form formBefore, Form formAfter, int16_t seriousness,
                               SeriousnessTier tier);
    static void showEvolutionEvent(const EvolutionResult& result, int16_t srAfter);

    // 严肃值系统
    static void showIdleTierChange(SeriousnessTier tierBefore, SeriousnessTier tierAfter);
    static void showIdleFormChange(Form formBefore, Form formAfter);
    static void showIdleRhongoCountdown(uint32_t remainingHours);
    static void showMissedFeedPenalty(int16_t srBefore, int16_t srAfter, int16_t penalty);

    // 销毁/重置
    static void showDestroyConfirm(uint8_t cursor);
    static void showDestroyCursorMove(uint8_t cursor);
    static void showDestroyExecuted(Form destroyedForm);
    static void showDestroyReset();
    static void showDestroyCancelled();

    // 存档系统
    static void showAutoSave();

    // 等待时间设置
    static void showWaitTimeSet();

    // 图鉴
    static void showGalleryGrid(int page_index, int selected_index);
    static void showGalleryCompleteUnlocked();

    // Toast
    static void showToast(const char* message, uint32_t durationMs = PAGE_DURATION_TOAST);

    // 宠物快照更新 (每帧由 main 调用)
    static void updatePetSnapshot(const PetState& pet);

    // 获取 model (只读, 供外部查询)
    static const DisplayModel& getModel() { return _model; }

private:
    static DisplayPage  _currentPage;
    static AnimState    _currentAnim;
    static uint32_t     _pageEnteredMs;
    static uint32_t     _animStartedMs;
    static uint32_t     _animDurationMs;
    static UIContext    _animCompleteContext;    // 动画结束后通知的上下文
    static bool         _dirty;
    static DisplayModel _model;
    static uint32_t     _lastRenderMs;

    // --- Page hold 机制 ---
    static bool         _pageHoldActive;        // 是否正在 hold
    static uint32_t     _pageHoldUntilMs;       // hold 结束时间
    static DisplayPage  _pageAfterHold;         // hold 结束后切换到的页面
    static UIContext    _contextAfterHold;      // hold 结束后切换到的上下文

    // --- Pending evolution (麻婆诅咒链式动画) ---
    static bool             _pendingEvolutionActive;
    static EvolutionResult  _pendingEvolution;
    static int16_t          _pendingEvolutionSrAfter;

    // 内部: 标记脏
    static void markDirty();

    // 内部: 获取动画时长
    static uint32_t getAnimDuration(AnimState anim);

    // 内部: 设置页面 hold
    static void holdPageThen(uint32_t durationMs, DisplayPage nextPage, UIContext nextContext);
};

#endif // DISPLAY_MANAGER_H
