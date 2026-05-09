// src/core/save_manager.h

#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include "game_state.h"
#include "../pet/gallery.h"
#include <stdint.h>
#include <stddef.h>

// ============================================
// NVS 多槽存档管理器（物理槽 0/1/2）
// - 正常游戏使用“两槽轮转”：
//   * 默认使用槽 0 和槽 1
//   * 串口导入后：导入槽必入活跃槽对；其余两槽中“更旧”的一槽加入活跃槽对；
//     剩余一槽冻结保留，不参与后续自动/手动 save 覆写
// - 每 AUTO_SAVE_INTERVAL_SEC 秒自动存档，覆写当前活动槽对中 sequence 较旧的一槽
// - 写入后经读回校验，失败则不递增 sequence，并「粘滞」在该槽重复写入，直到校验成功
// - 读取时只在当前活动槽对中按新到旧顺序尝试，优先读最新；新槽 load 失败则退回另一槽的好档
// ============================================

// 自动存档间隔 (秒), 可直接修改此值自定义
#define AUTO_SAVE_INTERVAL_SEC  300     // 默认5分钟

#define SAVE_SLOT_COUNT         3

enum SaveResult : uint8_t {
    SAVE_OK = 0,
    SAVE_ERR_NVS_INIT,
    SAVE_ERR_WRITE,
    SAVE_ERR_READ,
    SAVE_ERR_NO_DATA,
    SAVE_ERR_VERSION_MISMATCH,
    SAVE_ERR_CHECKSUM,
    SAVE_ERR_CORRUPTED,
    SAVE_ERR_VERIFY  // NVS put 成功但读回与 pet/save_time/seq 不一致，未提交 sequence
};

class SaveManager {
public:
    // 初始化 NVS, 扫描三个槽确定当前状态
    SaveResult init();

    // 存档：目标槽通常为当前较旧的槽；若上次写入校验失败则会粘滞重试同一槽直至成功
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

    // --- 串口按槽导入/导出支持 ---
    SaveResult exportSlotRaw(uint8_t slot, SaveHeader& hdr, PetState& pet);
    // 导出带图鉴的按槽快照：优先使用该槽绑定图鉴；若缺失则回退为最小一致图鉴（只解锁当前形态）
    SaveResult exportSlotRawWithGallery(uint8_t slot, SaveHeader& hdr, PetState& pet,
                                        GalleryData& gallery, bool& galleryBoundToSlot);
    SaveResult importSlotRaw(uint8_t slot, const SaveHeader& hdr, const PetState& pet, bool forceNewest);
    SaveResult loadFromSlot(uint8_t slot, PetState& pet);
    bool getSlotStatus(uint8_t slot, SaveHeader& hdr, SaveResult& status);
    // 将当前全局图鉴快照绑定到指定槽（用于导入后补齐槽位图鉴快照）
    void syncGlobalGalleryToSlot(uint8_t slot);

    // --- 活动存档槽对配置（持久化在 NVS） ---
    void setActivePairForImportedBaseSlot(uint8_t baseSlot);
    void setImportTimeSetupRequired(bool required);
    bool isImportTimeSetupRequired();

private:
    bool _initialized = false;
    uint32_t _lastSaveTime = 0;
    uint32_t _nextSequence = 1;     // 下一个写入的序号（仅在校验成功后递增）
    uint32_t _loadedSaveTime = 0;   // load 时读取的存档时间戳
    uint8_t _stickyWriteSlot = 0xFF;  // >= SAVE_SLOT_COUNT 表示无粘滞；否则只在对应槽重写直到校验通过
    uint8_t _activeSlotA = 0;       // 当前活动槽对 A（默认 0）
    uint8_t _activeSlotB = 1;       // 当前活动槽对 B（默认 1）

    // 每个槽的 NVS key
    static const char* slotHdrKey(uint8_t slot);
    static const char* slotDataKey(uint8_t slot);
    static const char* slotGalleryKey(uint8_t slot);

    // 读取单个槽
    SaveResult loadSlot(uint8_t slot, PetState& pet);

    // 读取槽头 (不读数据, 仅用于判断新旧)
    bool readSlotHeader(uint8_t slot, SaveHeader& hdr);

    uint8_t getOldestSlot();
    uint8_t getSaveTargetSlot();
    bool verifyWrittenSlot(uint8_t slot, uint32_t saveTime, uint32_t expectedSeq, const PetState& pet);
    void resyncSequenceFromNvs();
    bool isSlotActive(uint8_t slot) const;
    void loadActivePairConfig();
    void persistActivePairConfig();
    bool loadSlotGallery(uint8_t slot, GalleryData& gallery);

    // 简单校验和
    uint16_t calcChecksum(const uint8_t* data, size_t len);
};

// 全局单例
extern SaveManager saveManager;

#endif // SAVE_MANAGER_H
