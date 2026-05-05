// src/core/save_manager.h

#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include "game_state.h"
#include <stdint.h>
#include <stddef.h>

// ============================================
// NVS 存档管理器
// 自动存档 + 关键事件存档
// 支持存档版本检查和损坏恢复
// ============================================

enum SaveResult : uint8_t {
    SAVE_OK = 0,
    SAVE_ERR_NVS_INIT,
    SAVE_ERR_WRITE,
    SAVE_ERR_READ,
    SAVE_ERR_NO_DATA,
    SAVE_ERR_VERSION_MISMATCH,
    SAVE_ERR_CHECKSUM,
    SAVE_ERR_CORRUPTED
};

class SaveManager {
public:
    // 初始化 NVS
    SaveResult init();

    // 存档
    SaveResult save(const PetState& pet);

    // 读档
    SaveResult load(PetState& pet);

    // 是否存在存档
    bool hasSave();

    // 删除存档 (销毁时用)
    SaveResult erase();

    // 获取上次存档时间
    uint32_t getLastSaveTime() { return _lastSaveTime; }

    // 检查是否需要自动存档
    bool shouldAutoSave(uint32_t currentTime);

    // 更新自动存档计时
    void markSaved(uint32_t currentTime);

private:
    bool _initialized = false;
    uint32_t _lastSaveTime = 0;

    // 简单校验和
    uint16_t calcChecksum(const uint8_t* data, size_t len);
};

// 全局单例
extern SaveManager saveManager;

#endif // SAVE_MANAGER_H
