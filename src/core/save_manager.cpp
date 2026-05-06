// src/core/save_manager.cpp

#include "save_manager.h"
#include "../config/game_config.h"
#include <Arduino.h>
#include <Preferences.h>

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

SaveResult SaveManager::save(const PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    uint8_t targetSlot = getOldestSlot();

    // 构建存档头
    SaveHeader header;
    header.version = SAVE_DATA_VERSION;
    header.data_size = sizeof(PetState);
    header.checksum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    header.sequence = _nextSequence;

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
            Serial.printf("[Save] Loaded from slot %d\n", slots[i].slot);
            return SAVE_OK;
        }
        Serial.printf("[Save] Slot %d failed (err=%d), trying next...\n", slots[i].slot, r);
    }

    Serial.println("[Save] All slots failed");
    return SAVE_ERR_NO_DATA;
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
