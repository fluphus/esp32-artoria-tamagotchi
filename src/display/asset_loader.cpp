// src/display/asset_loader.cpp
// 资源加载系统实现 - 使用构建脚本生成的 PROGMEM 查找表

#include "asset_loader.h"
#include "generated_assets/generated_asset_index.h"
#include <Arduino.h>
#include <string.h>

// PSRAM 解码缓冲区
uint16_t* AssetLoader::_decodeBuffer = nullptr;
uint32_t AssetLoader::_decodeBufferSize = 0;

static constexpr uint32_t DECODE_BUFFER_PIXELS = 128 * 80;
static constexpr uint32_t DECODE_BUFFER_BYTES = DECODE_BUFFER_PIXELS * 2;

// ============================================================================
//  初始化
// ============================================================================

void AssetLoader::init() {
    allocateDecodeBuffer();
    Serial.printf("[AssetLoader] Init: %d idle, %d eating, %d reaction, %d poke seqs\n",
                  GENERATED_IDLE_COUNT, GENERATED_EATING_COUNT,
                  GENERATED_REACTION_COUNT, GENERATED_POKE_COUNT);
    Serial.printf("[AssetLoader] Total asset data: %d bytes\n", GENERATED_TOTAL_BYTES);
}

// ============================================================================
//  缓冲区管理
// ============================================================================

void AssetLoader::allocateDecodeBuffer() {
    if (_decodeBuffer) return;
    _decodeBuffer = (uint16_t*)ps_malloc(DECODE_BUFFER_BYTES);
    if (_decodeBuffer) {
        _decodeBufferSize = DECODE_BUFFER_BYTES;
        Serial.printf("[AssetLoader] Decode buffer: %u bytes (PSRAM)\n", DECODE_BUFFER_BYTES);
    } else {
        constexpr uint32_t SMALL_BUFFER = 64 * 64 * 2;
        _decodeBuffer = (uint16_t*)malloc(SMALL_BUFFER);
        _decodeBufferSize = SMALL_BUFFER;
        Serial.printf("[AssetLoader] Decode buffer: %u bytes (internal RAM)\n", SMALL_BUFFER);
    }
}

void AssetLoader::freeDecodeBuffer() {
    if (_decodeBuffer) {
        free(_decodeBuffer);
        _decodeBuffer = nullptr;
        _decodeBufferSize = 0;
    }
}

// ============================================================================
//  查找辅助
// ============================================================================

static const AnimSequenceDescriptor* lookupTable(
    const AssetLookupEntry* table, uint16_t tableSize,
    uint8_t key1, uint8_t key2)
{
    for (uint16_t i = 0; i < tableSize; i++) {
        if (table[i].key1 == key1 && table[i].key2 == key2) {
            return table[i].seq;
        }
    }
    return nullptr;
}

static const AnimSequenceDescriptor* lookupTableKey1Only(
    const AssetLookupEntry* table, uint16_t tableSize,
    uint8_t key1)
{
    for (uint16_t i = 0; i < tableSize; i++) {
        if (table[i].key1 == key1) {
            return table[i].seq;
        }
    }
    return nullptr;
}

// ============================================================================
//  资源查询
// ============================================================================

const AnimSequenceDescriptor* AssetLoader::getEatingAnim(Form form, uint8_t foodId) {
    // 先查专属覆盖 (form × food)
    const AnimSequenceDescriptor* specific = lookupTable(
        EATING_ASSET_TABLE, GENERATED_EATING_COUNT, (uint8_t)form, foodId);
    if (specific) return specific;
    // Fallback: 通用进食 (form × 255)
    return lookupTable(EATING_ASSET_TABLE, GENERATED_EATING_COUNT, (uint8_t)form, 255);
}

const AnimSequenceDescriptor* AssetLoader::getFoodAnim(uint8_t foodId) {
    return lookupTableKey1Only(FOOD_ICON_ASSET_TABLE, GENERATED_FOOD_ICON_COUNT, foodId);
}

const AnimSequenceDescriptor* AssetLoader::getReactionAnim(Form form, ReactionType reaction) {
    return lookupTable(REACTION_ASSET_TABLE, GENERATED_REACTION_COUNT,
                       (uint8_t)form, (uint8_t)reaction);
}

