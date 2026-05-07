// src/display/DisplayManager.cpp
// 显示接口层实现 - 当前为空实现桩, 后续接入实际屏幕驱动

#include "DisplayManager.h"

// 静态成员初始化
DisplayPage DisplayManager::_currentPage = PAGE_IDLE;
AnimState DisplayManager::_currentAnim = ANIM_NONE;

// ==== 页面切换接口 ====
void DisplayManager::switchPage(DisplayPage page) { _currentPage = page; }
DisplayPage DisplayManager::getCurrentPage() { return _currentPage; }

// ==== 动画状态接口 ====
void DisplayManager::playAnimation(AnimState anim) { _currentAnim = anim; }
void DisplayManager::stopAnimation() { _currentAnim = ANIM_NONE; }
AnimState DisplayManager::getCurrentAnimation() { return _currentAnim; }
bool DisplayManager::isAnimationPlaying() { return _currentAnim != ANIM_NONE; }

// ==== 系统初始化 ====
void DisplayManager::showBootScreen() {}
void DisplayManager::showSaveLoaded() {}
void DisplayManager::showSaveCorruptedNewGame() {}
void DisplayManager::showNewGame() {}
void DisplayManager::showSystemReady() {}

// ==== 状态面板 ====
void DisplayManager::showStatusPanel(const PetState& pet) {}
void DisplayManager::hideStatusPanel() {}

// ==== 投喂流程 ====
void DisplayManager::showCannotInteract() {}
void DisplayManager::showFeedCheckFailed(FeedResult result, uint32_t waitSeconds) {}
void DisplayManager::showFeedDraw(const FeedDraw& draw) {}
void DisplayManager::showFeedCursorMove(uint8_t cursor, const bool selected[4]) {}
void DisplayManager::showFeedSlotToggle(uint8_t slot, bool isSelected) {}
void DisplayManager::showFeedResult(const FeedOutcome& outcome, int16_t srAfter) {}
void DisplayManager::showFeedComboTriggered(ComboType combo) {}
void DisplayManager::showFeedCancel() {}

// ==== 特殊食物 ====
void DisplayManager::showSpecialFoodSelection(uint8_t count) {}
void DisplayManager::showSpecialFoodCursor(uint8_t cursor) {}
void DisplayManager::showSpecialFoodConfirm(uint8_t id) {}
void DisplayManager::showSpecialFoodAnimation(uint8_t specialFoodId) {}

// ==== 麻婆豆腐彩蛋 ====
void DisplayManager::showMapoTofuTriggered(uint8_t currentCount, uint8_t threshold) {}
void DisplayManager::showMapoTofuCurseActivated() {}

// ==== 戳一戳 ====
void DisplayManager::showPokeAnimation() {}
void DisplayManager::showPokeResult(bool valueChanged, int16_t srBefore, int16_t srAfter) {}
void DisplayManager::showPokeCooldown(uint32_t remainingSeconds) {}
void DisplayManager::showPokeIdlePaused(uint32_t pauseMinutes) {}

// ==== 日结算 ====
void DisplayManager::showDayEndStart() {}
void DisplayManager::showDayEndIdleSR(int16_t srBefore, int16_t srAfter,
                                       SeriousnessTier tierBefore, SeriousnessTier tierAfter) {}
void DisplayManager::showDayEndTerminalState() {}
void DisplayManager::showDayEndWindowBonus(int16_t bonusHP) {}
void DisplayManager::showDayEndWindowPenalty(int16_t penaltyHP) {}
void DisplayManager::showDayEndMissedFeed(uint8_t fedCount, uint8_t limit) {}
void DisplayManager::showDayEndComplete(uint16_t dayNumber) {}
void DisplayManager::showNewDayDetected() {}

// ==== 进化系统 ====
void DisplayManager::showChildGraduation(const EvolutionResult& result, Alignment alignment) {}
void DisplayManager::showFormChange(Form formBefore, Form formAfter, int16_t seriousness,
                                     SeriousnessTier tier) {}
void DisplayManager::showRhongomyniadTriggered() {}
void DisplayManager::showBlackRhongomyniadTriggered() {}
void DisplayManager::showWhiteFunFormLocked(Form funForm) {}
void DisplayManager::showEvolutionEvent(const EvolutionResult& result) {}

// ==== 严肃值系统 ====
void DisplayManager::showRhongoTimerStarted() {}
void DisplayManager::showRhongoTimerTriggered() {}
void DisplayManager::showRhongoTimerReset(int16_t seriousness, int16_t threshold) {}
void DisplayManager::showMissedFeedPenalty(int16_t srBefore, int16_t srAfter, int16_t penalty) {}
void DisplayManager::showIdleTierChange(SeriousnessTier tierBefore, SeriousnessTier tierAfter) {}
void DisplayManager::showIdleFormChange(Form formBefore, Form formAfter) {}
void DisplayManager::showIdleRhongoCountdown(uint32_t remainingHours) {}

// ==== 销毁/重置 ====
void DisplayManager::showDestroyConfirm(uint8_t cursor) {}
void DisplayManager::showDestroyCursorMove(uint8_t cursor) {}
void DisplayManager::showDestroyExecuted(Form destroyedForm) {}
void DisplayManager::showDestroyReset() {}
void DisplayManager::showDestroyCancelled() {}

// ==== 存档系统 ====
void DisplayManager::showSaveSuccess() {}
void DisplayManager::showSaveFailed() {}
void DisplayManager::showLoadSuccess() {}
void DisplayManager::showLoadFailed() {}
void DisplayManager::showSaveErased() {}
void DisplayManager::showAutoSave() {}

// ==== 时间系统 ====
void DisplayManager::showTimeAdvanced(uint32_t minutes) {}
void DisplayManager::showDayAdvanced(uint32_t days) {}
void DisplayManager::showTimeSet() {}

// ==== UI上下文切换 ====
void DisplayManager::showContextChange(UIContext from, UIContext to) {}
