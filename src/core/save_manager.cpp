// src/core/save_manager.cpp

#include "save_manager.h"
#include "../config/game_config.h"
#include "../pet/gallery.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

SaveManager saveManager;

static Preferences prefs;

// NVS key 名称 (每个槽独立)
static const char* HDR_KEYS[SAVE_SLOT_COUNT]  = { "s0_hdr", "s1_hdr" };
static const char* DATA_KEYS[SAVE_SLOT_COUNT] = { "s0_dat", "s1_dat" };

const char* SaveManager::slotHdrKey(uint8_t slot) {
    return HDR_KEYS[slot];
}

const char* SaveManager::slotDataKey(uint8_t slot) {
    return DATA_KEYS[slot];
}

SaveResult SaveManager::init() {
    bool ok = prefs.begin(SAVE_NVS_NAMESPACE, false);
    if (!ok) {
        Serial.println("[Save] ERROR: NVS init failed");
        return SAVE_ERR_NVS_INIT;
    }

    _initialized = true;
    _lastSaveTime = 0;

    Serial.println("[Save] NVS initialized (dual-slot)");

    // 扫描两个槽, 确定最大 sequence 以便后续递增
    uint32_t maxSeq = 0;
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        if (readSlotHeader(i, hdr)) {
            validCount++;
            if (hdr.sequence > maxSeq) maxSeq = hdr.sequence;
            Serial.printf("[Save] Slot %d: seq=%lu, ver=%d\n", i, hdr.sequence, hdr.version);
        } else {
            Serial.printf("[Save] Slot %d: empty/invalid\n", i);
        }
    }

    _nextSequence = maxSeq + 1;

    if (validCount > 0)
        Serial.printf("[Save] Found %d valid slot(s), next seq=%lu\n", validCount, _nextSequence);
    else
        Serial.println("[Save] No existing save data");

    return SAVE_OK;
}

SaveResult SaveManager::save(const PetState& pet, uint32_t saveTime) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    uint8_t targetSlot = getOldestSlot();

    // 构建存档头
    SaveHeader header;
    header.version = SAVE_DATA_VERSION;
    header.data_size = sizeof(PetState);
    header.checksum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    header.sequence = _nextSequence;
    header.save_time = saveTime;

    // 写入头
    size_t written = prefs.putBytes(slotHdrKey(targetSlot), &header, sizeof(SaveHeader));
    if (written != sizeof(SaveHeader)) {
        Serial.printf("[Save] ERROR: Failed to write header to slot %d\n", targetSlot);
        return SAVE_ERR_WRITE;
    }

    // 写入数据
    written = prefs.putBytes(slotDataKey(targetSlot), &pet, sizeof(PetState));
    if (written != sizeof(PetState)) {
        Serial.printf("[Save] ERROR: Failed to write data to slot %d\n", targetSlot);
        return SAVE_ERR_WRITE;
    }

    Serial.printf("[Save] Slot %d saved (seq=%lu, %d bytes, crc=0x%04X)\n",
                  targetSlot, _nextSequence, (int)sizeof(PetState), header.checksum);

    _nextSequence++;
    return SAVE_OK;
}

SaveResult SaveManager::load(PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    // 收集有效槽的 header, 按 sequence 从大到小尝试
    struct SlotInfo {
        uint8_t slot;
        uint32_t sequence;
        bool valid;
    };
    SlotInfo slots[SAVE_SLOT_COUNT];

    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        slots[i].slot = i;
        slots[i].valid = readSlotHeader(i, hdr);
        slots[i].sequence = slots[i].valid ? hdr.sequence : 0;
    }

    // 简单排序: 按 sequence 降序 (新的在前)
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT - 1; i++) {
        for (uint8_t j = i + 1; j < SAVE_SLOT_COUNT; j++) {
            if (slots[j].sequence > slots[i].sequence) {
                SlotInfo tmp = slots[i];
                slots[i] = slots[j];
                slots[j] = tmp;
            }
        }
    }

    // 按新到旧顺序尝试加载
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!slots[i].valid) continue;

        Serial.printf("[Save] Trying slot %d (seq=%lu)...\n", slots[i].slot, slots[i].sequence);
        SaveResult r = loadSlot(slots[i].slot, pet);
        if (r == SAVE_OK) {
            // 读取该槽的 save_time
            SaveHeader hdr;
            if (readSlotHeader(slots[i].slot, hdr)) {
                _loadedSaveTime = hdr.save_time;
            }
            Serial.printf("[Save] Loaded from slot %d (save_time=%lu)\n", slots[i].slot, _loadedSaveTime);
            return SAVE_OK;
        }
        Serial.printf("[Save] Slot %d failed (err=%d), trying next...\n", slots[i].slot, r);
    }

    Serial.println("[Save] All slots failed");
    return SAVE_ERR_NO_DATA;
}

