// src/pet/gallery.h
// 图鉴系统 - 管理形态解锁记录与分页浏览逻辑

#ifndef GALLERY_H
#define GALLERY_H

#include <stdint.h>
#include "../core/game_state.h"

// ============================================================================
//  图鉴配置
// ============================================================================

// 总可解锁形态数 (与 FORM_COUNT 一致)
#define TOTAL_GALLERY_FORMS     ((uint8_t)FORM_COUNT)

// 每页显示数量 (2x2 网格)
#define GALLERY_ITEMS_PER_PAGE  4

// 总页数 (向上取整)
#define GALLERY_TOTAL_PAGES     ((TOTAL_GALLERY_FORMS + GALLERY_ITEMS_PER_PAGE - 1) / GALLERY_ITEMS_PER_PAGE)

// ============================================================================
//  图鉴解锁数据 (存入存档)
// ============================================================================

struct GalleryData {
    bool unlocked_forms[FORM_COUNT];    // 每个形态是否已解锁

    void init() {
        for (uint8_t i = 0; i < FORM_COUNT; i++) {
            unlocked_forms[i] = false;
        }
    }

    // 解锁指定形态
    void unlock(Form form) {
        if (form < FORM_COUNT) {
            unlocked_forms[form] = true;
        }
    }

    // 解锁所有形态
    void unlockAll() {
        for (uint8_t i = 0; i < FORM_COUNT; i++) {
            unlocked_forms[i] = true;
        }
    }

    // 查询是否已解锁
    bool isUnlocked(Form form) const {
        if (form >= FORM_COUNT) return false;
        return unlocked_forms[form];
    }

    // 获取已解锁数量
    uint8_t getUnlockedCount() const {
        uint8_t count = 0;
        for (uint8_t i = 0; i < FORM_COUNT; i++) {
            if (unlocked_forms[i]) count++;
        }
        return count;
    }
};

// ============================================================================
//  图鉴浏览状态 (运行时, 不存档)
// ============================================================================

struct GalleryBrowseState {
    uint8_t current_page;       // 当前页码 (0-based)
    uint8_t selected_index;     // 当前页内选中格子索引 (0 to ITEMS_PER_PAGE-1)
    bool    active;             // 是否正在浏览图鉴

    void reset() {
        current_page = 0;
        selected_index = 0;
        active = false;
    }
};

// ============================================================================
//  GallerySystem 类
// ============================================================================

class GallerySystem {
public:
    // 初始化
    void init();

    // 获取图鉴数据引用 (供存档系统读写)
    GalleryData& getData() { return _data; }
    const GalleryData& getData() const { return _data; }

    // 获取浏览状态引用
    GalleryBrowseState& getBrowseState() { return _browse; }
    const GalleryBrowseState& getBrowseState() const { return _browse; }

    // --- 解锁操作 ---
    // 解锁指定形态 (进化时调用), 返回是否为新解锁
    bool unlockForm(Form form);

    // 解锁所有 (测试用)
    void unlockAll();

    // 重置解锁记录 (测试用)
    void resetGallery();

    // --- 浏览操作 ---
    // 打开图鉴
    void open();

    // 关闭图鉴
    void close();

    // 导航: 左移光标 (支持跨页循环)
    void navigateLeft();

    // 导航: 右移光标 (支持跨页循环)
    void navigateRight();

    // --- 查询 ---
    // 获取当前页包含的形态ID列表
    // 返回实际数量 (最后一页可能不满)
    uint8_t getPageForms(uint8_t page, Form* outForms, uint8_t maxCount) const;

    // 获取指定页的有效格子数
    uint8_t getItemCountOnPage(uint8_t page) const;

    // 获取全局索引 (page * ITEMS_PER_PAGE + selected_index)
    uint8_t getGlobalIndex() const;

    // 获取当前选中的形态ID
    Form getSelectedForm() const;

    // 查询形态是否已解锁
    bool isFormUnlocked(Form form) const { return _data.isUnlocked(form); }

private:
    GalleryData _data;
    GalleryBrowseState _browse;
};

// 全局单例
extern GallerySystem gallerySystem;

#endif // GALLERY_H
