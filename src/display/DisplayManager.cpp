// src/display/DisplayManager.cpp
// 显示管理器实现 - UI 状态机
// show*() 只更新 DisplayModel + 标记 dirty
// update() 管理动画/页面超时, renderIfDirty() 委托 DisplayRenderer 绘制

#include "DisplayManager.h"
#include "../input/menu_controller.h"
#include "../core/time_manager.h"
#include "../pet/gallery.h"
#include <Arduino.h>
#include <string.h>

// ============================================================================
//  静态成员初始化
// ============================================================================

DisplayPage  DisplayManager::_currentPage       = PAGE_BOOT;
AnimState    DisplayManager::_currentAnim       = ANIM_NONE;
uint32_t     DisplayManager::_pageEnteredMs     = 0;
uint32_t     DisplayManager::_animStartedMs     = 0;
uint32_t     DisplayManager::_animDurationMs    = 0;
UIContext    DisplayManager::_animCompleteContext = UI_IDLE;
bool         DisplayManager::_dirty             = true;
DisplayModel DisplayManager::_model;
uint32_t     DisplayManager::_lastRenderMs      = 0;

// Page hold 机制
bool         DisplayManager::_pageHoldActive    = false;
uint32_t     DisplayManager::_pageHoldUntilMs   = 0;
DisplayPage  DisplayManager::_pageAfterHold     = PAGE_IDLE;
UIContext    DisplayManager::_contextAfterHold  = UI_IDLE;

// Pending evolution (麻婆诅咒链式动画)
bool             DisplayManager::_pendingEvolutionActive   = false;
EvolutionResult  DisplayManager::_pendingEvolution         = {};
int16_t          DisplayManager::_pendingEvolutionSrAfter  = 0;

// ============================================================================
//  生命周期
// ============================================================================

void DisplayManager::init() {
    _model.clear();
    _currentPage = PAGE_BOOT;
    _currentAnim = ANIM_NONE;
    _pageEnteredMs = millis();
    _dirty = true;
    _lastRenderMs = 0;
    _pageHoldActive = false;
    _pageHoldUntilMs = 0;
    _pageAfterHold = PAGE_IDLE;
    _contextAfterHold = UI_IDLE;
    _pendingEvolutionActive = false;

    DisplayRenderer::init();
    markDirty();
}

void DisplayManager::update(uint32_t nowMs) {
    // --- 动画超时检测 ---
    if (_currentAnim != ANIM_NONE && _animDurationMs > 0) {
        uint32_t elapsed = nowMs - _animStartedMs;
        if (elapsed >= _animDurationMs) {
            // 动画结束
            AnimState finishedAnim = _currentAnim;
            UIContext ctx = _animCompleteContext;
            _currentAnim = ANIM_NONE;
            _animDurationMs = 0;
            _model.animState = ANIM_NONE;
            _model.animElapsedMs = 0;
            _model.animFrameIndex = 0;
            markDirty();

            // 检查是否有 pending evolution (麻婆诅咒链式动画)
            if (_pendingEvolutionActive &&
                (finishedAnim == ANIM_MAPO_TOFU || finishedAnim == ANIM_NOBU_EVENT)) {
                _pendingEvolutionActive = false;
                // 直接播放 pending evolution 动画
                showEvolutionEvent(_pendingEvolution, _pendingEvolutionSrAfter);
                // 不通知 MenuController, evolution 动画结束后由正常流程处理
            } else {
                // 通知 MenuController 动画完成
                if (ctx != UI_IDLE || finishedAnim == ANIM_POKE) {
                    menuController.onAnimationComplete(ctx);
                } else {
                    // 动画完成 context 为 UI_IDLE 且不需要通知 MenuController 时,
                    // 直接切回 idle 页面 (如 ANIM_DESTROY, ANIM_DAY_END 等)
                    switchPage(PAGE_IDLE);
                }
            }
        } else {
            // 动画播放中: 更新帧状态并持续刷新
            _model.animState = _currentAnim;
            _model.animElapsedMs = elapsed;
            // 按 DISPLAY_FRAME_MS 计算帧索引
            _model.animFrameIndex = (uint8_t)(elapsed / DISPLAY_FRAME_MS);
            markDirty();
        }
    }

    // --- Page hold 超时检测 ---
    if (_pageHoldActive && nowMs >= _pageHoldUntilMs) {
        _pageHoldActive = false;
        DisplayPage nextPage = _pageAfterHold;
        // UIContext nextCtx = _contextAfterHold; // 不主动切 context, 由 switchPage 触发
        switchPage(nextPage);
    }

    // --- 页面自动切换 ---
    switch (_currentPage) {
        case PAGE_BOOT:
            if (nowMs - _pageEnteredMs >= ANIM_DURATION_BOOT) {
                switchPage(PAGE_IDLE);
            }
            break;

        case PAGE_FEED_RESULT:
            // 至少停留 PAGE_DURATION_FEED_RESULT (由 hold 机制管理)
            // 如果没有 hold active 且没有动画, 说明 hold 已结束或未设置
            if (!_pageHoldActive && _currentAnim == ANIM_NONE &&
                (nowMs - _pageEnteredMs >= PAGE_DURATION_FEED_RESULT)) {
                switchPage(PAGE_IDLE);
            }
            break;

        default:
            break;
    }

    // --- Toast 超时 ---
    if (_model.toast[0] != '\0' && nowMs >= _model.toastUntilMs) {
        _model.toast[0] = '\0';
        markDirty();
    }

    // --- 渲染 (帧率限制) ---
    if (_dirty && (nowMs - _lastRenderMs >= DISPLAY_FRAME_MS)) {
        renderIfDirty();
        _lastRenderMs = nowMs;
    }
}