const AnimSequenceDescriptor* AssetLoader::getIdleAnim(IdleAnimId id) {
    // Map IdleAnimId → (form_idx, condition_idx) matching Python's IDLE_CONDITION_MAP:
    // 0=normal, 1=sr_high, 2=hp_low, 3=hp_critical, 4=lion_king_sr_max, 5=lion_king_sr_critical
    uint8_t form_idx = 0;
    uint8_t cond_idx = 0;

    switch (id) {
        // Lily (form 0): normal, hp_low, hp_critical
        case IDLE_LILY_NORMAL:       form_idx = 0; cond_idx = 0; break;
        case IDLE_LILY_HP_LOW:       form_idx = 0; cond_idx = 2; break;
        case IDLE_LILY_HP_CRITICAL:  form_idx = 0; cond_idx = 3; break;

        // White Saber (form 1)
        case IDLE_WHITE_SABER_NORMAL:      form_idx = 1; cond_idx = 0; break;
        case IDLE_WHITE_SABER_SR_HIGH:     form_idx = 1; cond_idx = 1; break;
        case IDLE_WHITE_SABER_HP_LOW:      form_idx = 1; cond_idx = 2; break;
        case IDLE_WHITE_SABER_HP_CRITICAL: form_idx = 1; cond_idx = 3; break;

        // Black Saber (form 2)
        case IDLE_BLACK_SABER_NORMAL:      form_idx = 2; cond_idx = 0; break;
        case IDLE_BLACK_SABER_SR_HIGH:     form_idx = 2; cond_idx = 1; break;
        case IDLE_BLACK_SABER_HP_LOW:      form_idx = 2; cond_idx = 2; break;
        case IDLE_BLACK_SABER_HP_CRITICAL: form_idx = 2; cond_idx = 3; break;

        // White Lancer (form 3)
        case IDLE_WHITE_LANCER_NORMAL:              form_idx = 3; cond_idx = 0; break;
        case IDLE_WHITE_LANCER_SR_HIGH:             form_idx = 3; cond_idx = 1; break;
        case IDLE_WHITE_LANCER_HP_LOW:              form_idx = 3; cond_idx = 2; break;
        case IDLE_WHITE_LANCER_HP_CRITICAL:         form_idx = 3; cond_idx = 3; break;
        case IDLE_WHITE_LANCER_LION_KING_SR_MAX:    form_idx = 3; cond_idx = 4; break;
        case IDLE_WHITE_LANCER_LION_KING_SR_CRITICAL: form_idx = 3; cond_idx = 5; break;

        // Black Lancer (form 4)
        case IDLE_BLACK_LANCER_NORMAL:      form_idx = 4; cond_idx = 0; break;
        case IDLE_BLACK_LANCER_SR_HIGH:     form_idx = 4; cond_idx = 1; break;
        case IDLE_BLACK_LANCER_HP_LOW:      form_idx = 4; cond_idx = 2; break;
        case IDLE_BLACK_LANCER_HP_CRITICAL: form_idx = 4; cond_idx = 3; break;

        // White Archer (form 5)
        case IDLE_WHITE_ARCHER_NORMAL:      form_idx = 5; cond_idx = 0; break;
        case IDLE_WHITE_ARCHER_SR_HIGH:     form_idx = 5; cond_idx = 1; break;
        case IDLE_WHITE_ARCHER_HP_LOW:      form_idx = 5; cond_idx = 2; break;
        case IDLE_WHITE_ARCHER_HP_CRITICAL: form_idx = 5; cond_idx = 3; break;

        // Black Rider (form 6)
        case IDLE_BLACK_RIDER_NORMAL:      form_idx = 6; cond_idx = 0; break;
        case IDLE_BLACK_RIDER_SR_HIGH:     form_idx = 6; cond_idx = 1; break;
        case IDLE_BLACK_RIDER_HP_LOW:      form_idx = 6; cond_idx = 2; break;
        case IDLE_BLACK_RIDER_HP_CRITICAL: form_idx = 6; cond_idx = 3; break;

        // White Ruler (form 7)
        case IDLE_WHITE_RULER_NORMAL:      form_idx = 7; cond_idx = 0; break;
        case IDLE_WHITE_RULER_SR_HIGH:     form_idx = 7; cond_idx = 1; break;
        case IDLE_WHITE_RULER_HP_LOW:      form_idx = 7; cond_idx = 2; break;
        case IDLE_WHITE_RULER_HP_CRITICAL: form_idx = 7; cond_idx = 3; break;

        // Rhongomyniad (form 8)
        case IDLE_RHONGOMYNIAD:            form_idx = 8; cond_idx = 0; break;

        // Black Rhongomyniad (form 9)
        case IDLE_BLACK_RHONGOMYNIAD:      form_idx = 9; cond_idx = 0; break;

        // Nobu (form 10)
        case IDLE_NOBU_NORMAL:             form_idx = 10; cond_idx = 0; break;

        // Oda Nobunaga (form 11)
        case IDLE_ODA_NOBUNAGA_NORMAL:     form_idx = 11; cond_idx = 0; break;

        default: form_idx = 0; cond_idx = 0; break;
    }

    return lookupTable(IDLE_ASSET_TABLE, GENERATED_IDLE_COUNT, form_idx, cond_idx);
}

