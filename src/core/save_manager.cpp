// src/core/save_manager.cpp

#include "save_manager.h"
#include "../config/game_config.h"
#include "../pet/gallery.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

SaveManager saveManager;

static Preferences prefs;

// NVS key 名称 (slot0/1/2)
static const char* HDR_KEYS[SAVE_SLOT_COUNT]  = { "s0_hdr", "s1_hdr", "s2_hdr" };
static const char* DATA_KEYS[SAVE_SLOT_COUNT] = { "s0_dat", "s1_dat", "s2_dat" };
static const char* GALLERY_KEYS[SAVE_SLOT_COUNT] = { "s0_gal", "s1_gal", "s2_gal" };
static const char* GALLERY_NVS_KEY = "gallery";
static const char* ACTIVE_PAIR_KEY = "slot_cfg";
static const char* IMPORT_TIME_REQ_KEY = "imp_treq";

static bool isPetStateSemanticallyValid(const PetState& pet) {
    if (pet.form >= FORM_COUNT) return false;
    if (pet.base_form >= FORM_COUNT) return false;
    if (pet.alignment > ALIGN_BLACK) return false;
    if (pet.stage > STAGE_ADULT) return false;
    if (pet.health < HEALTH_MIN || pet.health > HEALTH_MAX) return false;
    if (pet.seriousness < SERIOUSNESS_MIN || pet.seriousness > SERIOUSNESS_MAX) return false;
    if (pet.white_fun_form >= FORM_COUNT) return false;
    return true;
}

const char* SaveManager::slotHdrKey(uint8_t slot) {
    return HDR_KEYS[slot];
}

const char* SaveManager::slotDataKey(uint8_t slot) {
    return DATA_KEYS[slot];
}

const char* SaveManager::slotGalleryKey(uint8_t slot) {
    return GALLERY_KEYS[slot];
}

SaveResult SaveManager::init() {
    bool ok = prefs.begin(SAVE_NVS_NAMESPACE, false);
    if (!ok) {
        Serial.println("[Save] ERROR: NVS init failed");
        return SAVE_ERR_NVS_INIT;
    }

    _initialized = true;
    _lastSaveTime = 0;
    _stickyWriteSlot = 0xFF;
    _activeSlotA = 0;
    _activeSlotB = 1;

    Serial.println("[Save] NVS initialized (multi-slot)");

    resyncSequenceFromNvs();
    loadActivePairConfig();

    return SAVE_OK;
}

void SaveManager::resyncSequenceFromNvs() {
    uint32_t maxSeq = 0;
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        if (readSlotHeader(i, hdr)) {
            validCount++;
            if (hdr.sequence > maxSeq) maxSeq = hdr.sequence;
            Serial.printf("[Save] 存档%u (slot %u): seq=%lu, ver=%d\n",
                          (unsigned)(i + 1), i, hdr.sequence, hdr.version);
        } else {
            Serial.printf("[Save] 存档%u (slot %u): empty/invalid\n", (unsigned)(i + 1), i);
        }
    }

    _nextSequence = maxSeq + 1;
    if (validCount > 0)
        Serial.printf("[Save] Found %d valid slot(s), next seq=%lu\n", validCount, _nextSequence);
    else
        Serial.println("[Save] No existing save data");
}

bool SaveManager::isSlotActive(uint8_t slot) const {
    return (slot == _activeSlotA) || (slot == _activeSlotB);
}

void SaveManager::loadActivePairConfig() {
    struct PairCfg {
        uint8_t a;
        uint8_t b;
        uint8_t reserved[2];
    } cfg{};

    size_t readLen = prefs.getBytes(ACTIVE_PAIR_KEY, &cfg, sizeof(cfg));
    if (readLen == sizeof(cfg) &&
        cfg.a < SAVE_SLOT_COUNT &&
        cfg.b < SAVE_SLOT_COUNT &&
        cfg.a != cfg.b) {
        _activeSlotA = cfg.a;
        _activeSlotB = cfg.b;
    } else {
        _activeSlotA = 0;
        _activeSlotB = 1;
        persistActivePairConfig();
    }
    Serial.printf("[Save] Active slots: %u,%u\n", _activeSlotA, _activeSlotB);
}

