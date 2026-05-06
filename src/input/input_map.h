// src/config/input_map.h
// 硬件输入接口映射 - ESP32-S3 物理按键定义与游戏输入事件声明
// 本文件仅定义接口枚举与结构体, 不包含任何映射逻辑的硬编码

#ifndef INPUT_MAP_H
#define INPUT_MAP_H

#include <stdint.h>

// ============================================================================
//  GPIO 引脚定义 (ESP32-S3) - 3键布局
// ============================================================================

#define PIN_BTN_L           4               // 左键 (GPIO4)
#define PIN_BTN_M           5               // 中键 (GPIO5)
#define PIN_BTN_R           6               // 右键 (GPIO6)

// ============================================================================
//  按键硬件参数
// ============================================================================

#define BTN_DEBOUNCE_MS         30          // 消抖时间 (ms)
#define BTN_LONG_PRESS_MS       800         // 长按判定阈值 (ms)
#define BTN_REPEAT_DELAY_MS     500         // 连按首次延迟 (ms)
#define BTN_REPEAT_INTERVAL_MS  150         // 连按重复间隔 (ms)
#define BTN_DESTROY_HOLD_MS     5000        // 三键同时长按5秒触发销毁确认

#define BTN_ACTIVE_LOW          1           // 1=低电平有效(内部上拉), 0=高电平有效
#define BTN_COUNT               3           // 总按键数量

// ============================================================================
//  物理按键ID
// ============================================================================

enum ButtonId : uint8_t {
    BTN_L = 0,                  // 左键
    BTN_M,                      // 中键
    BTN_R,                      // 右键
    BTN_ID_COUNT
};

// 按键到GPIO引脚的映射表
static const uint8_t BTN_PIN_MAP[BTN_ID_COUNT] = {
    PIN_BTN_L,
    PIN_BTN_M,
    PIN_BTN_R
};

// ============================================================================
//  按键事件类型
// ============================================================================

enum ButtonEventType : uint8_t {
    BTN_EVENT_NONE = 0,         // 无事件
    BTN_EVENT_PRESS,            // 按下 (单击)
    BTN_EVENT_RELEASE,          // 释放
    BTN_EVENT_LONG_PRESS,       // 长按触发
    BTN_EVENT_REPEAT            // 连按 (持续按住时重复触发)
};

// 按键事件结构体
struct ButtonEvent {
    ButtonId        button;     // 哪个按键
    ButtonEventType type;       // 事件类型
    uint32_t        timestamp;  // 事件时间戳 (ms)
    uint16_t        duration;   // 按住时长 (ms), 仅 RELEASE/LONG_PRESS 有效
};

// ============================================================================
//  游戏输入动作 (逻辑层 - 与物理按键解耦)
// ============================================================================

enum GameInput : uint8_t {
    // --- 通用导航 ---
    INPUT_NONE = 0,             // 无输入
    INPUT_NAV_LEFT,             // 导航: 左 / 上一项
    INPUT_NAV_RIGHT,            // 导航: 右 / 下一项
    INPUT_CONFIRM,              // 确认选择
    INPUT_CANCEL,               // 取消 / 返回上一级

    // --- 主交互 ---
    INPUT_FEED_START,           // 开始投喂 (抽取4张食物卡)
    INPUT_FEED_PICK_TOGGLE,     // 选中/取消选中当前食物槽位 (pick 3 of 4)
    INPUT_FEED_CONFIRM,         // 确认投喂选择 (提交3张)
    INPUT_FEED_CANCEL,          // 取消投喂 (放弃本次抽卡)
    INPUT_SPECIAL_FOOD_SELECT,  // 选择特殊食物 (连携奖励)
    INPUT_POKE,                 // 戳一戳 (互动)

    // --- 食物选择导航 ---
    INPUT_FOOD_SLOT_NEXT,       // 食物槽位: 下一个
    INPUT_FOOD_SLOT_PREV,       // 食物槽位: 上一个
    INPUT_SFOOD_NEXT,           // 特殊食物: 下一个
    INPUT_SFOOD_PREV,           // 特殊食物: 上一个

    // --- 系统操作 ---
    INPUT_MENU_OPEN,            // 打开系统菜单
    INPUT_MENU_CLOSE,           // 关闭系统菜单
    INPUT_STATUS_VIEW,          // 查看状态面板
    INPUT_SAVE_MANUAL,          // 手动存档
    INPUT_LOAD_SAVE,            // 读取存档

    // --- 销毁 (危险操作) ---
    INPUT_DESTROY_HOLD,         // 销毁: 长按中 (需持续 BTN_DESTROY_HOLD_MS)
    INPUT_DESTROY_CONFIRM,      // 销毁: 长按完成, 确认销毁
    INPUT_DESTROY_CANCEL,       // 销毁: 中途释放, 取消

    INPUT_COUNT                 // 总输入动作数
};

// ============================================================================
//  游戏界面上下文 (决定同一按键在不同界面的行为)
// ============================================================================

enum UIContext : uint8_t {
    UI_IDLE = 0,                // 待机主界面 (宠物动画)
    UI_STATUS,                  // 状态查看界面
    UI_MENU,                    // 系统菜单
    UI_FEED_DRAW,               // 投喂: 展示4张抽到的食物
    UI_FEED_PICK,               // 投喂: 选择3张食物
    UI_SPECIAL_FOOD,            // 连携奖励: 选择特殊食物
    UI_POKE_ANIM,               // 戳一戳动画播放中
    UI_EVOLUTION,               // 进化演出中
    UI_DESTROY_CONFIRM,         // 销毁确认界面
    UI_CONTEXT_COUNT
};

// ============================================================================
//  按键映射条目结构体 (供外部映射表使用)
// ============================================================================

struct InputMapping {
    UIContext       context;        // 当前界面上下文
    ButtonId        button;         // 物理按键
    ButtonEventType event_type;     // 事件类型 (PRESS / LONG_PRESS / REPEAT / RELEASE)
    GameInput       action;         // 映射到的游戏动作
};

// ============================================================================
//  辅助: 名称字符串 (调试用)
// ============================================================================

static const char* const GAME_INPUT_NAMES[] = {
    "NONE",
    "NAV_LEFT", "NAV_RIGHT",
    "CONFIRM", "CANCEL",
    "FEED_START", "FEED_PICK_TOGGLE", "FEED_CONFIRM", "FEED_CANCEL",
    "SPECIAL_FOOD_SELECT", "POKE",
    "FOOD_SLOT_NEXT", "FOOD_SLOT_PREV", "SFOOD_NEXT", "SFOOD_PREV",
    "MENU_OPEN", "MENU_CLOSE", "STATUS_VIEW",
    "SAVE_MANUAL", "LOAD_SAVE",
    "DESTROY_HOLD", "DESTROY_CONFIRM", "DESTROY_CANCEL"
};

static const char* const BTN_NAMES[BTN_ID_COUNT] = {
    "L", "M", "R"
};

static const char* const UI_CONTEXT_NAMES[UI_CONTEXT_COUNT] = {
    "IDLE", "STATUS", "MENU",
    "FEED_DRAW", "FEED_PICK", "SPECIAL_FOOD",
    "POKE_ANIM", "EVOLUTION", "DESTROY_CONFIRM"
};

#endif // INPUT_MAP_H