const AnimSequenceDescriptor* AssetLoader::getRandomIdleAnim(RandomIdleAnimId id) {
    // Map RandomIdleAnimId to (form_idx, anim_idx)
    // Random idle table stores all random anims per form sequentially
    // We just need to find any entry matching the form
    // For now, pick based on form from the ID ranges defined in idle_resolver.h
    uint8_t form_idx = 0;
    uint8_t anim_idx = 0;

    if (id >= RANDOM_IDLE_LILY_YAWN && id <= RANDOM_IDLE_LILY_STRETCH) {
        form_idx = 0; anim_idx = id - RANDOM_IDLE_LILY_YAWN;
    } else if (id >= RANDOM_IDLE_WHITE_SABER_LOOK_AROUND && id <= RANDOM_IDLE_WHITE_SABER_SIGH) {
        form_idx = 1; anim_idx = id - RANDOM_IDLE_WHITE_SABER_LOOK_AROUND;
    } else if (id >= RANDOM_IDLE_BLACK_SABER_SMIRK && id <= RANDOM_IDLE_BLACK_SABER_CROSS_ARMS) {
        form_idx = 2; anim_idx = id - RANDOM_IDLE_BLACK_SABER_SMIRK;
    } else {
        // For forms without explicit enum entries, use form index directly
        return nullptr;
    }

    return lookupTable(RANDOM_IDLE_ASSET_TABLE, GENERATED_RANDOM_IDLE_COUNT, form_idx, anim_idx);
}

const AnimSequenceDescriptor* AssetLoader::getComboGiverAnim(uint8_t specialFoodId) {
    return lookupTable(COMBO_ASSET_TABLE, GENERATED_COMBO_COUNT, specialFoodId, 0);
}

const AnimSequenceDescriptor* AssetLoader::getMapoTofuAnim(Form form) {
    return lookupTableKey1Only(MAPO_ASSET_TABLE, GENERATED_MAPO_COUNT, (uint8_t)form);
}

const AnimSequenceDescriptor* AssetLoader::getPokeAnim(Form form) {
    return lookupTableKey1Only(POKE_ASSET_TABLE, GENERATED_POKE_COUNT, (uint8_t)form);
}

const AnimSequenceDescriptor* AssetLoader::getEvolutionAnim(Form formBefore, Form formAfter) {
    (void)formBefore; (void)formAfter;
    return nullptr;  // TODO: evolution assets
}

// ============================================================================
//  帧解码
// ============================================================================

const uint16_t* AssetLoader::decodeFrame(const AnimFrameDescriptor* frame) {
    if (!frame || !frame->data || !_decodeBuffer) return nullptr;

    uint32_t pixelCount = (uint32_t)frame->width * frame->height;
    if (pixelCount * 2 > _decodeBufferSize) return nullptr;

    switch (frame->format) {
        case ASSET_FMT_RGB565:
            decodeRGB565(frame->data, _decodeBuffer, pixelCount);
            break;
        case ASSET_FMT_INDEXED4:
            decodeIndexed4(frame->data, frame->palette, _decodeBuffer, pixelCount);
            break;
        case ASSET_FMT_INDEXED8:
            decodeIndexed8(frame->data, frame->palette, _decodeBuffer, pixelCount);
            break;
        default:
            decodeRGB565(frame->data, _decodeBuffer, pixelCount);
            break;
    }

    return _decodeBuffer;
}

// ============================================================================
//  解码器
// ============================================================================

void AssetLoader::decodeRGB565(const uint8_t* src, uint16_t* dst, uint32_t pixelCount) {
    // PROGMEM data: read with pgm_read_word for 16-bit aligned access
    const uint16_t* src16 = (const uint16_t*)src;
    for (uint32_t i = 0; i < pixelCount; i++) {
        dst[i] = pgm_read_word(&src16[i]);
    }
}

void AssetLoader::decodeIndexed4(const uint8_t* src, const uint16_t* palette,
                                  uint16_t* dst, uint32_t pixelCount) {
    if (!palette) return;
    for (uint32_t i = 0; i < pixelCount; i += 2) {
        uint8_t byte = pgm_read_byte(&src[i / 2]);
        dst[i] = pgm_read_word(&palette[(byte >> 4) & 0x0F]);
        if (i + 1 < pixelCount) {
            dst[i + 1] = pgm_read_word(&palette[byte & 0x0F]);
        }
    }
}

void AssetLoader::decodeIndexed8(const uint8_t* src, const uint16_t* palette,
                                  uint16_t* dst, uint32_t pixelCount) {
    if (!palette) return;
    for (uint32_t i = 0; i < pixelCount; i++) {
        dst[i] = pgm_read_word(&palette[pgm_read_byte(&src[i])]);
    }
}

void AssetLoader::decodeRLE_RGB565(const uint8_t* src, uint16_t* dst, uint32_t pixelCount) {
    (void)src; (void)dst; (void)pixelCount;
    // TODO: implement when RLE format is used
}

void AssetLoader::decodeRLE_Indexed4(const uint8_t* src, const uint16_t* palette,
                                      uint16_t* dst, uint32_t pixelCount) {
    (void)src; (void)palette; (void)dst; (void)pixelCount;
    // TODO: implement when RLE indexed format is used
}