void SaveManager::persistActivePairConfig() {
    struct PairCfg {
        uint8_t a;
        uint8_t b;
        uint8_t reserved[2];
    } cfg{};
    cfg.a = _activeSlotA;
    cfg.b = _activeSlotB;
    prefs.putBytes(ACTIVE_PAIR_KEY, &cfg, sizeof(cfg));
}

SaveResult SaveManager::save(const PetState& pet, uint32_t saveTime) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    uint8_t targetSlot = getSaveTargetSlot();

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
        Serial.printf("[Save] ERROR: Failed to write header to 存档%u (slot %u)\n",
                      (unsigned)(targetSlot + 1), targetSlot);
        return SAVE_ERR_WRITE;
    }

    // 写入数据
    written = prefs.putBytes(slotDataKey(targetSlot), &pet, sizeof(PetState));
    if (written != sizeof(PetState)) {
        Serial.printf("[Save] ERROR: Failed to write data to 存档%u (slot %u)\n",
                      (unsigned)(targetSlot + 1), targetSlot);
        return SAVE_ERR_WRITE;
    }

    if (!verifyWrittenSlot(targetSlot, saveTime, _nextSequence, pet)) {
        _stickyWriteSlot = targetSlot;
        Serial.printf("[Save] VERIFY FAIL: 存档%u (slot %u) data invalid after write; "
                      "will retry this slot until OK (seq still %lu)\n",
                      (unsigned)(targetSlot + 1), targetSlot, _nextSequence);
        return SAVE_ERR_VERIFY;
    }

    _stickyWriteSlot = 0xFF;
    Serial.printf("[Save] 存档%u (slot %u) saved (seq=%lu, %d bytes, crc=0x%04X)\n",
                  (unsigned)(targetSlot + 1), targetSlot, _nextSequence,
                  (int)sizeof(PetState), header.checksum);
    syncGlobalGalleryToSlot(targetSlot);

    _nextSequence++;
    return SAVE_OK;
}

SaveResult SaveManager::load(PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    // 先全槽扫描：若“有且仅有一个有效槽”，固定从该槽加载并重建活动槽对
    struct FullSlotInfo {
        bool valid;
        SaveHeader hdr;
    };
    FullSlotInfo full[SAVE_SLOT_COUNT];
    uint8_t validCount = 0;
    uint8_t onlyValidSlot = 0xFF;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader h;
        bool ok = readSlotHeader(i, h);
        full[i].valid = ok;
        if (ok) {
            full[i].hdr = h;
            validCount++;
            onlyValidSlot = i;
        }
    }
    if (validCount == 1 && onlyValidSlot < SAVE_SLOT_COUNT) {
        Serial.printf("[Save] Only one valid slot found: %u. Forcing load from it.\n", onlyValidSlot);
        setActivePairForImportedBaseSlot(onlyValidSlot);
        SaveResult r = loadSlot(onlyValidSlot, pet);
        if (r == SAVE_OK) {
            _loadedSaveTime = full[onlyValidSlot].hdr.save_time;
            Serial.printf("[Save] Loaded from only valid slot %u (save_time=%lu)\n",
                          onlyValidSlot, _loadedSaveTime);
            return SAVE_OK;
        }
        Serial.printf("[Save] Only valid slot %u failed to load (err=%d)\n", onlyValidSlot, r);
        return r;
    }

    // 收集活动槽对中的有效槽头，按 sequence 从大到小尝试
    struct SlotInfo {
        uint8_t slot;
        uint32_t sequence;
        bool valid;
    };
    SlotInfo slots[SAVE_SLOT_COUNT];

    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        slots[i].slot = i;
        if (!isSlotActive(i)) {
            slots[i].valid = false;
            slots[i].sequence = 0;
            continue;
        }
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

    // 按新到旧顺序尝试加载（活动槽对）
    bool anyActiveValid = false;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!slots[i].valid) continue;
        anyActiveValid = true;

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

    // 兜底：活动槽对失败后，扫描全槽尝试恢复（配置异常/历史残留时避免“有档读不到”）
    Serial.println("[Save] Active-pair load failed, trying full-slot fallback...");
    struct FallbackInfo {
        uint8_t slot;
        uint32_t sequence;
        bool valid;
    };
    FallbackInfo fb[SAVE_SLOT_COUNT];
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        fb[i].slot = i;
        fb[i].valid = readSlotHeader(i, hdr);
        fb[i].sequence = fb[i].valid ? hdr.sequence : 0;
    }
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT - 1; i++) {
        for (uint8_t j = i + 1; j < SAVE_SLOT_COUNT; j++) {
            if (fb[j].sequence > fb[i].sequence) {
                FallbackInfo tmp = fb[i];
                fb[i] = fb[j];
                fb[j] = tmp;
            }
        }
    }
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!fb[i].valid) continue;
        SaveResult r = loadSlot(fb[i].slot, pet);
        if (r == SAVE_OK) {
            SaveHeader hdr;
            if (readSlotHeader(fb[i].slot, hdr)) {
                _loadedSaveTime = hdr.save_time;
            }
            Serial.printf("[Save] Fallback loaded slot %d (seq=%lu, save_time=%lu)\n",
                          fb[i].slot, fb[i].sequence, _loadedSaveTime);
            // 自动修复活动槽对：以该槽为导入基槽套用通用规则
            setActivePairForImportedBaseSlot(fb[i].slot);
            return SAVE_OK;
        }
        Serial.printf("[Save] Fallback slot %d failed (err=%d)\n", fb[i].slot, r);
    }

    if (!anyActiveValid) {
        Serial.println("[Save] Active pair had no valid header, and fallback found no recoverable save");
    } else {
        Serial.println("[Save] Active pair had headers but all loads failed; fallback also failed");
    }
    return SAVE_ERR_NO_DATA;
}

