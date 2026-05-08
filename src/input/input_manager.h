// src/input/input_manager.h
// 输入管理器 - 根据当前UI上下文将 ButtonEvent 映射为 GameInput
// 管理 UIContext 状态切换

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "input_map.h"
#include "button_driver.h"

// ============================================================================
//  输入管理器配置
// ============================================================================

// 映射表最大条目数
#define INPUT_MAPPING_TABLE_SIZE    40

// ============================================================================
//  InputManager 类
// ============================================================================

class InputManager {
public:
    // 初始化 (建立映射表)
    void init();

    // 每帧调用: 读取 ButtonDriver 事件, 转换为 GameInput
    // 返回本帧产出的 GameInput 数量
    uint8_t update();

    // 处理已注入的事件 (调试用, 不调用 buttonDriver.update)
    // 直接将 buttonDriver 中已有的事件映射为 GameInput
    uint8_t processInjected();

    // 获取本帧产出的 GameInput 列表
    static const uint8_t MAX_ACTIONS_PER_FRAME = 8;
    const GameInput* getActions() const { return _actions; }
    uint8_t getActionCount() const { return _action_count; }

    // UI上下文管理
    UIContext getContext() const { return _context; }
    void setContext(UIContext ctx);

    // 查询三键销毁组合状态
    bool isDestroyComboTriggered() const;
    bool isDestroyComboHolding() const;
    uint32_t destroyComboHoldDuration() const;
    void resetDestroyCombo();

private:
    UIContext    _context;
    GameInput    _actions[MAX_ACTIONS_PER_FRAME];
    uint8_t     _action_count;

    // 静态映射表
    InputMapping _mappings[INPUT_MAPPING_TABLE_SIZE];
    uint8_t      _mapping_count;

    void buildMappingTable();
    GameInput lookupMapping(UIContext ctx, ButtonId btn, ButtonEventType evt);
    void addMapping(UIContext ctx, ButtonId btn, ButtonEventType evt, GameInput action);
};

// 全局单例
extern InputManager inputManager;

#endif // INPUT_MANAGER_H