void DisplayManager::renderIfDirty() {
    if (!_dirty) return;
    _dirty = false;

    // 根据当前页面调用对应的 renderer
    switch (_currentPage) {
        case PAGE_BOOT:
            DisplayRenderer::drawBoot(_model);
            break;
        case PAGE_IDLE:
            DisplayRenderer::drawIdle(_model);
            break;
        case PAGE_STATUS:
            DisplayRenderer::drawStatus(_model);
            break;
        case PAGE_FEED_DRAW:
            DisplayRenderer::drawFeedDraw(_model);
            break;
        case PAGE_FEED_PICK:
            DisplayRenderer::drawFeedPick(_model);
            break;
        case PAGE_FEED_RESULT:
            DisplayRenderer::drawFeedResult(_model);
            break;
        case PAGE_SPECIAL_FOOD:
            DisplayRenderer::drawSpecialFood(_model);
            break;
        case PAGE_POKE_ANIM:
            DisplayRenderer::drawPoke(_model);
            break;
        case PAGE_EVOLUTION:
            DisplayRenderer::drawEvolution(_model);
            break;
        case PAGE_DESTROY_CONFIRM:
            DisplayRenderer::drawDestroyConfirm(_model);
            break;
        case PAGE_DAY_END:
            DisplayRenderer::drawDayEnd(_model);
            break;
        case PAGE_WAIT_TIME_SET:
            DisplayRenderer::drawWaitTimeSet(_model);
            break;
        case PAGE_GALLERY:
            DisplayRenderer::drawGallery(_model);
            break;
        default:
            break;
    }

    // Toast 叠加层
    if (_model.toast[0] != '\0') {
        DisplayRenderer::drawToast(_model);
    }
    DisplayRenderer::present();
}

// ============================================================================
//  页面/动画控制
// ============================================================================

void DisplayManager::switchPage(DisplayPage page) {
    // 如果当前页面正在 hold 中, 不允许外部切走 (除非切到同一页面)
    if (_pageHoldActive && page != _currentPage) {
        return;
    }
    if (_currentPage == page) return;
    _currentPage = page;
    _pageEnteredMs = millis();
    markDirty();
}

DisplayPage DisplayManager::getCurrentPage() {
    return _currentPage;
}

void DisplayManager::setAnimation(AnimState anim, uint32_t durationMs, UIContext completeContext) {
    _currentAnim = anim;
    _animStartedMs = millis();
    _animDurationMs = durationMs;
    _animCompleteContext = completeContext;
    markDirty();
}

void DisplayManager::stopAnimation() {
    _currentAnim = ANIM_NONE;
    _animDurationMs = 0;
    markDirty();
}

