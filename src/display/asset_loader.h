// src/display/asset_loader.h
// 资源加载系统 - 支持多种存储格式，自动从文件名映射到资源 ID
// 构建时由 Python 脚本将 PNG 文件转换为 .bin 并生成索引头文件
// 运行时从 Flash (PROGMEM) 或 LittleFS 加载，解压到 PSRAM 缓冲区

#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

#include <stdint.h>
#include "../core/game_state.h"
#include "../config/food_table.h"
#include "../presentation/presentation_types.h"
#include "../presentation/idle_resolver.h"

// ============================================================================
//  资源存储格式
// ============================================================================

enum AssetFormat : uint8_t {
    ASSET_FMT_RGB565 = 0,       // 原始 RGB565 (每像素 2 字节)
    ASSET_FMT_INDEXED4,         // 4-bit 索引色 (16 色调色板 + 每像素 0.5 字节)
    ASSET_FMT_INDEXED8,         // 8-bit 索引色 (256 色调色板 + 每像素 1 字节)
    ASSET_FMT_RLE_RGB565,       // RLE 压缩 RGB565
    ASSET_FMT_RLE_INDEXED4,     // RLE 压缩 4-bit 索引
};

// ============================================================================
//  动画帧序列描述符 (由构建脚本生成)
// ============================================================================

struct AnimFrameDescriptor {
    const uint8_t* data;        // 帧数据指针 (PROGMEM)
    uint16_t width;
    uint16_t height;
    AssetFormat format;
    uint16_t paletteSize;       // 调色板条目数 (0 = 无调色板)
    const uint16_t* palette;    // RGB565 调色板指针 (PROGMEM)
};

struct AnimSequenceDescriptor {
    const AnimFrameDescriptor* frames;  // 帧数组
    uint8_t frameCount;
    uint16_t frameDelayMs;              // 每帧间隔
    bool loop;                          // 是否循环播放
};

// ============================================================================
//  资源查找键 (用于从逻辑 ID 映射到实际资源)
// ============================================================================

// 进食动画: Form × FoodId → AnimSequenceDescriptor
// 反应动画: Form × ReactionType → AnimSequenceDescriptor
// 待机动画: IdleAnimId → AnimSequenceDescriptor
// 随机待机: RandomIdleAnimId → AnimSequenceDescriptor
// Combo 给予者: SpecialFoodId → AnimSequenceDescriptor
// 麻婆豆腐: Form → AnimSequenceDescriptor

// ============================================================================
//  AssetLoader 类 (静态方法)
// ============================================================================

class AssetLoader {
public:
    static void init();

    // --- 资源查询 (返回 nullptr 表示资源不存在，渲染层应画占位) ---

    // 进食动画: 优先查 Form×Food 专属覆盖，fallback 到通用进食动作
    static const AnimSequenceDescriptor* getEatingAnim(Form form, uint8_t foodId);

    // 食物层动画 (食物被吃掉的独立小动画)
    static const AnimSequenceDescriptor* getFoodAnim(uint8_t foodId);

    // 反应动画
    static const AnimSequenceDescriptor* getReactionAnim(Form form, ReactionType reaction);

    // 待机动画
    static const AnimSequenceDescriptor* getIdleAnim(IdleAnimId id);

    // 随机待机动画
    static const AnimSequenceDescriptor* getRandomIdleAnim(RandomIdleAnimId id);

    // Combo 给予者动画
    static const AnimSequenceDescriptor* getComboGiverAnim(uint8_t specialFoodId);

    // 麻婆豆腐动画
    static const AnimSequenceDescriptor* getMapoTofuAnim(Form form);

    // Poke 动画
    static const AnimSequenceDescriptor* getPokeAnim(Form form);

    // 进化动画
    static const AnimSequenceDescriptor* getEvolutionAnim(Form formBefore, Form formAfter);

    // --- 帧解码 (将压缩/索引格式解码为 RGB565 到缓冲区) ---
    // 返回解码后的 RGB565 像素数据指针 (指向 PSRAM 缓冲区)
    // 如果资源不存在返回 nullptr
    static const uint16_t* decodeFrame(const AnimFrameDescriptor* frame);

    // --- 缓冲区管理 ---
    static void allocateDecodeBuffer();     // 在 PSRAM 中分配解码缓冲区
    static void freeDecodeBuffer();

private:
    static uint16_t* _decodeBuffer;         // PSRAM 解码缓冲区
    static uint32_t _decodeBufferSize;      // 缓冲区大小 (字节)

    // 解码器
    static void decodeRGB565(const uint8_t* src, uint16_t* dst, uint32_t pixelCount);
    static void decodeIndexed4(const uint8_t* src, const uint16_t* palette,
                               uint16_t* dst, uint32_t pixelCount);
    static void decodeIndexed8(const uint8_t* src, const uint16_t* palette,
                               uint16_t* dst, uint32_t pixelCount);
    static void decodeRLE_RGB565(const uint8_t* src, uint16_t* dst, uint32_t pixelCount);
    static void decodeRLE_Indexed4(const uint8_t* src, const uint16_t* palette,
                                   uint16_t* dst, uint32_t pixelCount);
};

#endif // ASSET_LOADER_H