// expectedEpoch 须与触发本次校验前对 save() 传入的 saveTime 相同（不在此重采 timeManager）
bool SaveManager::verifyLatestSave(uint32_t expectedEpoch, const PetState& pet) {
    if (!_initialized) return false;

    uint8_t newestSlot = 0;
    uint32_t maxSeq = 0;
    bool any = false;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        if (!readSlotHeader(i, hdr)) continue;
        any = true;
        if (hdr.sequence >= maxSeq) {
            maxSeq = hdr.sequence;
            newestSlot = i;
        }
    }
    if (!any) return false;

    SaveHeader hdr;
    size_t readLen = prefs.getBytes(slotHdrKey(newestSlot), &hdr, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return false;
    if (hdr.version != SAVE_DATA_VERSION) return false;
    if (hdr.data_size != sizeof(PetState)) return false;
    if (hdr.save_time != expectedEpoch) return false;

    uint16_t petSum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    if (hdr.checksum != petSum) return false;

    PetState temp;
    SaveResult lr = loadSlot(newestSlot, temp);
    if (lr != SAVE_OK) return false;

    return memcmp(&temp, &pet, sizeof(PetState)) == 0;
}

SaveResult SaveManager::loadSlot(uint8_t slot, PetState& pet) {
    SaveHeader header;
    size_t readLen = prefs.getBytes(slotHdrKey(slot), &header, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return SAVE_ERR_NO_DATA;

    // 版本检查
    if (header.version != SAVE_DATA_VERSION) return SAVE_ERR_VERSION_MISMATCH;

    // 大小检查
    if (header.data_size != sizeof(PetState)) return SAVE_ERR_CORRUPTED;

    // 读取数据
    PetState temp;
    readLen = prefs.getBytes(slotDataKey(slot), &temp, sizeof(PetState));
    if (readLen != sizeof(PetState)) return SAVE_ERR_READ;

    // 校验
    uint16_t checksum = calcChecksum((const uint8_t*)&temp, sizeof(PetState));
    if (checksum != header.checksum) return SAVE_ERR_CHECKSUM;

    pet = temp;
    return SAVE_OK;
}

bool SaveManager::readSlotHeader(uint8_t slot, SaveHeader& hdr) {
    if (!_initialized) return false;
    size_t readLen = prefs.getBytes(slotHdrKey(slot), &hdr, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return false;
    if (hdr.version != SAVE_DATA_VERSION) return false;
    if (hdr.data_size != sizeof(PetState)) return false;
    return true;
}

uint8_t SaveManager::getOldestSlot() {
    // 找 sequence 最小的槽 (空槽 sequence=0, 优先被覆写)
    uint32_t minSeq = UINT32_MAX;
    uint8_t oldest = 0;

    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        uint32_t seq = 0;
        if (readSlotHeader(i, hdr)) {
            seq = hdr.sequence;
        }
        // 空槽 seq=0, 一定最小
        if (seq < minSeq) {
            minSeq = seq;
            oldest = i;
        }
    }
    return oldest;
}

bool SaveManager::hasSave() {
    if (!_initialized) return false;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        if (readSlotHeader(i, hdr)) return true;
    }
    return false;
}

SaveResult SaveManager::erase() {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    prefs.clear();
    _nextSequence = 1;
    Serial.println("[Save] All save data erased");

    return SAVE_OK;
}

bool SaveManager::shouldAutoSave(uint32_t currentTime) {
    if (_lastSaveTime == 0) return true;
    return (currentTime - _lastSaveTime) >= AUTO_SAVE_INTERVAL_SEC;
}

void SaveManager::markSaved(uint32_t currentTime) {
    _lastSaveTime = currentTime;
}

uint16_t SaveManager::calcChecksum(const uint8_t* data, size_t len) {
    uint16_t sum = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
        sum = (sum << 3) | (sum >> 13);  // rotate left 3
        sum += data[i];
    }
    return sum;
}

// ============================================================================
//  图鉴存档 (独立 NVS key, 与主存档分离)
// ============================================================================

static const char* GALLERY_NVS_KEY = "gallery";

SaveResult SaveManager::saveGallery(const GalleryData& gallery) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    size_t written = prefs.putBytes(GALLERY_NVS_KEY, &gallery, sizeof(GalleryData));
    if (written != sizeof(GalleryData)) {
        Serial.println("[Save] ERROR: Failed to write gallery data");
        return SAVE_ERR_WRITE;
    }

    Serial.printf("[Save] Gallery saved (%d bytes, %d/%d unlocked)\n",
                  (int)sizeof(GalleryData), gallery.getUnlockedCount(), FORM_COUNT);
    return SAVE_OK;
}

SaveResult SaveManager::loadGallery(GalleryData& gallery) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    size_t readLen = prefs.getBytes(GALLERY_NVS_KEY, &gallery, sizeof(GalleryData));
    if (readLen != sizeof(GalleryData)) {
        // 旧存档兼容: 没有图鉴数据, 安全初始化为空
        Serial.println("[Save] No gallery data found (old save?), initializing empty");
        gallery.init();
        return SAVE_ERR_NO_DATA;
    }

    Serial.printf("[Save] Gallery loaded (%d/%d unlocked)\n",
                  gallery.getUnlockedCount(), FORM_COUNT);
    return SAVE_OK;
}
