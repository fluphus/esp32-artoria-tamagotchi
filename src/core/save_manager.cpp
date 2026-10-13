// src/core/save_manager.cpp

#include "save_manager.h"
#include "../config/game_config.h"
#include "../pet/gallery.h"
#include "save_io.h"
#include <string.h>
#include <initializer_list>
#include <inttypes.h>

SaveManager saveManager;

static auto& prefs=saveio::storage;

// NVS key 名称 (slot0/1/2)
static const char* HDR_KEYS[SAVE_SLOT_COUNT]  = { "s0_hdr", "s1_hdr", "s2_hdr" };
static const char* DATA_KEYS[SAVE_SLOT_COUNT] = { "s0_dat", "s1_dat", "s2_dat" };
static const char* GALLERY_KEYS[SAVE_SLOT_COUNT] = { "s0_gal", "s1_gal", "s2_gal" };
static const char* GALLERY_NVS_KEY = "gallery";
static const char* ACTIVE_PAIR_KEY = "slot_cfg";
static const char* DEVICE_STATE_KEY = "dev_state";

static bool isPetStateSemanticallyValid(const PetState& pet) {
    const auto* bytes=reinterpret_cast<const uint8_t*>(&pet);
    for(const size_t offset:{offsetof(PetState,is_rhongomyniad),offsetof(PetState,white_fun_form_locked),
        offsetof(PetState,is_black_rhongomyniad),offsetof(PetState,is_nobu),offsetof(PetState,is_oda_nobunaga)})
        if(bytes[offset]>1)return false;
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
    _initialized=false;_recoveryRequired=true;
    bool ok = prefs.begin(SAVE_NVS_NAMESPACE, false);
    if (!ok) {
        saveio::log.println("[Save] ERROR: NVS init failed");
        return SAVE_ERR_NVS_INIT;
    }

    _initialized = true;
    _operation=false;_frozenOwner=0xff;
    _recoveryRequired=hasSave();
    DeviceState device;device.init();
    if(loadDeviceState(device)==SAVE_OK&&device.is_visiting)_frozenOwner=device.owner_frozen_slot;
    _lastSaveTime = 0;
    _stickyWriteSlot = 0xFF;
    _activeSlotA = 0;
    _activeSlotB = 1;

    saveio::log.println("[Save] NVS initialized (multi-slot)");

    resyncSequenceFromNvs();
    return loadActivePairConfig();
}

void SaveManager::resyncSequenceFromNvs() {
    uint32_t maxSeq = 0;
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        if (readSlotHeader(i, hdr)) {
            validCount++;
            if (hdr.sequence > maxSeq) maxSeq = hdr.sequence;
            saveio::log.printf("[Save] 存档%u (slot %u): seq=%" PRIu32 ", ver=%d\n",
                          (unsigned)(i + 1), i, hdr.sequence, hdr.version);
        } else {
            saveio::log.printf("[Save] 存档%u (slot %u): empty/invalid\n", (unsigned)(i + 1), i);
        }
    }

    _nextSequence = maxSeq == UINT32_MAX ? 0 : maxSeq + 1;
    if (validCount > 0)
        saveio::log.printf("[Save] Found %d valid slot(s), next seq=%" PRIu32 "\n", validCount, _nextSequence);
    else
        saveio::log.println("[Save] No existing save data");
}

bool SaveManager::isSlotActive(uint8_t slot) const {
    return (slot == _activeSlotA) || (slot == _activeSlotB);
}

SaveResult SaveManager::loadActivePairConfig() {
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
        if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    }
    saveio::log.printf("[Save] Active slots: %u,%u\n", _activeSlotA, _activeSlotB);
    return SAVE_OK;
}

SaveResult SaveManager::persistActivePairConfig() {
    struct PairCfg {
        uint8_t a;
        uint8_t b;
        uint8_t reserved[2];
    } cfg{};
    cfg.a = _activeSlotA;
    cfg.b = _activeSlotB;
    return prefs.putBytes(ACTIVE_PAIR_KEY, &cfg, sizeof(cfg))==sizeof(cfg)?SAVE_OK:SAVE_ERR_WRITE;
}

