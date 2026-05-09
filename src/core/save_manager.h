// src/core/save_manager.h

#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include "game_state.h"
#include "../pet/gallery.h"
#include <stdint.h>
#include <stddef.h>

// ============================================
// NVS 双槽存档管理器
// - 每 AUTO_SAVE_INTERVAL_SEC 秒自动存档
// - 两个存档槽轮替写入, 新存档覆写最旧的槽
// - 读取时按新到旧顺序尝试, 优先读最新, 新档坏则读老档
// ============================================

// 自动存档间隔 (秒), 可直接修改此值自定义
#define AUTO_SAVE_INTERVAL_SEC  300     // 默认5分钟

#define SAVE_SLOT_COUNT         2

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
    // 初始化 NVS, 扫描两个槽确定当前状态
    SaveResult init();

    // 存档 (写入最旧的槽), saveTime = 当前 unix timestamp
    SaveResult save(const PetState& pet, uint32_t saveTime);

    // 读档 (按新到旧顺序尝试)
    SaveResult load(PetState& pet);

    // 校验 sequence 最新的槽：header.save_time 须等于「本次 save() 传入的 saveTime」
    // （调用方应对同一次尝试传入与 save() 相同的 expectedEpoch；不在此函数内再次采样时钟，避免处理延迟误判）
    // 且内容与 pet 一致（checksum + 读回 memcmp）
    bool verifyLatestSave(uint32_t expectedEpoch, const PetState& pet);

    // 是否存在至少一个有效存档
    bool hasSave();

    // 删除所有存档 (销毁时用)
    SaveResult erase();

    // 检查是否需要自动存档
    bool shouldAutoSave(uint32_t currentTime);

    // 更新自动存档计时
    void markSaved(uint32_t currentTime);

    // 获取最近一次成功 load 的存档时间 (0 = 无存档/新游戏)
    uint32_t getLastSaveTime() const { return _loadedSaveTime; }

    // --- 图鉴存档 (独立 NVS key, 与主存档分离) ---
    SaveResult saveGallery(const GalleryData& gallery);
    SaveResult loadGallery(GalleryData& gallery);

private:
    bool _initialized = false;
    uint32_t _lastSaveTime = 0;
    uint32_t _nextSequence = 1;     // 下一个写入的序号
    uint32_t _loadedSaveTime = 0;   // load 时读取的存档时间戳

    // 每个槽的 NVS key
    static const char* slotHdrKey(uint8_t slot);
    static const char* slotDataKey(uint8_t slot);

    // 读取单个槽
    SaveResult loadSlot(uint8_t slot, PetState& pet);

    // 读取槽头 (不读数据, 仅用于判断新旧)
    bool readSlotHeader(uint8_t slot, SaveHeader& hdr);

    // 确定写入目标槽 (最旧的那个)
    uint8_t getOldestSlot();

    // 简单校验和
    uint16_t calcChecksum(const uint8_t* data, size_t len);
};

// 全局单例
extern SaveManager saveManager;

#endif // SAVE_MANAGER_H
