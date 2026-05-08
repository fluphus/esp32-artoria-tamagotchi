// src/input/input_manager.cpp
// 输入管理器实现 - 上下文感知的按键到游戏动作映射

#include "input_manager.h"

InputManager inputManager;

void InputManager::init() {
    _context = UI_IDLE;
    _action_count = 0;
    _mapping_count = 0;
    buildMappingTable();
    buttonDriver.init();
}

uint8_t InputManager::update() {
    _action_count = 0;

    // 更新物理按键
    buttonDriver.update();

    // 优先检测三键销毁组合
    if (buttonDriver.isComboTriggered()) {
        // 三键组合触发, 不再处理普通按键事件
        return 0;
    }

    // 如果三键正在按住(但未触发), 不处理普通事件以避免误触
    if (buttonDriver.isComboHolding() && buttonDriver.comboHoldDuration() > 500) {
        return 0;
    }

    // 处理普通按键事件
    uint8_t eventCount = buttonDriver.getEventCount();
    const ButtonEvent* events = buttonDriver.getEvents();

    for (uint8_t i = 0; i < eventCount && _action_count < MAX_ACTIONS_PER_FRAME; i++) {
        const ButtonEvent& evt = events[i];
        GameInput action = lookupMapping(_context, evt.button, evt.type);
        if (action != INPUT_NONE) {
            _actions[_action_count++] = action;
        }
    }

    return _action_count;
}

uint8_t InputManager::processInjected() {
    _action_count = 0;

    // 不调用 buttonDriver.update(), 直接读取已注入的事件
    uint8_t eventCount = buttonDriver.getEventCount();
    const ButtonEvent* events = buttonDriver.getEvents();

    for (uint8_t i = 0; i < eventCount && _action_count < MAX_ACTIONS_PER_FRAME; i++) {
        const ButtonEvent& evt = events[i];
        GameInput action = lookupMapping(_context, evt.button, evt.type);
        if (action != INPUT_NONE) {
            _actions[_action_count++] = action;
        }
    }

    return _action_count;
}

void InputManager::setContext(UIContext ctx) {
    if (ctx < UI_CONTEXT_COUNT) {
        _context = ctx;
    }
}

bool InputManager::isDestroyComboTriggered() const {
    return buttonDriver.isComboTriggered();
}

bool InputManager::isDestroyComboHolding() const {
    return buttonDriver.isComboHolding();
}

uint32_t InputManager::destroyComboHoldDuration() const {
    return buttonDriver.comboHoldDuration();
}

void InputManager::resetDestroyCombo() {
    buttonDriver.resetCombo();
}

GameInput InputManager::lookupMapping(UIContext ctx, ButtonId btn, ButtonEventType evt) {
    for (uint8_t i = 0; i < _mapping_count; i++) {
        const InputMapping& m = _mappings[i];
        if (m.context == ctx && m.button == btn && m.event_type == evt) {
            return m.action;
        }
    }
    return INPUT_NONE;
}

void InputManager::addMapping(UIContext ctx, ButtonId btn, ButtonEventType evt, GameInput action) {
    if (_mapping_count >= INPUT_MAPPING_TABLE_SIZE) return;
    _mappings[_mapping_count].context = ctx;
    _mappings[_mapping_count].button = btn;
    _mappings[_mapping_count].event_type = evt;
    _mappings[_mapping_count].action = action;
    _mapping_count++;
}