SaveResult SaveManager::save(const PetState& pet, uint32_t saveTime) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    if(_recoveryRequired)return SAVE_ERR_CORRUPTED;
    if(_nextSequence==0){saveio::log.println("[Save] Sequence exhausted; records preserved, export before explicit recovery.");return SAVE_ERR_WRITE;}
    uint8_t targetSlot = getSaveTargetSlot();
    if(targetSlot>=SAVE_SLOT_COUNT)return SAVE_ERR_CORRUPTED;

    // 构建存档头
    SaveHeader header{};
    header.version = SAVE_DATA_VERSION;
    header.data_size = sizeof(PetState);
    header.checksum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    header.sequence = _nextSequence;
    header.save_time = saveTime;

    // 写入头
    size_t written = prefs.putBytes(slotHdrKey(targetSlot), &header, sizeof(SaveHeader));
    if (written != sizeof(SaveHeader)) {
        saveio::log.printf("[Save] ERROR: Failed to write header to 存档%u (slot %u)\n",
                      (unsigned)(targetSlot + 1), targetSlot);
        return SAVE_ERR_WRITE;
    }

    // 写入数据
    written = prefs.putBytes(slotDataKey(targetSlot), &pet, sizeof(PetState));
    if (written != sizeof(PetState)) {
        saveio::log.printf("[Save] ERROR: Failed to write data to 存档%u (slot %u)\n",
                      (unsigned)(targetSlot + 1), targetSlot);
        return SAVE_ERR_WRITE;
    }

    if (!verifyWrittenSlot(targetSlot, saveTime, _nextSequence, pet)) {
        _stickyWriteSlot = targetSlot;
        saveio::log.printf("[Save] VERIFY FAIL: 存档%u (slot %u) data invalid after write; "
                      "will retry this slot until OK (seq still %" PRIu32 ")\n",
                      (unsigned)(targetSlot + 1), targetSlot, _nextSequence);
        return SAVE_ERR_VERIFY;
    }

    _stickyWriteSlot = 0xFF;
    saveio::log.printf("[Save] 存档%u (slot %u) saved (seq=%" PRIu32 ", %d bytes, crc=0x%04X)\n",
                  (unsigned)(targetSlot + 1), targetSlot, _nextSequence,
                  (int)sizeof(PetState), header.checksum);
    const auto galleryResult=syncGlobalGalleryToSlot(targetSlot);
    if(galleryResult!=SAVE_OK)return galleryResult;

    _nextSequence++;
    return SAVE_OK;
}