AnimState DisplayManager::getCurrentAnimation() {
    return _currentAnim;
}

bool DisplayManager::isAnimationPlaying() {
    return _currentAnim != ANIM_NONE;
}

bool DisplayManager::isPageBlockingInput() {
    // 这些页面/状态下阻塞普通输入
    if (_currentAnim != ANIM_NONE && _currentAnim != ANIM_IDLE) return true;
    if (_currentPage == PAGE_BOOT) return true;
    if (_currentPage == PAGE_EVOLUTION) return true;
    if (_currentPage == PAGE_DAY_END) return true;
    if (_currentPage == PAGE_WAIT_TIME_SET) return true;
    // Page hold 期间阻塞输入 (FeedResult hold, SpecialFood confirm hold)
    if (_pageHoldActive) return true;
    return false;
}

bool DisplayManager::isPageHoldActive() {
    return _pageHoldActive;
}

// ============================================================================
//  内部辅助
// ============================================================================

void DisplayManager::markDirty() {
    _dirty = true;
}

uint32_t DisplayManager::getAnimDuration(AnimState anim) {
    switch (anim) {
        case ANIM_EATING:               return ANIM_DURATION_EATING;
        case ANIM_POKE:                 return ANIM_DURATION_POKE;
        case ANIM_EVOLUTION:            return ANIM_DURATION_EVOLUTION;
        case ANIM_COMBO:                return ANIM_DURATION_COMBO;
        case ANIM_MAPO_TOFU:            return ANIM_DURATION_MAPO_TOFU;
        case ANIM_RHONGOMYNIAD:         return ANIM_DURATION_RHONGOMYNIAD;
        case ANIM_BLACK_RHONGOMYNIAD:   return ANIM_DURATION_BLACK_RHONGO;
        case ANIM_DESTROY:              return ANIM_DURATION_DESTROY;
        case ANIM_DAY_END:              return ANIM_DURATION_DAY_END;
        case ANIM_NOBU_EVENT:           return ANIM_DURATION_NOBU_EVENT;
        case ANIM_SAVE:                 return ANIM_DURATION_SAVE;
        case ANIM_GALLERY_COMPLETE:     return ANIM_DURATION_GALLERY_COMPLETE;
        default:                        return 0;
    }
}

void DisplayManager::holdPageThen(uint32_t durationMs, DisplayPage nextPage, UIContext nextContext) {
    _pageHoldActive = true;
    _pageHoldUntilMs = millis() + durationMs;
    _pageAfterHold = nextPage;
    _contextAfterHold = nextContext;
}

// ============================================================================
//  宠物快照更新
// ============================================================================

void DisplayManager::updatePetSnapshot(const PetState& pet) {
    bool changed = memcmp(&_model.petSnapshot, &pet, sizeof(PetState)) != 0;
    if (changed) {
        _model.petSnapshot = pet;
        // 更新时间/日期字符串
        timeManager.getFormattedTime(_model.timeStr, sizeof(_model.timeStr));
        timeManager.getFormattedDate(_model.dateStr, sizeof(_model.dateStr));
        _model.ageDay = pet.age_days + 1;
        if (_currentPage == PAGE_IDLE) {
            markDirty();
        }
    }
}

// ============================================================================
//  show* 实现 - 只更新 model + 页面 + 动画 + dirty
// ============================================================================

// --- 系统 ---

void DisplayManager::showBootScreen() {
    _model.bootMessage[0] = '\0';
    switchPage(PAGE_BOOT);
}

void DisplayManager::showSaveLoaded() {
    strncpy(_model.bootMessage, "Save loaded", sizeof(_model.bootMessage) - 1);
    _model.bootMessage[sizeof(_model.bootMessage) - 1] = '\0';
    markDirty();
}

void DisplayManager::showSaveCorruptedNewGame() {
    strncpy(_model.bootMessage, "Save corrupted!", sizeof(_model.bootMessage) - 1);
    _model.bootMessage[sizeof(_model.bootMessage) - 1] = '\0';
    markDirty();
}

void DisplayManager::showNewGame() {
    strncpy(_model.bootMessage, "New game", sizeof(_model.bootMessage) - 1);
    _model.bootMessage[sizeof(_model.bootMessage) - 1] = '\0';
    markDirty();
}

