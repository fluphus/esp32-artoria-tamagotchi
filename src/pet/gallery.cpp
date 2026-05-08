// src/pet/gallery.cpp
// 图鉴系统实现

#include "gallery.h"
#include <Arduino.h>

GallerySystem gallerySystem;

void GallerySystem::init() {
    _data.init();
    _browse.reset();
}

bool GallerySystem::unlockForm(Form form) {
    if (form >= FORM_COUNT) return false;
    if (_data.isUnlocked(form)) return false;  // 已解锁, 非新解锁
    _data.unlock(form);
    Serial.printf("[Gallery] Unlocked: %s (%d/%d)\n", FORM_NAMES[form],
                  _data.getUnlockedCount(), TOTAL_GALLERY_FORMS);
    return true;
}

void GallerySystem::unlockAll() {
    _data.unlockAll();
    Serial.printf("[Gallery] All forms unlocked (%d/%d)\n",
                  _data.getUnlockedCount(), TOTAL_GALLERY_FORMS);
}

void GallerySystem::resetGallery() {
    _data.init();
    Serial.println("[Gallery] Gallery reset (all forms locked)");
}

void GallerySystem::open() {
    _browse.active = true;
    _browse.current_page = 0;
    _browse.selected_index = 0;
    Serial.println("[Gallery] Opened");
}

void GallerySystem::close() {
    _browse.active = false;
    Serial.println("[Gallery] Closed");
}

void GallerySystem::navigateLeft() {
    uint8_t globalIdx = getGlobalIndex();

    if (globalIdx == 0) {
        // 全局第一个 -> 循环到全局最后一个
        uint8_t lastPage = GALLERY_TOTAL_PAGES - 1;
        uint8_t lastPageItems = getItemCountOnPage(lastPage);
        _browse.current_page = lastPage;
        _browse.selected_index = lastPageItems - 1;
    } else {
        if (_browse.selected_index == 0) {
            // 当前页第一个 -> 翻到上一页最后一个
            _browse.current_page--;
            _browse.selected_index = getItemCountOnPage(_browse.current_page) - 1;
        } else {
            _browse.selected_index--;
        }
    }
}

void GallerySystem::navigateRight() {
    uint8_t globalIdx = getGlobalIndex();
    uint8_t totalItems = TOTAL_GALLERY_FORMS;
    uint8_t itemsOnPage = getItemCountOnPage(_browse.current_page);

    if (globalIdx >= totalItems - 1) {
        // 全局最后一个 -> 循环到全局第一个
        _browse.current_page = 0;
        _browse.selected_index = 0;
    } else {
        if (_browse.selected_index >= itemsOnPage - 1) {
            // 当前页最后一个 -> 翻到下一页第一个
            _browse.current_page++;
            _browse.selected_index = 0;
        } else {
            _browse.selected_index++;
        }
    }
}

uint8_t GallerySystem::getPageForms(uint8_t page, Form* outForms, uint8_t maxCount) const {
    uint8_t startIdx = page * GALLERY_ITEMS_PER_PAGE;
    uint8_t count = 0;

    for (uint8_t i = 0; i < maxCount && (startIdx + i) < TOTAL_GALLERY_FORMS; i++) {
        outForms[i] = (Form)(startIdx + i);
        count++;
    }
    return count;
}

uint8_t GallerySystem::getItemCountOnPage(uint8_t page) const {
    uint8_t startIdx = page * GALLERY_ITEMS_PER_PAGE;
    uint8_t remaining = TOTAL_GALLERY_FORMS - startIdx;
    return (remaining < GALLERY_ITEMS_PER_PAGE) ? remaining : GALLERY_ITEMS_PER_PAGE;
}

uint8_t GallerySystem::getGlobalIndex() const {
    return _browse.current_page * GALLERY_ITEMS_PER_PAGE + _browse.selected_index;
}

Form GallerySystem::getSelectedForm() const {
    uint8_t globalIdx = getGlobalIndex();
    if (globalIdx >= TOTAL_GALLERY_FORMS) return FORM_LILY;  // safety
    return (Form)globalIdx;
}
