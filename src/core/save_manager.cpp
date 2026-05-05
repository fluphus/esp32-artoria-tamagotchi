// src/core/save_manager.cpp

#include "save_manager.h"
#include "../config/game_config.h"
#include <Arduino.h>
#include <Preferences.h>

SaveManager saveManager;

static Preferences prefs;

SaveResult SaveManager::init() {
    bool ok = prefs.begin(SAVE_NVS_NAMESPACE, false);
    if (!ok) {
        Serial.println("[Save] ERROR: NVS init failed");
        return SAVE_ERR_NVS_INIT;
    }

    _initialized = true;
    _lastSaveTime = 0;

    Serial.println("[Save] NVS initialized");

    // 检查是否有存档
    if (hasSave()) {
        uint8_t ver = prefs.getUChar(SAVE_NVS_KEY_VERSION, 0);
        Serial.printf("[Save] Found save data (version %d)\n", ver);
    } else {
        Serial.println("[Save] No existing save data");
    }

    return SAVE_OK;
}

SaveResult SaveManager::save(const PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    // 构建存档头
    SaveHeader header;
    header.version = SAVE_DATA_VERSION;
    header.data_size = sizeof(PetState);
    header.checksum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));

    // 写入头
    size_t written = prefs.putBytes("save_hdr", &header, sizeof(SaveHeader));
    if (written != sizeof(SaveHeader)) {
        Serial.println("[Save] ERROR: Failed to write header");
        return SAVE_ERR_WRITE;
    }

    // 写入数据
    written = prefs.putBytes(SAVE_NVS_KEY_PET, &pet, sizeof(PetState));
    if (written != sizeof(PetState)) {
        Serial.println("[Save] ERROR: Failed to write pet data");
        return SAVE_ERR_WRITE;
    }

    // 写入版本号
    prefs.putUChar(SAVE_NVS_KEY_VERSION, SAVE_DATA_VERSION);

    Serial.printf("[Save] Saved (%d bytes, checksum=0x%04X)\n",
                  sizeof(PetState), header.checksum);

    return SAVE_OK;
}

SaveResult SaveManager::load(PetState& pet) {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    // 读取头
    SaveHeader header;
    size_t readLen = prefs.getBytes("save_hdr", &header, sizeof(SaveHeader));
    if (readLen != sizeof(SaveHeader)) {
        Serial.println("[Save] ERROR: No save header found");
        return SAVE_ERR_NO_DATA;
    }

    // 版本检查
    if (header.version != SAVE_DATA_VERSION) {
        Serial.printf("[Save] ERROR: Version mismatch (save=%d, current=%d)\n",
                      header.version, SAVE_DATA_VERSION);
        return SAVE_ERR_VERSION_MISMATCH;
    }

    // 大小检查
    if (header.data_size != sizeof(PetState)) {
        Serial.printf("[Save] ERROR: Size mismatch (save=%d, current=%d)\n",
                      header.data_size, (int)sizeof(PetState));
        return SAVE_ERR_CORRUPTED;
    }

    // 读取数据
    readLen = prefs.getBytes(SAVE_NVS_KEY_PET, &pet, sizeof(PetState));
    if (readLen != sizeof(PetState)) {
        Serial.println("[Save] ERROR: Failed to read pet data");
        return SAVE_ERR_READ;
    }

    // 校验
    uint16_t checksum = calcChecksum((const uint8_t*)&pet, sizeof(PetState));
    if (checksum != header.checksum) {
        Serial.printf("[Save] ERROR: Checksum mismatch (expected=0x%04X, got=0x%04X)\n",
                      header.checksum, checksum);
        return SAVE_ERR_CHECKSUM;
    }

    Serial.printf("[Save] Loaded (version=%d, %d bytes, checksum OK)\n",
                  header.version, readLen);

    return SAVE_OK;
}

bool SaveManager::hasSave() {
    if (!_initialized) return false;
    return prefs.isKey(SAVE_NVS_KEY_PET);
}

SaveResult SaveManager::erase() {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    prefs.clear();
    Serial.println("[Save] All save data erased");

    return SAVE_OK;
}

bool SaveManager::shouldAutoSave(uint32_t currentTime) {
    if (_lastSaveTime == 0) return true;
    return (currentTime - _lastSaveTime) >= SAVE_INTERVAL_SEC;
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