SaveResult SaveManager::load(PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    _recoveryRequired=true;
    if(operationPending())return SAVE_ERR_CORRUPTED;
    DeviceState device;device.init();
    auto result=loadDeviceState(device);
    if(result!=SAVE_OK&&result!=SAVE_ERR_NO_DATA)return result;
    GalleryData gallery;gallery.init();result=loadGallery(gallery);
    if(result!=SAVE_OK&&result!=SAVE_ERR_NO_DATA)return result;
    _frozenOwner=device.is_visiting?device.owner_frozen_slot:0xff;
    if(device.is_visiting&&setActivePairForVisit(_frozenOwner)!=SAVE_OK)return SAVE_ERR_WRITE;
    const auto readable=[&](uint8_t slot){return slot!=_frozenOwner;};

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
        PetState candidate;
        bool ok = readable(i) && readSlotHeader(i, h) && loadSlot(i,candidate)==SAVE_OK;
        full[i].valid = ok;
        if (ok) {
            full[i].hdr = h;
            validCount++;
            onlyValidSlot = i;
        }
    }
    if (validCount == 1 && onlyValidSlot < SAVE_SLOT_COUNT) {
        saveio::log.printf("[Save] Only one valid slot found: %u. Forcing load from it.\n", onlyValidSlot);
        if(!device.is_visiting&&setActivePairForImportedBaseSlot(onlyValidSlot)!=SAVE_OK)return SAVE_ERR_WRITE;
        SaveResult r = loadSlot(onlyValidSlot, pet);
        if (r == SAVE_OK) {
            _loadedSaveTime = full[onlyValidSlot].hdr.save_time;
            saveio::log.printf("[Save] Loaded from only valid slot %u (save_time=%" PRIu32 ")\n",
                          onlyValidSlot, _loadedSaveTime);
            _recoveryRequired=false;
            return SAVE_OK;
        }
        saveio::log.printf("[Save] Only valid slot %u failed to load (err=%d)\n", onlyValidSlot, r);
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
        if (!isSlotActive(i)||!readable(i)) {
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

        saveio::log.printf("[Save] Trying slot %d (seq=%" PRIu32 ")...\n", slots[i].slot, slots[i].sequence);
        SaveResult r = loadSlot(slots[i].slot, pet);
        if (r == SAVE_OK) {
            // 读取该槽的 save_time
            SaveHeader hdr;
            if (readSlotHeader(slots[i].slot, hdr)) {
                _loadedSaveTime = hdr.save_time;
            }
            saveio::log.printf("[Save] Loaded from slot %d (save_time=%" PRIu32 ")\n", slots[i].slot, _loadedSaveTime);
            _recoveryRequired=false;
            return SAVE_OK;
        }
        saveio::log.printf("[Save] Slot %d failed (err=%d), trying next...\n", slots[i].slot, r);
    }

    // 兜底：活动槽对失败后，扫描全槽尝试恢复（配置异常/历史残留时避免“有档读不到”）
    saveio::log.println("[Save] Active-pair load failed, trying full-slot fallback...");
    struct FallbackInfo {
        uint8_t slot;
        uint32_t sequence;
        bool valid;
    };
    FallbackInfo fb[SAVE_SLOT_COUNT];
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        fb[i].slot = i;
        fb[i].valid = readable(i) && readSlotHeader(i, hdr);
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
            saveio::log.printf("[Save] Fallback loaded slot %d (seq=%" PRIu32 ", save_time=%" PRIu32 ")\n",
                          fb[i].slot, fb[i].sequence, _loadedSaveTime);
            // 自动修复活动槽对：以该槽为导入基槽套用通用规则
            if(!device.is_visiting&&setActivePairForImportedBaseSlot(fb[i].slot)!=SAVE_OK)return SAVE_ERR_WRITE;
            _recoveryRequired=false;
            return SAVE_OK;
        }
        saveio::log.printf("[Save] Fallback slot %d failed (err=%d)\n", fb[i].slot, r);
    }

    if (!anyActiveValid) {
        saveio::log.println("[Save] Active pair had no valid header, and fallback found no recoverable save");
    } else {
        saveio::log.println("[Save] Active pair had headers but all loads failed; fallback also failed");
    }
    return SAVE_ERR_NO_DATA;
}

// expectedEpoch 须与触发本次校验前对 save() 传入的 saveTime 相同（不在此重采 timeManager）
bool SaveManager::verifyLatestSave(uint32_t expectedEpoch, const PetState& pet) {
    if (!_initialized) return false;

    // 与 load() 活动槽排序一致: sequence 降序, 同序取槽号较小者 (load 的稳定排序先试小槽)
    uint8_t newestSlot = 0xFF;
    uint32_t maxSeq = 0;
    bool any = false;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!isSlotActive(i)) continue;
        SaveHeader hdr;
        if (!readSlotHeader(i, hdr)) continue;
        if (!any) {
            any = true;
            maxSeq = hdr.sequence;
            newestSlot = i;
            continue;
        }
        if (hdr.sequence > maxSeq || (hdr.sequence == maxSeq && i < newestSlot)) {
            maxSeq = hdr.sequence;
            newestSlot = i;
        }
    }
    if (!any || newestSlot >= SAVE_SLOT_COUNT) return false;

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
    if(slot>=SAVE_SLOT_COUNT)return SAVE_ERR_CORRUPTED;
    const char* newer[3]={"s0_v10","s1_v10","s2_v10"};
    if(prefs.isKey(newer[slot]))return SAVE_ERR_VERSION_MISMATCH;
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

    if(!isPetStateSemanticallyValid(temp))return SAVE_ERR_CORRUPTED;
    GalleryData gallery;
    if(prefs.isKey(slotGalleryKey(slot))&&!loadSlotGallery(slot,gallery))return SAVE_ERR_CORRUPTED;
    pet = temp;
    return SAVE_OK;
}