// expectedEpoch 须与触发本次校验前对 save() 传入的 saveTime 相同（不在此重采 timeManager）
bool SaveManager::verifyLatestSave(uint32_t expectedEpoch, const PetState& pet) {
    if (!_initialized) return false;

    uint8_t newestSlot = 0;
    uint32_t maxSeq = 0;
    bool any = false;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!isSlotActive(i)) continue;
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
    const char* hdrKey = slotHdrKey(slot);
    const char* datKey = slotDataKey(slot);
    if (!prefs.isKey(hdrKey) || !prefs.isKey(datKey)) return SAVE_ERR_NO_DATA;

    size_t readLen = prefs.getBytes(hdrKey, &header, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return SAVE_ERR_NO_DATA;

    // 版本检查
    if (header.version != SAVE_DATA_VERSION) return SAVE_ERR_VERSION_MISMATCH;

    // 大小检查
    if (header.data_size != sizeof(PetState)) return SAVE_ERR_CORRUPTED;

    // 读取数据
    PetState temp;
    readLen = prefs.getBytes(datKey, &temp, sizeof(PetState));
    if (readLen != sizeof(PetState)) return SAVE_ERR_READ;

    // 校验
    uint16_t checksum = calcChecksum((const uint8_t*)&temp, sizeof(PetState));
    if (checksum != header.checksum) return SAVE_ERR_CHECKSUM;

    pet = temp;
    return SAVE_OK;
}