void DisplayManager::showSystemReady() {
    // Boot page auto-transitions to idle via update() timeout.
    // Show a brief toast so user sees the boot message was acknowledged.
    showToast("Ready", 1000);
}

// --- 状态面板 ---

void DisplayManager::showStatusPanel(const PetState& pet) {
    _model.petSnapshot = pet;
    switchPage(PAGE_STATUS);
}

void DisplayManager::hideStatusPanel() {
    switchPage(PAGE_IDLE);
}

// --- 投喂流程 ---

void DisplayManager::showCannotInteract() {
    showToast("Cannot interact", 1500);
}

void DisplayManager::showFeedCheckFailed(FeedResult result, uint32_t waitSeconds) {
    char buf[64];
    if (result == FEED_ERR_TOO_SOON) {
        snprintf(buf, sizeof(buf), "Wait %lus", waitSeconds);
    } else if (result == FEED_ERR_DAILY_LIMIT) {
        snprintf(buf, sizeof(buf), "Daily limit reached");
    } else {
        snprintf(buf, sizeof(buf), "Cannot feed");
    }
    showToast(buf, 2000);
}

void DisplayManager::showFeedDraw(const FeedDraw& draw) {
    _model.feedDraw = draw;
    for (uint8_t i = 0; i < 4; i++) _model.feedSelected[i] = false;
    _model.feedCursor = 0;
    switchPage(PAGE_FEED_DRAW);
    setAnimation(ANIM_EATING, ANIM_DURATION_FEED_DRAW, UI_FEED_DRAW);
}

void DisplayManager::showFeedCursorMove(uint8_t cursor, const bool selected[4]) {
    _model.feedCursor = cursor;
    for (uint8_t i = 0; i < 4; i++) _model.feedSelected[i] = selected[i];
    markDirty();
}

void DisplayManager::showFeedSlotToggle(uint8_t slot, bool isSelected) {
    if (slot < 4) _model.feedSelected[slot] = isSelected;
    markDirty();
}

void DisplayManager::showFeedResult(const FeedOutcome& outcome, int16_t srAfter) {
    _model.feedOutcome = outcome;
    _model.feedSrAfter = srAfter;
    switchPage(PAGE_FEED_RESULT);

    // 非 combo 投喂: 设置 hold, 确保结果页至少停留 PAGE_DURATION_FEED_RESULT
    if (!outcome.combo_triggered) {
        holdPageThen(PAGE_DURATION_FEED_RESULT, PAGE_IDLE, UI_IDLE);
    }
    // combo 路径: 由 ANIM_COMBO 动画管理, 不设 hold
}

void DisplayManager::showFeedComboTriggered(ComboType combo) {
    setAnimation(ANIM_COMBO, ANIM_DURATION_COMBO, UI_SPECIAL_FOOD);
    markDirty();
}

void DisplayManager::showFeedCancel() {
    switchPage(PAGE_IDLE);
}

// --- 特殊食物 ---

void DisplayManager::showSpecialFoodSelection(uint8_t count) {
    _model.specialFoodCount = count;
    _model.specialFoodCursor = 0;
    _model.mapoTriggered = false;
    _model.mapoCurseActivated = false;
    switchPage(PAGE_SPECIAL_FOOD);
}

void DisplayManager::showSpecialFoodCursor(uint8_t cursor) {
    _model.specialFoodCursor = cursor;
    markDirty();
}

void DisplayManager::showSpecialFoodConfirm(uint8_t id, const FeedOutcome& outcome) {
    _model.specialFoodSelectedId = id;
    _model.feedOutcome = outcome;
    _model.mapoTriggered = outcome.mapo_tofu_triggered;
    _model.mapoCount = outcome.mapo_tofu_total;
    _model.mapoCurseActivated = outcome.mapo_tofu_curse_activated;
    markDirty();

    if (outcome.mapo_tofu_triggered) {
        // nobu 路线使用占位动画
        if (_model.petSnapshot.is_nobu) {
            setAnimation(ANIM_NOBU_EVENT, ANIM_DURATION_NOBU_EVENT, UI_IDLE);
        } else {
            setAnimation(ANIM_MAPO_TOFU, ANIM_DURATION_MAPO_TOFU, UI_IDLE);
        }
    } else {
        // 非 mapo: 至少显示确认 1 秒
        holdPageThen(1000, PAGE_IDLE, UI_IDLE);
    }
}