SaveResult SaveManager::loadFromSlot(uint8_t slot, PetState& pet) {
    if(operationPending()&&!_operation)return SAVE_ERR_CORRUPTED;
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if (slot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    SaveResult r = loadSlot(slot, pet);
    if (r != SAVE_OK) return r;

    SaveHeader hdr;
    if (!readSlotHeader(slot, hdr)) return SAVE_ERR_CORRUPTED;
    _loadedSaveTime = hdr.save_time;
    _recoveryRequired=false;
    return SAVE_OK;
}

SaveResult SaveManager::exportSlotRaw(uint8_t slot, SaveHeader& hdr, PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if (slot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;
    PetState checked;const auto validation=loadSlot(slot,checked);
    if(validation!=SAVE_OK)return validation;

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

SaveResult SaveManager::validateImport(const SaveHeader& hdrIn,const PetState& pet) {
    if (hdrIn.version != SAVE_DATA_VERSION) return SAVE_ERR_VERSION_MISMATCH;
    if (hdrIn.data_size != sizeof(PetState)) return SAVE_ERR_CORRUPTED;

    uint16_t petSum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    if (hdrIn.checksum != petSum) return SAVE_ERR_CHECKSUM;
    if (!isPetStateSemanticallyValid(pet)) return SAVE_ERR_CORRUPTED;

    return SAVE_OK;
}

SaveResult SaveManager::importSlotRaw(uint8_t slot, const SaveHeader& hdrIn, const PetState& pet, bool forceNewest) {
    if((operationPending()&&!_operation)||slot==_frozenOwner)return SAVE_ERR_CORRUPTED;
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if (slot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    const auto validation=validateImport(hdrIn,pet);
    if(validation!=SAVE_OK)return validation;

    SaveHeader toWrite = hdrIn;
    if (forceNewest) {
        resyncSequenceFromNvs();
        if(_nextSequence==0)return SAVE_ERR_WRITE;
        toWrite.sequence = _nextSequence;
    }

    size_t written = prefs.putBytes(slotHdrKey(slot), &toWrite, sizeof(SaveHeader));
    if (written != sizeof(SaveHeader)) return SAVE_ERR_WRITE;
    written = prefs.putBytes(slotDataKey(slot), &pet, sizeof(PetState));
    if (written != sizeof(PetState)) return SAVE_ERR_WRITE;

    if (!verifyWrittenSlot(slot, toWrite.save_time, toWrite.sequence, pet)) return SAVE_ERR_VERIFY;

    _stickyWriteSlot = 0xFF;
    resyncSequenceFromNvs();
    _recoveryRequired=false;
    return SAVE_OK;
}

SaveResult SaveManager::setActivePairForImportedBaseSlot(uint8_t baseSlot) {
    if (baseSlot >= SAVE_SLOT_COUNT) {
        return SAVE_ERR_CORRUPTED;
    }

    // 导入完整备份时：不需要保留现有存档
    // 导入槽 baseSlot 必入活跃槽对
    // 在另外两个槽中选"更旧"的那个入活跃槽对（后续轮换写入）
    // 另一个槽冻结（不参与自动/手动 save 轮换）
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
        if (a.valid != b.valid) return !a.valid;
        if (!a.valid && !b.valid) return a.slot < b.slot;
        if (a.seq != b.seq) return a.seq < b.seq;
        if (a.saveTime != b.saveTime) return a.saveTime < b.saveTime;
        return a.slot < b.slot;
    };

    SlotAgeInfo o0 = readAge(others[0]);
    SlotAgeInfo o1 = readAge(others[1]);
    uint8_t older = olderThan(o0, o1) ? o0.slot : o1.slot;
    uint8_t frozen = (older == others[0]) ? others[1] : others[0];

    _activeSlotA = baseSlot;
    _activeSlotB = older;
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    saveio::log.printf("[Save] Active slots updated after import: %u,%u (frozen=%u)\n",
                  _activeSlotA, _activeSlotB, frozen);
    return SAVE_OK;
}

SaveResult SaveManager::setActivePairForVisit(uint8_t ownerFrozenSlot) {
    if (ownerFrozenSlot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    _frozenOwner=ownerFrozenSlot;
    // 串门：冻结主人存档所在槽，其余两槽作为访客宠物的轮转写入槽
    uint8_t others[2];
    uint8_t k = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (i == ownerFrozenSlot) continue;
        others[k++] = i;
    }
    _activeSlotA = others[0];
    _activeSlotB = others[1];
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    saveio::log.printf("[Save] Active slots for visit: %u,%u (owner frozen=%u)\n",
                  _activeSlotA, _activeSlotB, ownerFrozenSlot);
    return SAVE_OK;
}

SaveResult SaveManager::setActivePairAfterVisitEnd(uint8_t ownerSlot) {
    if (ownerSlot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    // 串门结束：主人存档槽回到活跃对，另一个槽选更旧的
    const auto result=setActivePairForImportedBaseSlot(ownerSlot);
    if(result!=SAVE_OK)return result;
    saveio::log.printf("[Save] Active slots restored after visit end (owner=%u)\n", ownerSlot);
    return SAVE_OK;
}

SaveResult SaveManager::setActivePair(uint8_t a, uint8_t b) {
    if (a >= SAVE_SLOT_COUNT || b >= SAVE_SLOT_COUNT || a == b) {
        saveio::log.printf("[Save] WARN: setActivePair(%u,%u) invalid, reloading from NVS\n", a, b);
        loadActivePairConfig();
        return SAVE_ERR_CORRUPTED;
    }
    _activeSlotA = a;
    _activeSlotB = b;
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    saveio::log.printf("[Save] Active slots set directly: %u,%u\n", _activeSlotA, _activeSlotB);
    return SAVE_OK;
}

bool SaveManager::readSlotHeader(uint8_t slot, SaveHeader& hdr) {
    if (!_initialized) return false;
    if(slot>=SAVE_SLOT_COUNT)return false;
    const char* newer[3]={"s0_v10","s1_v10","s2_v10"};
    if(prefs.isKey(newer[slot]))return false;
    const char* hdrKey = slotHdrKey(slot);
    if (!prefs.isKey(hdrKey)) return false;
    size_t readLen = prefs.getBytes(hdrKey, &hdr, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) return false;
    if (hdr.version != SAVE_DATA_VERSION) return false;
    if (hdr.data_size != sizeof(PetState)) return false;
    return true;
}

bool SaveManager::loadSlotGallery(uint8_t slot, GalleryData& gallery) {
    if(!_initialized||slot>=SAVE_SLOT_COUNT)return false;
    uint8_t bytes[sizeof(GalleryData)];const auto key=slotGalleryKey(slot);
    if(prefs.getBytesLength(key)!=sizeof(bytes)||prefs.getBytes(key,bytes,sizeof(bytes))!=sizeof(bytes))return false;
    for(const auto value:bytes)if(value>1)return false;
    memcpy(&gallery,bytes,sizeof(bytes));return true;
}

SaveResult SaveManager::syncGlobalGalleryToSlot(uint8_t slot) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(_recoveryRequired||slot>=SAVE_SLOT_COUNT||slot==_frozenOwner)return SAVE_ERR_CORRUPTED;
    GalleryData gallery;gallery.init();const auto result=loadGallery(gallery);
    if(result!=SAVE_OK&&result!=SAVE_ERR_NO_DATA)return result;
    return prefs.putBytes(slotGalleryKey(slot),&gallery,sizeof(gallery))==sizeof(gallery)?SAVE_OK:SAVE_ERR_WRITE;
}

bool SaveManager::slotHasData(uint8_t slot) const {
    if(slot>=SAVE_SLOT_COUNT)return false;
    const char* newer[3]={"s0_v10","s1_v10","s2_v10"};
    return prefs.isKey(HDR_KEYS[slot])||prefs.isKey(DATA_KEYS[slot])||prefs.isKey(GALLERY_KEYS[slot])||prefs.isKey(newer[slot]);
}
bool SaveManager::slotWritable(uint8_t slot) {
    if(slot>=SAVE_SLOT_COUNT||slot==_frozenOwner)return false;
    if(!slotHasData(slot))return true;
    PetState pet;return loadSlot(slot,pet)==SAVE_OK;
}

uint8_t SaveManager::getOldestSlot() {
    // 在当前活动槽对中找 sequence 最小的槽 (空槽 sequence=0, 优先被覆写)
    uint32_t minSeq = UINT32_MAX;
    uint8_t oldest = 0xff;

    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!isSlotActive(i)||!slotWritable(i)) continue;
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
    if (_stickyWriteSlot < SAVE_SLOT_COUNT&&slotWritable(_stickyWriteSlot))
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
    if(!_initialized)return false;
    for(uint8_t slot=0;slot<SAVE_SLOT_COUNT;++slot)if(slotHasData(slot))return true;
    for(const auto key:{"gallery","dev_state","gallery_v10","device_v10"})if(prefs.isKey(key))return true;
    return operationPending();
}

bool SaveManager::operationPending() const {
    if(!prefs.isKey("repair_guard"))return false;
    uint8_t bytes[6];
    if(prefs.getBytesLength("repair_guard")!=sizeof(bytes)||prefs.getBytes("repair_guard",bytes,sizeof(bytes))!=sizeof(bytes))return true;
    uint16_t sum=0xffff;
    for(unsigned i=0;i<4;++i){sum^=bytes[i];sum=uint16_t((sum<<3)|(sum>>13));sum+=bytes[i];}
    return bytes[0]!=0xa7||bytes[3]!=1||bytes[4]!=uint8_t(sum)||bytes[5]!=uint8_t(sum>>8)||bytes[1]!=0||bytes[2]!=255;
}
SaveResult SaveManager::beginOperation(uint8_t owner) {
    if(!_initialized||operationPending()||(owner>=3&&owner!=255))return SAVE_ERR_CORRUPTED;
    uint8_t bytes[6]={0xa7,1,owner,1,0,0};const auto sum=calcChecksum(bytes,4);
    bytes[4]=uint8_t(sum);bytes[5]=uint8_t(sum>>8);
    if(prefs.putBytes("repair_guard",bytes,sizeof(bytes))!=sizeof(bytes)){requireRecovery();return SAVE_ERR_WRITE;}
    _frozenOwner=owner;_operation=true;_recoveryRequired=false;return SAVE_OK;
}
SaveResult SaveManager::finishOperation() {
    if(!_operation)return SAVE_ERR_CORRUPTED;
    DeviceState device;const auto result=loadDeviceState(device);
    if(result!=SAVE_OK)return result;
    uint8_t bytes[6]={0xa7,0,255,1,0,0};const auto sum=calcChecksum(bytes,4);
    bytes[4]=uint8_t(sum);bytes[5]=uint8_t(sum>>8);
    if(prefs.putBytes("repair_guard",bytes,sizeof(bytes))!=sizeof(bytes)){requireRecovery();return SAVE_ERR_WRITE;}
    _operation=false;_frozenOwner=device.is_visiting?device.owner_frozen_slot:255;
    return SAVE_OK;
}

SaveResult SaveManager::erase() {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    if(!prefs.clear())return SAVE_ERR_WRITE;
    _recoveryRequired=false;_operation=false;_frozenOwner=255;
    _nextSequence = 1;
    _stickyWriteSlot = 0xFF;
    _activeSlotA = 0;
    _activeSlotB = 1;
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    saveio::log.println("[Save] All save data erased");
    saveio::log.println("[Save] Active slots reset to default: 0,1");

    return SAVE_OK;
}

bool SaveManager::shouldAutoSave(uint32_t currentTime) {
    if(!_initialized||_recoveryRequired||_nextSequence==0)return false;
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
    if(_recoveryRequired)return SAVE_ERR_CORRUPTED;

    size_t written = prefs.putBytes(GALLERY_NVS_KEY, &gallery, sizeof(GalleryData));
    if (written != sizeof(GalleryData)) {
        saveio::log.println("[Save] ERROR: Failed to write gallery data");
        return SAVE_ERR_WRITE;
    }

    saveio::log.printf("[Save] Gallery saved (%d bytes, %d/%d unlocked)\n",
                  (int)sizeof(GalleryData), gallery.getUnlockedCount(), FORM_COUNT);
    return SAVE_OK;
}

SaveResult SaveManager::loadGallery(GalleryData& gallery) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(prefs.isKey("gallery_v10"))return SAVE_ERR_VERSION_MISMATCH;
    if(!prefs.isKey(GALLERY_NVS_KEY)){gallery.init();return SAVE_ERR_NO_DATA;}
    uint8_t bytes[sizeof(GalleryData)];
    if(prefs.getBytesLength(GALLERY_NVS_KEY)!=sizeof(bytes)||prefs.getBytes(GALLERY_NVS_KEY,bytes,sizeof(bytes))!=sizeof(bytes))return SAVE_ERR_CORRUPTED;
    for(const auto value:bytes)if(value>1)return SAVE_ERR_CORRUPTED;
    memcpy(&gallery,bytes,sizeof(bytes));return SAVE_OK;
}

// ============================================================================
//  设备态 (独立 NVS key, 绑定本机)
// ============================================================================

SaveResult SaveManager::saveDeviceState(const DeviceState& ds) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if(_recoveryRequired)return SAVE_ERR_CORRUPTED;

    size_t written = prefs.putBytes(DEVICE_STATE_KEY, &ds, sizeof(DeviceState));
    if (written != sizeof(DeviceState)) {
        saveio::log.println("[Save] ERROR: Failed to write device state");
        return SAVE_ERR_WRITE;
    }

    saveio::log.printf("[Save] DeviceState saved (rounds=%u, visiting=%d, clock=%" PRIu32 ")\n",
                  ds.rounds, ds.is_visiting, ds.device_clock_epoch);
    return SAVE_OK;
}

SaveResult SaveManager::loadDeviceState(DeviceState& ds) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(prefs.isKey("device_v10"))return SAVE_ERR_VERSION_MISMATCH;
    if(!prefs.isKey(DEVICE_STATE_KEY)){ds.init();return SAVE_ERR_NO_DATA;}
    uint8_t bytes[sizeof(DeviceState)];
    if(prefs.getBytesLength(DEVICE_STATE_KEY)!=sizeof(bytes)||prefs.getBytes(DEVICE_STATE_KEY,bytes,sizeof(bytes))!=sizeof(bytes))return SAVE_ERR_CORRUPTED;
    if(bytes[offsetof(DeviceState,is_visiting)]>1)return SAVE_ERR_CORRUPTED;
    DeviceState candidate;memcpy(&candidate,bytes,sizeof(bytes));
    if(candidate.is_visiting&&candidate.owner_frozen_slot>=SAVE_SLOT_COUNT)return SAVE_ERR_CORRUPTED;
    ds=candidate;return SAVE_OK;
}