SaveResult SaveManager::loadFromSlot(uint8_t slot, PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if (slot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    SaveResult r = loadSlot(slot, pet);
    if (r != SAVE_OK) return r;

    SaveHeader hdr;
    if (!readSlotHeader(slot, hdr)) return SAVE_ERR_CORRUPTED;
    _loadedSaveTime = hdr.save_time;
    return SAVE_OK;
}

SaveResult SaveManager::exportSlotRaw(uint8_t slot, SaveHeader& hdr, PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if (slot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    const char* hdrKey = slotHdrKey(slot);
    const char* datKey = slotDataKey(slot);
    if (!prefs.isKey(hdrKey) || !prefs.isKey(datKey)) return SAVE_ERR_NO_DATA;

    size_t readLen = prefs.getBytes(hdrKey, &hdr, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return SAVE_ERR_NO_DATA;
    if (hdr.version != SAVE_DATA_VERSION) return SAVE_ERR_VERSION_MISMATCH;
    if (hdr.data_size != sizeof(PetState)) return SAVE_ERR_CORRUPTED;

    readLen = prefs.getBytes(datKey, &pet, sizeof(PetState));
    if (readLen != sizeof(PetState)) return SAVE_ERR_READ;

    uint16_t checksum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    if (checksum != hdr.checksum) return SAVE_ERR_CHECKSUM;
    return SAVE_OK;
}

SaveResult SaveManager::exportSlotRawWithGallery(uint8_t slot, SaveHeader& hdr, PetState& pet,
                                                 GalleryData& gallery, bool& galleryBoundToSlot) {
    SaveResult r = exportSlotRaw(slot, hdr, pet);
    if (r != SAVE_OK) return r;

    if (loadSlotGallery(slot, gallery)) {
        galleryBoundToSlot = true;
        return SAVE_OK;
    }

    // 旧版本没有槽位图鉴快照时，构造最小一致图鉴，避免“旧宠物+新图鉴”混搭
    gallery.init();
    gallery.unlock(pet.form);
    galleryBoundToSlot = false;
    return SAVE_OK;
}

bool SaveManager::getSlotStatus(uint8_t slot, SaveHeader& hdr, SaveResult& status) {
    if (!_initialized) {
        status = SAVE_ERR_NVS_INIT;
        return false;
    }
    if (slot >= SAVE_SLOT_COUNT) {
        status = SAVE_ERR_CORRUPTED;
        return false;
    }
    PetState temp;
    status = exportSlotRaw(slot, hdr, temp);
    return status == SAVE_OK;
}

SaveResult SaveManager::importSlotRaw(uint8_t slot, const SaveHeader& hdrIn, const PetState& pet, bool forceNewest) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if (slot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    if (hdrIn.version != SAVE_DATA_VERSION) return SAVE_ERR_VERSION_MISMATCH;
    if (hdrIn.data_size != sizeof(PetState)) return SAVE_ERR_CORRUPTED;

    uint16_t petSum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    if (hdrIn.checksum != petSum) return SAVE_ERR_CHECKSUM;
    if (!isPetStateSemanticallyValid(pet)) return SAVE_ERR_CORRUPTED;

    SaveHeader toWrite = hdrIn;
    if (forceNewest) {
        resyncSequenceFromNvs();
        toWrite.sequence = _nextSequence;
    }

    size_t written = prefs.putBytes(slotHdrKey(slot), &toWrite, sizeof(SaveHeader));
    if (written != sizeof(SaveHeader)) return SAVE_ERR_WRITE;
    written = prefs.putBytes(slotDataKey(slot), &pet, sizeof(PetState));
    if (written != sizeof(PetState)) return SAVE_ERR_WRITE;

    if (!verifyWrittenSlot(slot, toWrite.save_time, toWrite.sequence, pet)) return SAVE_ERR_VERIFY;

    _stickyWriteSlot = 0xFF;
    resyncSequenceFromNvs();
    return SAVE_OK;
}

void SaveManager::setActivePairForImportedBaseSlot(uint8_t baseSlot) {
    if (baseSlot >= SAVE_SLOT_COUNT) {
        return;
    }

    // 通用规则：
    // 1) 导入槽 baseSlot 必入活跃槽对
    // 2) 在另外两个槽中选“更旧”的那个入活跃槽对（后续轮换写入）
    // 3) 另一个槽冻结（不参与自动/手动 save 轮换）
    uint8_t others[2];
    uint8_t k = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (i == baseSlot) continue;
        others[k++] = i;
    }

    struct SlotAgeInfo {
        uint8_t slot;
        bool valid;
        uint32_t seq;
        uint32_t saveTime;
    };

    auto readAge = [&](uint8_t slot) -> SlotAgeInfo {
        SaveHeader hdr;
        bool ok = readSlotHeader(slot, hdr);
        SlotAgeInfo info{};
        info.slot = slot;
        info.valid = ok;
        info.seq = ok ? hdr.sequence : 0;
        info.saveTime = ok ? hdr.save_time : 0;
        return info;
    };

    auto olderThan = [&](const SlotAgeInfo& a, const SlotAgeInfo& b) -> bool {
        // 空槽/无效槽视为更旧
        if (a.valid != b.valid) return !a.valid;
        if (!a.valid && !b.valid) return a.slot < b.slot;  // 都无效，固定序兜底

        if (a.seq != b.seq) return a.seq < b.seq;          // sequence 小者更旧
        if (a.saveTime != b.saveTime) return a.saveTime < b.saveTime;
        return a.slot < b.slot;                              // 完全相同时固定序
    };

    SlotAgeInfo o0 = readAge(others[0]);
    SlotAgeInfo o1 = readAge(others[1]);
    uint8_t older = olderThan(o0, o1) ? o0.slot : o1.slot;
    uint8_t frozen = (older == others[0]) ? others[1] : others[0];

    _activeSlotA = baseSlot;
    _activeSlotB = older;
    persistActivePairConfig();
    Serial.printf("[Save] Active slots updated after import: %u,%u (frozen=%u)\n",
                  _activeSlotA, _activeSlotB, frozen);
}

void SaveManager::setImportTimeSetupRequired(bool required) {
    uint8_t v = required ? 1 : 0;
    prefs.putUChar(IMPORT_TIME_REQ_KEY, v);
    Serial.printf("[Save] Import-time setup required: %s\n", required ? "YES" : "NO");
}

bool SaveManager::isImportTimeSetupRequired() {
    uint8_t v = prefs.getUChar(IMPORT_TIME_REQ_KEY, 0);
    return v == 1;
}

bool SaveManager::readSlotHeader(uint8_t slot, SaveHeader& hdr) {
    if (!_initialized) return false;
    const char* hdrKey = slotHdrKey(slot);
    if (!prefs.isKey(hdrKey)) return false;
    size_t readLen = prefs.getBytes(hdrKey, &hdr, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return false;
    if (hdr.version != SAVE_DATA_VERSION) return false;
    if (hdr.data_size != sizeof(PetState)) return false;
    return true;
}

bool SaveManager::loadSlotGallery(uint8_t slot, GalleryData& gallery) {
    if (!_initialized) return false;
    if (slot >= SAVE_SLOT_COUNT) return false;

    const char* key = slotGalleryKey(slot);
    if (!prefs.isKey(key)) return false;
    size_t readLen = prefs.getBytes(key, &gallery, sizeof(GalleryData));
    return readLen == sizeof(GalleryData);
}

void SaveManager::syncGlobalGalleryToSlot(uint8_t slot) {
    if (!_initialized) return;
    if (slot >= SAVE_SLOT_COUNT) return;

    GalleryData g;
    size_t readLen = prefs.getBytes(GALLERY_NVS_KEY, &g, sizeof(GalleryData));
    if (readLen != sizeof(GalleryData)) {
        g.init();
    }
    prefs.putBytes(slotGalleryKey(slot), &g, sizeof(GalleryData));
}

uint8_t SaveManager::getOldestSlot() {
    // 在当前活动槽对中找 sequence 最小的槽 (空槽 sequence=0, 优先被覆写)
    uint32_t minSeq = UINT32_MAX;
    uint8_t oldest = 0;

    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!isSlotActive(i)) continue;
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

uint8_t SaveManager::getSaveTargetSlot() {
    if (_stickyWriteSlot < SAVE_SLOT_COUNT)
        return _stickyWriteSlot;
    return getOldestSlot();
}

bool SaveManager::verifyWrittenSlot(uint8_t slot, uint32_t saveTime, uint32_t expectedSeq,
                                    const PetState& pet) {
    SaveHeader hdr;
    size_t readLen = prefs.getBytes(slotHdrKey(slot), &hdr, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return false;
    if (hdr.version != SAVE_DATA_VERSION) return false;
    if (hdr.data_size != sizeof(PetState)) return false;
    if (hdr.sequence != expectedSeq) return false;
    if (hdr.save_time != saveTime) return false;

    uint16_t petSum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    if (hdr.checksum != petSum) return false;

    PetState temp;
    if (loadSlot(slot, temp) != SAVE_OK) return false;
    return memcmp(&temp, &pet, sizeof(PetState)) == 0;
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
    _stickyWriteSlot = 0xFF;
    _activeSlotA = 0;
    _activeSlotB = 1;
    persistActivePairConfig();
    setImportTimeSetupRequired(false);
    Serial.println("[Save] All save data erased");
    Serial.println("[Save] Active slots reset to default: 0,1");

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