// --- 麻婆豆腐 ---

void DisplayManager::showMapoTofuTriggered(uint8_t currentCount, uint8_t threshold) {
    _model.mapoTriggered = true;
    _model.mapoCount = currentCount;
    markDirty();
}

void DisplayManager::showMapoTofuCurseActivated() {
    _model.mapoCurseActivated = true;
    markDirty();
}

// --- 戳一戳 ---

void DisplayManager::showPokeAnimation() {
    switchPage(PAGE_POKE_ANIM);
    setAnimation(ANIM_POKE, ANIM_DURATION_POKE, UI_POKE_ANIM);
}

void DisplayManager::showPokeResult(bool valueChanged, int16_t srBefore, int16_t srAfter) {
    _model.pokeValueChanged = valueChanged;
    _model.pokeSrBefore = srBefore;
    _model.pokeSrAfter = srAfter;
    markDirty();
}

// --- 日结算 ---

void DisplayManager::showDayEndStart() {
    memset(&_model.dayEnd, 0, sizeof(_model.dayEnd));
    switchPage(PAGE_DAY_END);
    setAnimation(ANIM_DAY_END, ANIM_DURATION_DAY_END, UI_IDLE);
}

void DisplayManager::showDayEndIdleSR(int16_t srBefore, int16_t srAfter,
                                       SeriousnessTier tierBefore, SeriousnessTier tierAfter) {
    _model.dayEnd.idleSrBefore = srBefore;
    _model.dayEnd.idleSrAfter = srAfter;
    _model.dayEnd.idleTierBefore = tierBefore;
    _model.dayEnd.idleTierAfter = tierAfter;
    markDirty();
}

void DisplayManager::showDayEndTerminalState() {
    _model.dayEnd.terminalState = true;
    markDirty();
}

void DisplayManager::showDayEndWindowBonus(int16_t bonusHP) {
    _model.dayEnd.windowBonusApplied = true;
    _model.dayEnd.windowBonusHP = bonusHP;
    markDirty();
}

void DisplayManager::showDayEndWindowPenalty(int16_t penaltyHP) {
    _model.dayEnd.windowPenaltyApplied = true;
    _model.dayEnd.windowPenaltyHP = penaltyHP;
    markDirty();
}

void DisplayManager::showDayEndMissedFeed(uint8_t fedCount, uint8_t limit) {
    _model.dayEnd.missedFeedPenalty = true;
    _model.dayEnd.fedCount = fedCount;
    _model.dayEnd.feedLimit = limit;
    markDirty();
}

void DisplayManager::showDayEndComplete(uint16_t dayNumber) {
    _model.dayEnd.dayNumber = dayNumber;
    markDirty();
}

void DisplayManager::showNewDayDetected() {
    showToast("New day!", 1000);
}

// --- 进化系统 ---

void DisplayManager::showChildGraduation(const EvolutionResult& result, Alignment alignment) {
    _model.evolution = result;
    switchPage(PAGE_EVOLUTION);
    setAnimation(ANIM_EVOLUTION, ANIM_DURATION_EVOLUTION, UI_EVOLUTION);
}

void DisplayManager::showFormChange(Form formBefore, Form formAfter, int16_t seriousness,
                                     SeriousnessTier tier) {
    _model.evolution.form_before = formBefore;
    _model.evolution.form_after = formAfter;
    _model.evolution.event = EVO_FORM_CHANGED;
    _model.evolution.tier = tier;
    switchPage(PAGE_EVOLUTION);
    setAnimation(ANIM_EVOLUTION, ANIM_DURATION_EVOLUTION, UI_EVOLUTION);
}