void InputManager::buildMappingTable() {
    _mapping_count = 0;

    // ========================================================================
    // UI_IDLE - 主界面 (待机)
    // ========================================================================
    // 左键: 开始投喂
    addMapping(UI_IDLE, BTN_L, BTN_EVENT_PRESS, INPUT_FEED_START);
    // 中键: 查看状态面板
    addMapping(UI_IDLE, BTN_M, BTN_EVENT_PRESS, INPUT_STATUS_VIEW);
    // 中键长按: 打开图鉴
    addMapping(UI_IDLE, BTN_M, BTN_EVENT_LONG_PRESS, INPUT_GALLERY_OPEN);
    // 右键: 戳一戳
    addMapping(UI_IDLE, BTN_R, BTN_EVENT_PRESS, INPUT_POKE);

    // ========================================================================
    // UI_STATUS - 状态面板
    // ========================================================================
    // 中键: 关闭状态面板
    addMapping(UI_STATUS, BTN_M, BTN_EVENT_PRESS, INPUT_STATUS_VIEW);
    // 中键长按: 打开图鉴 (从状态面板直接进入, 处理长按时PRESS已先触发的情况)
    addMapping(UI_STATUS, BTN_M, BTN_EVENT_LONG_PRESS, INPUT_GALLERY_OPEN);

    // ========================================================================
    // UI_FEED_PICK - 食物选择界面 (选3张)
    // ========================================================================
    // 左键: 光标左移
    addMapping(UI_FEED_PICK, BTN_L, BTN_EVENT_PRESS, INPUT_FOOD_SLOT_PREV);
    // 右键: 光标右移
    addMapping(UI_FEED_PICK, BTN_R, BTN_EVENT_PRESS, INPUT_FOOD_SLOT_NEXT);
    // 中键: 选取/取消选取当前食物
    addMapping(UI_FEED_PICK, BTN_M, BTN_EVENT_PRESS, INPUT_FEED_PICK_TOGGLE);

    // ========================================================================
    // UI_SPECIAL_FOOD - 特殊食物选择
    // ========================================================================
    // 左键: 光标左移
    addMapping(UI_SPECIAL_FOOD, BTN_L, BTN_EVENT_PRESS, INPUT_SFOOD_PREV);
    // 右键: 光标右移
    addMapping(UI_SPECIAL_FOOD, BTN_R, BTN_EVENT_PRESS, INPUT_SFOOD_NEXT);
    // 中键: 确认选择
    addMapping(UI_SPECIAL_FOOD, BTN_M, BTN_EVENT_PRESS, INPUT_SPECIAL_FOOD_SELECT);

    // ========================================================================
    // UI_DESTROY_CONFIRM - 销毁确认界面
    // ========================================================================
    // 左键: 导航左 (选择 yes/no)
    addMapping(UI_DESTROY_CONFIRM, BTN_L, BTN_EVENT_PRESS, INPUT_NAV_LEFT);
    // 右键: 导航右 (选择 yes/no)
    addMapping(UI_DESTROY_CONFIRM, BTN_R, BTN_EVENT_PRESS, INPUT_NAV_RIGHT);
    // 中键: 确认当前选择
    addMapping(UI_DESTROY_CONFIRM, BTN_M, BTN_EVENT_PRESS, INPUT_CONFIRM);

    // ========================================================================
    // UI_MENU - 系统菜单 (预留)
    // ========================================================================
    addMapping(UI_MENU, BTN_L, BTN_EVENT_PRESS, INPUT_NAV_LEFT);
    addMapping(UI_MENU, BTN_R, BTN_EVENT_PRESS, INPUT_NAV_RIGHT);
    addMapping(UI_MENU, BTN_M, BTN_EVENT_PRESS, INPUT_CONFIRM);

    // ========================================================================
    // UI_POKE_ANIM - 戳一戳动画播放中 (忽略输入)
    // ========================================================================
    // 无映射, 动画播放期间不响应

    // ========================================================================
    // UI_FEED_DRAW - 投喂抽卡展示 (等待动画完成后自动切换到 FEED_PICK)
    // ========================================================================
    // 无映射, 等待动画

    // ========================================================================
    // UI_GALLERY - 图鉴浏览界面
    // ========================================================================
    // 左键: 光标左移 / 翻页
    addMapping(UI_GALLERY, BTN_L, BTN_EVENT_PRESS, INPUT_GALLERY_NAV_LEFT);
    // 右键: 光标右移 / 翻页
    addMapping(UI_GALLERY, BTN_R, BTN_EVENT_PRESS, INPUT_GALLERY_NAV_RIGHT);
    // 中键: 退出图鉴
    addMapping(UI_GALLERY, BTN_M, BTN_EVENT_PRESS, INPUT_GALLERY_CLOSE);

    // ========================================================================
    // UI_TIME_SETUP - 初次开机时间设置
    // ========================================================================
    // 左键: 当前字段递增
    addMapping(UI_TIME_SETUP, BTN_L, BTN_EVENT_PRESS, INPUT_TIMESET_INC);
    addMapping(UI_TIME_SETUP, BTN_L, BTN_EVENT_REPEAT, INPUT_TIMESET_INC);
    // 左键长按: 恢复默认时间
    addMapping(UI_TIME_SETUP, BTN_L, BTN_EVENT_LONG_PRESS, INPUT_TIMESET_RESET_DEFAULT);
    // 左键释放: 解除长按后的锁
    addMapping(UI_TIME_SETUP, BTN_L, BTN_EVENT_RELEASE, INPUT_TIMESET_LEFT_RELEASE);
    // 中键: 确认当前字段/下一字段
    addMapping(UI_TIME_SETUP, BTN_M, BTN_EVENT_PRESS, INPUT_TIMESET_NEXT);
    // 右键: 回退到上一字段
    addMapping(UI_TIME_SETUP, BTN_R, BTN_EVENT_PRESS, INPUT_TIMESET_BACK);
}
