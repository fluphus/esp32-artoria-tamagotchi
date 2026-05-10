// src/input/menu_controller.h
// 菜单与交互控制器 - 管理游戏交互流程
// 将 GameInput 动作转化为对游戏系统的调用
// 保留 UI/动画回调接口供外部实现

#ifndef MENU_CONTROLLER_H
#define MENU_CONTROLLER_H

#include <stdint.h>
#include <utility>
#include "input_map.h"
#include "input_manager.h"
#include "../core/game_state.h"
#include "../pet/feeding.h"
#include "../pet/seriousness.h"
#include "../pet/evolution.h"
#include "../pet/gallery.h"
#include "../core/time_manager.h"
#include "../core/save_manager.h"

// ============================================================================
//  UI 回调接口 (由外部 UI/动画系统实现)
// ============================================================================

struct UICallbacks {
    // --- 状态面板 ---
    void (*onStatusOpen)(const PetState& pet);
    void (*onStatusClose)();

    // --- 投喂流程 ---
    void (*onFeedDrawStart)(const FeedDraw& draw);          // 展示4张食物卡动画
    void (*onFeedCursorMove)(uint8_t cursor, const bool selected[4]);  // 光标移动
    void (*onFeedSlotToggle)(uint8_t slot, bool selected);  // 选取/取消选取
    void (*onFeedConfirm)(const FeedOutcome& outcome, int16_t srAfter);  // 投喂结果 + 最终SR
    void (*onFeedCancel)();                                 // 取消投喂

    // --- 特殊食物 ---
    void (*onSpecialFoodShow)(uint8_t count);               // 展示特殊食物选项
    void (*onSpecialFoodCursor)(uint8_t cursor);            // 光标移动
    void (*onSpecialFoodSelect)(uint8_t id, const FeedOutcome& outcome);  // 选择确认 + 完整outcome

    // --- 戳一戳 ---
    void (*onPokeStart)();                                  // 戳一戳动画开始
    void (*onPokeResult)(bool valueChanged, int16_t srBefore, int16_t srAfter);

    // --- 销毁 ---
    void (*onDestroyConfirmShow)(uint8_t cursor);           // 展示销毁确认 (0=yes, 1=no)
    void (*onDestroyCursorMove)(uint8_t cursor);            // yes/no 光标移动
    void (*onDestroyExecuted)(Form destroyedForm);          // 销毁已执行 (传入被销毁的形态)
    void (*onDestroyCancelled)();                           // 销毁已取消

    // --- 进化 ---
    void (*onEvolution)(const EvolutionResult& result, int16_t srAfter);  // 进化事件 + 当前SR

    // --- 通用 ---
    void (*onContextChange)(UIContext from, UIContext to);  // UI上下文切换
    void (*onInitialTimeEdit)(uint16_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t minute, uint8_t fieldIndex,
                              bool awaitingConfirm);
    void (*onInitialTimeConfirm)(uint16_t year, uint8_t month, uint8_t day,
                                 uint8_t hour, uint8_t minute);
};

// ============================================================================
//  食物选择状态
// ============================================================================

struct FeedPickState {
    FeedDraw    draw;                   // 当前抽到的4张食物
    uint8_t     cursor;                 // 当前光标位置 (0-3)
    bool        selected[4];            // 各槽位是否被选中
    uint8_t     selected_count;         // 已选中数量
    bool        active;                 // 是否处于选择状态
};

// ============================================================================
//  销毁确认状态
// ============================================================================

struct DestroyConfirmState {
    uint8_t     cursor;                 // 0 = yes (INPUT_DESTROY_CONFIRM), 1 = no (INPUT_DESTROY_CANCEL)
    bool        active;
};

// ============================================================================
//  初次时间设置状态
// ============================================================================

struct InitialTimeSetupState {
    bool active;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t fieldIndex; // 0=Y 1=M 2=D 3=H 4=m
    bool awaitingConfirm;
    bool leftLockedUntilRelease;
};

// ============================================================================
//  MenuController 类
// ============================================================================

class MenuController {
public:
    // 初始化
    void init(PetState* pet, DeviceState* devState, UICallbacks* callbacks);

    // 每帧调用: 处理 InputManager 产出的 GameInput
    void update();

    // 外部通知: 动画播放完成 (用于从动画状态恢复)
    void onAnimationComplete(UIContext animContext);

    // 查询状态
    const FeedPickState& getFeedState() const { return _feed; }
    const DestroyConfirmState& getDestroyState() const { return _destroy; }
    UIContext getCurrentContext() const { return inputManager.getContext(); }

    // 手动触发上下文切换 (供外部使用)
    void switchContext(UIContext newCtx);

    // 注入模拟按键事件 (调试用, 绕过GPIO)
    // 直接向 ButtonDriver 注入事件, 经 InputManager 映射后处理
    void injectButton(ButtonId btn, ButtonEventType type);
    void startInitialTimeSetup(uint32_t baseEpoch);

private:
    PetState*       _pet;
    DeviceState*    _devState;
    UICallbacks*    _callbacks;
    FeedPickState   _feed;
    DestroyConfirmState _destroy;
    InitialTimeSetupState _initialTime;

    // 特殊食物选择
    uint8_t         _sfood_cursor;
    uint8_t         _sfood_count;
    bool            _combo_pending;
    FeedOutcome     _last_feed_outcome;

    // 动作处理
    void handleAction(GameInput action);

    // 各上下文的动作处理
    void handleIdle(GameInput action);
    void handleStatus(GameInput action);
    void handleFeedPick(GameInput action);
    void handleSpecialFood(GameInput action);
    void handleDestroyConfirm(GameInput action);
    void handleGallery(GameInput action);
    void handleTimeSetup(GameInput action);

    // 业务逻辑
    void startFeed();
    void toggleFoodSlot();
    void confirmFeed();
    void cancelFeed();
    void doPoke();
    void enterDestroyConfirm();
    void executeDestroy();
    void cancelDestroy();
    void emitInitialTimeEdit();
    void incrementInitialTimeField();

    // 回调安全调用（UICallbacks 内为函数指针字段：用成员数据指针，避免实参求值时解引用 _callbacks）
    template<typename Sig, typename... Args>
    void safeCallback(Sig UICallbacks::*member, Args&&... args) {
        if (!_callbacks) return;
        Sig fn = _callbacks->*member;
        if (fn) fn(std::forward<Args>(args)...);
    }
};

// 全局单例
extern MenuController menuController;

#endif // MENU_CONTROLLER_H