void DisplayManager::showEvolutionEvent(const EvolutionResult& result, int16_t srAfter) {
    // 如果当前正在播放 ANIM_MAPO_TOFU, 将进化排队等待
    if (_currentAnim == ANIM_MAPO_TOFU || _currentAnim == ANIM_NOBU_EVENT) {
        _pendingEvolutionActive = true;
        _pendingEvolution = result;
        _pendingEvolutionSrAfter = srAfter;
        return;
    }

    _model.evolution = result;
    _model.srAfter = srAfter;

    AnimState anim = ANIM_EVOLUTION;
    uint32_t duration = ANIM_DURATION_EVOLUTION;

    if (result.event == EVO_RHONGOMYNIAD) {
        anim = ANIM_RHONGOMYNIAD;
        duration = ANIM_DURATION_RHONGOMYNIAD;
    } else if (result.event == EVO_BLACK_RHONGOMYNIAD) {
        anim = ANIM_BLACK_RHONGOMYNIAD;
        duration = ANIM_DURATION_BLACK_RHONGO;
    }

    switchPage(PAGE_EVOLUTION);
    setAnimation(anim, duration, UI_EVOLUTION);
}

// --- 严肃值系统 ---

void DisplayManager::showIdleTierChange(SeriousnessTier tierBefore, SeriousnessTier tierAfter) {
    _model.tierBefore = tierBefore;
    _model.tierAfter = tierAfter;
    markDirty();
}

void DisplayManager::showIdleFormChange(Form formBefore, Form formAfter) {
    _model.evolution.form_before = formBefore;
    _model.evolution.form_after = formAfter;
    _model.evolution.event = EVO_FORM_CHANGED;
    switchPage(PAGE_EVOLUTION);
    setAnimation(ANIM_EVOLUTION, ANIM_DURATION_EVOLUTION, UI_EVOLUTION);
}

void DisplayManager::showIdleRhongoCountdown(uint32_t remainingHours) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Rhongo: %luh", remainingHours);
    showToast(buf, 2000);
}

void DisplayManager::showMissedFeedPenalty(int16_t srBefore, int16_t srAfter, int16_t penalty) {
    _model.srBefore = srBefore;
    _model.srAfter = srAfter;
    markDirty();
}

// --- 销毁/重置 ---

void DisplayManager::showDestroyConfirm(uint8_t cursor) {
    _model.destroyCursor = cursor;
    switchPage(PAGE_DESTROY_CONFIRM);
}

void DisplayManager::showDestroyCursorMove(uint8_t cursor) {
    _model.destroyCursor = cursor;
    _lastRenderMs = 0;  // bypass frame-rate limiter for immediate input feedback
    markDirty();
}

void DisplayManager::showDestroyExecuted(Form destroyedForm) {
    _model.destroyedForm = destroyedForm;
    switchPage(PAGE_EVOLUTION);  // 复用进化页面展示销毁动画
    setAnimation(ANIM_DESTROY, ANIM_DURATION_DESTROY, UI_IDLE);
}

void DisplayManager::showDestroyReset() {
    switchPage(PAGE_IDLE);
}

void DisplayManager::showDestroyCancelled() {
    switchPage(PAGE_IDLE);
}

// --- 存档 ---

void DisplayManager::showAutoSave() {
    showToast("Autosaved", 800);
}

// --- 等待时间设置 ---

void DisplayManager::showWaitTimeSet() {
    switchPage(PAGE_WAIT_TIME_SET);
}

// --- 图鉴 ---

void DisplayManager::showGalleryGrid(int page_index, int selected_index) {
    _model.galleryPage = (uint8_t)page_index;
    _model.gallerySelectedIndex = (uint8_t)selected_index;
    _model.galleryTotalForms = TOTAL_GALLERY_FORMS;
    _model.galleryItemsThisPage = gallerySystem.getItemCountOnPage((uint8_t)page_index);
    markDirty();
}

void DisplayManager::showGalleryCompleteUnlocked() {
    // 复用进化页面作为全屏动画容器，保持动画触发/结束流程与其他动画一致
    switchPage(PAGE_EVOLUTION);
    setAnimation(ANIM_GALLERY_COMPLETE, ANIM_DURATION_GALLERY_COMPLETE, UI_IDLE);
}

// --- Toast ---

void DisplayManager::showToast(const char* message, uint32_t durationMs) {
    strncpy(_model.toast, message, sizeof(_model.toast) - 1);
    _model.toast[sizeof(_model.toast) - 1] = '\0';
    _model.toastUntilMs = millis() + durationMs;
    markDirty();
}
