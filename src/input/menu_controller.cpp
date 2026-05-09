// src/input/menu_controller.cpp
// 菜单和交互控制器实现

#include "menu_controller.h"
#include "../config/game_config.h"
#include <esp_random.h>
#include "../config/food_table.h"
#include "../display/DisplayManager.h"
#include "../core/power_manager.h"

MenuController menuController;

static uint8_t daysInMonthForSetup(uint16_t year, uint8_t month) {
    if (month < 1 || month > 12) return 30;
    if (month == 2) {
        bool leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
        return leap ? 29 : 28;
    }
    static const uint8_t days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month - 1];
}

void MenuController::init(PetState* pet, UICallbacks* callbacks) {
    _pet = pet;
    _callbacks = callbacks;

    _feed.active = false;
    _feed.cursor = 0;
    _feed.selected_count = 0;
    for (uint8_t i = 0; i < 4; i++) _feed.selected[i] = false;

    _destroy.active = false;
    _destroy.cursor = 1;  // 默认选择 "no"

    _sfood_cursor = 0;
    _sfood_count = 0;
    _combo_pending = false;
    _initialTime.active = false;
    _initialTime.year = 2026;
    _initialTime.month = 1;
    _initialTime.day = 1;
    _initialTime.hour = 0;
    _initialTime.minute = 0;
    _initialTime.fieldIndex = 0;
    _initialTime.awaitingConfirm = false;
    _initialTime.leftLockedUntilRelease = false;

    inputManager.init();
}

void MenuController::update() {
    // 更新物理按键 (刷新硬件状态)
    inputManager.update();

    // 检查 display 是否阻止输入 (例如动画/boot/evolution), 如果是则返回
    if (DisplayManager::isPageBlockingInput()) {
        return;
    }

    // 优先检测三键销毁组合
    if (inputManager.getContext() != UI_TIME_SETUP && inputManager.isDestroyComboTriggered()) {
        if (!_destroy.active) {
            enterDestroyConfirm();
        }
        inputManager.resetDestroyCombo();
        return;
    }

    // 处理普通按键事件
    uint8_t count = inputManager.getActionCount();
    const GameInput* actions = inputManager.getActions();
    if (count > 0) {
        // 任何用户操作都通知电源管理器
        powerManager.onUserActivity();
    }
    for (uint8_t i = 0; i < count; i++) {
        handleAction(actions[i]);
    }
}

void MenuController::onAnimationComplete(UIContext animContext) {
    switch (animContext) {
        case UI_FEED_DRAW:
            // 食物绘制完成, 进入选择界面
            switchContext(UI_FEED_PICK);
            // 同步首帧 cursor：showFeedDraw() 会将 DisplayModel.feedCursor 重置为 0，
            // 若不在切到 PAGE_FEED_PICK 时补一次刷新，则 UI 首帧会显示错误的指针位置，
            // 直到用户按键触发 onFeedCursorMove 才会对齐。
            safeCallback(&UICallbacks::onFeedCursorMove, _feed.cursor, _feed.selected);
            break;
        case UI_POKE_ANIM:
            // 戳一下动画完成, 回到主界面
            switchContext(UI_IDLE);
            break;
        case UI_EVOLUTION:
            // 进化动画完成, 回到主界面
            // 注意: 如果当前已经是 UI_IDLE (例如 Nobu 进化路线中先切回了 IDLE),
            // switchContext 会因为 oldCtx == newCtx 而跳过, 导致 PAGE_EVOLUTION 不被清除.
            // 因此需要强制通知 UI 切换页面.
            if (inputManager.getContext() == UI_IDLE) {
                safeCallback(&UICallbacks::onContextChange, UI_EVOLUTION, UI_IDLE);
            } else {
                switchContext(UI_IDLE);
            }
            break;
        case UI_SPECIAL_FOOD:
            // combo 动画完成, 进入特殊食物选择
            if (_combo_pending) {
                switchContext(UI_SPECIAL_FOOD);
                safeCallback(&UICallbacks::onSpecialFoodShow, _sfood_count);
            }
            break;
        default:
            break;
    }
}

void MenuController::switchContext(UIContext newCtx) {
    UIContext oldCtx = inputManager.getContext();
    if (oldCtx == newCtx) return;
    inputManager.setContext(newCtx);
    safeCallback(&UICallbacks::onContextChange, oldCtx, newCtx);
}

void MenuController::handleAction(GameInput action) {
    UIContext ctx = inputManager.getContext();
    switch (ctx) {
        case UI_IDLE:
            handleIdle(action);
            break;
        case UI_STATUS:
            handleStatus(action);
            break;
        case UI_FEED_PICK:
            handleFeedPick(action);
            break;
        case UI_SPECIAL_FOOD:
            handleSpecialFood(action);
            break;
        case UI_DESTROY_CONFIRM:
            handleDestroyConfirm(action);
            break;
        case UI_GALLERY:
            handleGallery(action);
            break;
        case UI_TIME_SETUP:
            handleTimeSetup(action);
            break;
        default:
            break;
    }
}

void MenuController::startInitialTimeSetup(uint32_t baseEpoch) {
    TimeInfo t = timeManager.epochToTimeInfo(baseEpoch);
    _initialTime.active = true;
    _initialTime.year = t.year;
    _initialTime.month = t.month;
    _initialTime.day = t.day;
    _initialTime.hour = t.hour;
    _initialTime.minute = t.minute;
    _initialTime.fieldIndex = 0;
    _initialTime.awaitingConfirm = false;
    _initialTime.leftLockedUntilRelease = false;
    switchContext(UI_TIME_SETUP);
    emitInitialTimeEdit();
}

// ============================================================================
//  UI_IDLE 处理
// ============================================================================

void MenuController::handleIdle(GameInput action) {
    switch (action) {
        case INPUT_FEED_START:
            startFeed();
            break;
        case INPUT_STATUS_VIEW:
            switchContext(UI_STATUS);
            safeCallback(&UICallbacks::onStatusOpen, *_pet);
            break;
        case INPUT_POKE:
            doPoke();
            break;
        case INPUT_GALLERY_OPEN:
            gallerySystem.open();
            switchContext(UI_GALLERY);
            DisplayManager::switchPage(PAGE_GALLERY);
            DisplayManager::showGalleryGrid(
                gallerySystem.getBrowseState().current_page,
                gallerySystem.getBrowseState().selected_index);
            break;
        default:
            break;
    }
}

// ============================================================================
//  UI_STATUS 处理
// ============================================================================

void MenuController::handleStatus(GameInput action) {
    switch (action) {
        case INPUT_STATUS_VIEW:
            // 再次按下则关闭状态界面
            switchContext(UI_IDLE);
            safeCallback(&UICallbacks::onStatusClose);
            break;
        case INPUT_GALLERY_OPEN:
            // 长按中键: 从状态面板直接进入图鉴
            // (处理长按时PRESS已先触发打开了状态面板的情况)
            safeCallback(&UICallbacks::onStatusClose);
            gallerySystem.open();
            switchContext(UI_GALLERY);
            DisplayManager::switchPage(PAGE_GALLERY);
            DisplayManager::showGalleryGrid(
                gallerySystem.getBrowseState().current_page,
                gallerySystem.getBrowseState().selected_index);
            break;
        default:
            break;
    }
}

// ============================================================================
//  UI_FEED_PICK 处理
// ============================================================================

void MenuController::handleFeedPick(GameInput action) {
    switch (action) {
        case INPUT_FOOD_SLOT_PREV:
            if (_feed.cursor > 0) {
                _feed.cursor--;
            } else {
                _feed.cursor = FEED_DRAW_COUNT - 1;  // 循环
            }
            safeCallback(&UICallbacks::onFeedCursorMove, _feed.cursor, _feed.selected);
            break;

        case INPUT_FOOD_SLOT_NEXT:
            if (_feed.cursor < FEED_DRAW_COUNT - 1) {
                _feed.cursor++;
            } else {
                _feed.cursor = 0;  // 循环
            }
            safeCallback(&UICallbacks::onFeedCursorMove, _feed.cursor, _feed.selected);
            break;

        case INPUT_FEED_PICK_TOGGLE:
            toggleFoodSlot();
            break;

        case INPUT_CANCEL:
            cancelFeed();
            break;

        default:
            break;
    }
}

// ============================================================================
//  UI_SPECIAL_FOOD 处理
// ============================================================================

void MenuController::handleSpecialFood(GameInput action) {
    switch (action) {
        case INPUT_SFOOD_PREV:
            if (_sfood_cursor > 0) {
                _sfood_cursor--;
            } else {
                _sfood_cursor = _sfood_count - 1;
            }
            safeCallback(&UICallbacks::onSpecialFoodCursor, _sfood_cursor);
            break;

        case INPUT_SFOOD_NEXT:
            if (_sfood_cursor < _sfood_count - 1) {
                _sfood_cursor++;
            } else {
                _sfood_cursor = 0;
            }
            safeCallback(&UICallbacks::onSpecialFoodCursor, _sfood_cursor);
            break;

        case INPUT_SPECIAL_FOOD_SELECT:
            if (_combo_pending) {
                feedingSystem.applySpecialFood(*_pet, _last_feed_outcome, _sfood_cursor);
                _combo_pending = false;

                // 通知 UI 显示特殊食物确认 (触发 ANIM_MAPO_TOFU 动画)
                safeCallback(&UICallbacks::onSpecialFoodSelect, _sfood_cursor, _last_feed_outcome);

                // nobu 路线: 如果触发了麻婆豆腐为 Oda Nobunaga
                if (_pet->is_nobu && _last_feed_outcome.mapo_tofu_triggered) {
                    EvolutionResult eR = evolutionSystem.checkNobuMapo(*_pet);
                    if (eR.event != EVO_NONE) {
                        safeCallback(&UICallbacks::onEvolution, eR, _pet->seriousness);
                    }
                }
                // 普通路线: 如果触发了麻婆诅咒, 在 mapo 动画之后检查进化
                else if (_last_feed_outcome.mapo_tofu_curse_activated) {
                    EvolutionResult eR = evolutionSystem.checkMapoCurse(*_pet);
                    if (eR.event != EVO_NONE) {
                        safeCallback(&UICallbacks::onEvolution, eR, _pet->seriousness);
                    }
                }

                uint32_t ts = timeManager.now();
                SaveResult sr = saveManager.save(*_pet, ts);
                if (sr == SAVE_OK) {
                    saveManager.markSaved(ts);
                } else {
                    Serial.printf("[MC] WARN: special-food save failed (%d)\n", (int)sr);
                }
                switchContext(UI_IDLE);
            }
            break;

        default:
            break;
    }
}

// ============================================================================
//  UI_DESTROY_CONFIRM 处理
// ============================================================================

void MenuController::handleDestroyConfirm(GameInput action) {
    switch (action) {
        case INPUT_NAV_LEFT:
            if (_destroy.cursor > 0) {
                _destroy.cursor--;
                safeCallback(&UICallbacks::onDestroyCursorMove, _destroy.cursor);
            }
            break;

        case INPUT_NAV_RIGHT:
            if (_destroy.cursor < 1) {
                _destroy.cursor++;
                safeCallback(&UICallbacks::onDestroyCursorMove, _destroy.cursor);
            }
            break;

        case INPUT_CONFIRM:
            if (_destroy.cursor == 0) {
                // yes - 执行销毁
                executeDestroy();
            } else {
                // no - 取消
                cancelDestroy();
            }
            break;

        default:
            break;
    }
}

// ============================================================================
//  业务逻辑
// ============================================================================

void MenuController::startFeed() {
    if (!_pet) return;
    if (!evolutionSystem.canInteract(*_pet)) return;

    uint32_t now = timeManager.now();
    FeedResult check = feedingSystem.canFeed(*_pet, now);
    if (check != FEED_OK) return;

    // 随机抽取4个食物
    _feed.draw = feedingSystem.drawFood();
    _feed.cursor = 1;  // 默认选中中间偏左 (4个位: 0,1,2,3, 中间为1和2)
    _feed.selected_count = 0;
    _feed.active = true;
    for (uint8_t i = 0; i < 4; i++) _feed.selected[i] = false;

    // 切换到食物绘制显示界面 (触发动画)
    // 动画完成后 DisplayManager 调用 onAnimationComplete(UI_FEED_DRAW)
    switchContext(UI_FEED_DRAW);
    safeCallback(&UICallbacks::onFeedDrawStart, _feed.draw);
}

void MenuController::toggleFoodSlot() {
    uint8_t slot = _feed.cursor;
    if (slot >= FEED_DRAW_COUNT) return;

    if (_feed.selected[slot]) {
        // 取消选取
        _feed.selected[slot] = false;
        _feed.selected_count--;
        safeCallback(&UICallbacks::onFeedSlotToggle, slot, false);
    } else {
        // 选取
        if (_feed.selected_count >= FEED_PICK_COUNT) {
            // 已选满3个, 不能再选
            return;
        }
        _feed.selected[slot] = true;
        _feed.selected_count++;
        safeCallback(&UICallbacks::onFeedSlotToggle, slot, true);

        // 选满3个则自动提交
        if (_feed.selected_count >= FEED_PICK_COUNT) {
            confirmFeed();
        }
    }
}

void MenuController::confirmFeed() {
    if (!_pet) return;
    if (_feed.selected_count < FEED_PICK_COUNT) return;

    // 收集已选食物ID
    uint8_t picked[3];
    uint8_t pickIdx = 0;
    for (uint8_t i = 0; i < FEED_DRAW_COUNT && pickIdx < FEED_PICK_COUNT; i++) {
        if (_feed.selected[i]) {
            picked[pickIdx++] = _feed.draw.food_ids[i];
        }
    }

    uint32_t now = timeManager.now();
    uint8_t hour = timeManager.getHour();

    // 执行投喂
    FeedOutcome outcome = feedingSystem.feed(*_pet, picked, now, hour);
    if (outcome.result != FEED_OK) {
        _feed.active = false;
        switchContext(UI_IDLE);
        return;
    }

    // 更新严肃值
    InteractResult sR = seriousnessSystem.onInteract(*_pet, INTERACT_FEED, now);

    // 检查进化
    EvolutionResult eR = evolutionSystem.check(*_pet, now);

    // 通知UI: 投喂完成 seriousness
    safeCallback(&UICallbacks::onFeedConfirm, outcome, _pet->seriousness);

    // 处理组合
    if (outcome.combo_triggered) {
        _last_feed_outcome = outcome;
        _combo_pending = true;
        _sfood_cursor = 0;
        _sfood_count = SFOOD_COUNT;

        // 不直接切换到 UI_SPECIAL_FOOD, 让 combo 动画完成后通过 onAnimationComplete 切换
        // 先切回 idle 状态 (这样不会被其他输入打断)
        switchContext(UI_IDLE);
    } else {
        _feed.active = false;
        switchContext(UI_IDLE);
    }

    // 处理进化
    if (eR.event != EVO_NONE) {
        safeCallback(&UICallbacks::onEvolution, eR, _pet->seriousness);
    }

    SaveResult sr = saveManager.save(*_pet, timeManager.now());
    if (sr == SAVE_OK) {
        saveManager.markSaved(now);
    } else {
        Serial.printf("[MC] WARN: feed save failed (%d)\n", (int)sr);
    }
}

void MenuController::cancelFeed() {
    _feed.active = false;
    _feed.selected_count = 0;
    for (uint8_t i = 0; i < 4; i++) _feed.selected[i] = false;
    switchContext(UI_IDLE);
    safeCallback(&UICallbacks::onFeedCancel);
}

void MenuController::doPoke() {
    if (!_pet) return;
    if (!evolutionSystem.canInteract(*_pet)) return;

    uint32_t now = timeManager.now();

    // 切换到戳一下动画
    // 动画完成后 DisplayManager 调用 onAnimationComplete(UI_POKE_ANIM)
    switchContext(UI_POKE_ANIM);
    safeCallback(&UICallbacks::onPokeStart);

    // 执行戳一下逻辑
    InteractResult sR = seriousnessSystem.onInteract(*_pet, INTERACT_POKE, now);
    bool valueChanged = (sR.seriousness_before != sR.seriousness_after);

    // 检查进化
    EvolutionResult eR = evolutionSystem.check(*_pet, now);

    safeCallback(&UICallbacks::onPokeResult, valueChanged, sR.seriousness_before, sR.seriousness_after);

    if (eR.event != EVO_NONE) {
        safeCallback(&UICallbacks::onEvolution, eR, _pet->seriousness);
    }

    // 关键：poke 会更新冷却与待机暂停时间戳，需立即持久化以避免断电后离线补偿失真
    SaveResult sr = saveManager.save(*_pet, timeManager.now());
    if (sr == SAVE_OK) {
        saveManager.markSaved(now);
    } else {
        Serial.printf("[MC] WARN: poke save failed (%d)\n", (int)sr);
    }
}

void MenuController::enterDestroyConfirm() {
    _destroy.active = true;
    _destroy.cursor = 1;  // 默认选择 "no" (安全)
    switchContext(UI_DESTROY_CONFIRM);
    safeCallback(&UICallbacks::onDestroyConfirmShow, _destroy.cursor);
}

void MenuController::executeDestroy() {
    if (!_pet) return;

    uint32_t now = timeManager.now();
    Form destroyedForm = _pet->form;
    uint16_t prevAgeDays = _pet->age_days;
    uint16_t prevRounds = _pet->rounds;

    // 指定轮次保底: round 329 触发 reset 后 (即 rounds=330) 必定进入 Nobu 路线
    if (((prevRounds > 0) ? prevRounds : 1) == 329) {
        Serial.println("[MC] Forced Nobu: round 329 -> round 330");
        evolutionSystem.destroyToNobu(*_pet, now, prevAgeDays);
        _pet->rounds = 330;
    } else {
    // nobu 路线判定
    uint32_t roll = esp_random() % 1000;
    uint32_t threshold = NOBU_BASE_PERMILLE;
    if (prevAgeDays == 5) {   // 仅第6天概率翻倍
        threshold = NOBU_DAY6_PERMILLE;
    }

    Serial.println("[MC] 开始进行 Nobu 路线判定...");
    Serial.printf("[MC] 当前随机值: %lu, 目标阈值: %lu (当 roll < threshold 时触发)\n", roll, threshold);

    if (roll < threshold) {
        Serial.println("[MC] 路线判定结果: 成功! 进入 Nobu 路线");
        evolutionSystem.destroyToNobu(*_pet, now, prevAgeDays);
        Serial.printf("[MC] NOBU triggered! (roll=%lu, threshold=%lu)\n", roll, threshold);
    } else {
        Serial.println("[MC] 路线判定结果: 失败, 宠物进化为 Lily");
        evolutionSystem.destroy(*_pet, now);
    }
    _pet->rounds = ((prevRounds > 0) ? prevRounds : 1) + 1;
    }

    feedingSystem.resetDaily(*_pet, timeManager.getDay());

    // 销毁/重置后当前形态也应计入图鉴
    if (gallerySystem.unlockForm(_pet->form)) {
        SaveResult g = saveManager.saveGallery(gallerySystem.getData());
        if (g != SAVE_OK) {
            Serial.println("[MC] Gallery save failed after destroy/reset");
        }
    }

    _feed.active = false;
    _combo_pending = false;
    _destroy.active = false;

    SaveResult sr = saveManager.save(*_pet, timeManager.now());
    if (sr == SAVE_OK) {
        saveManager.markSaved(now);
    } else {
        Serial.printf("[MC] WARN: destroy save failed (%d)\n", (int)sr);
    }

    safeCallback(&UICallbacks::onDestroyExecuted, destroyedForm);
    switchContext(UI_IDLE);
}

void MenuController::cancelDestroy() {
    _destroy.active = false;
    safeCallback(&UICallbacks::onDestroyCancelled);
    switchContext(UI_IDLE);
}

// ============================================================================
//  UI_GALLERY 处理
// ============================================================================

void MenuController::handleGallery(GameInput action) {
    switch (action) {
        case INPUT_GALLERY_CLOSE:
            gallerySystem.close();
            switchContext(UI_IDLE);
            break;

        case INPUT_GALLERY_NAV_LEFT:
            gallerySystem.navigateLeft();
            DisplayManager::showGalleryGrid(
                gallerySystem.getBrowseState().current_page,
                gallerySystem.getBrowseState().selected_index);
            break;

        case INPUT_GALLERY_NAV_RIGHT:
            gallerySystem.navigateRight();
            DisplayManager::showGalleryGrid(
                gallerySystem.getBrowseState().current_page,
                gallerySystem.getBrowseState().selected_index);
            break;

        default:
            break;
    }
}

void MenuController::handleTimeSetup(GameInput action) {
    if (!_initialTime.active) return;
    switch (action) {
        case INPUT_TIMESET_RESET_DEFAULT:
            _initialTime.year = 2026;
            _initialTime.month = 1;
            _initialTime.day = 1;
            _initialTime.hour = 0;
            _initialTime.minute = 0;
            _initialTime.fieldIndex = 0;
            _initialTime.awaitingConfirm = false;
            _initialTime.leftLockedUntilRelease = true;
            emitInitialTimeEdit();
            break;
        case INPUT_TIMESET_LEFT_RELEASE:
            _initialTime.leftLockedUntilRelease = false;
            break;
        case INPUT_TIMESET_INC:
            if (_initialTime.leftLockedUntilRelease) break;
            if (_initialTime.awaitingConfirm) break;
            incrementInitialTimeField();
            emitInitialTimeEdit();
            break;
        case INPUT_TIMESET_NEXT:
            if (_initialTime.fieldIndex < 4) {
                _initialTime.fieldIndex++;
                _initialTime.awaitingConfirm = false;
                emitInitialTimeEdit();
            } else {
                if (!_initialTime.awaitingConfirm) {
                    _initialTime.awaitingConfirm = true;
                    emitInitialTimeEdit();
                } else {
                    _initialTime.active = false;
                    _initialTime.awaitingConfirm = false;
                    safeCallback(&UICallbacks::onInitialTimeConfirm,
                                 _initialTime.year, _initialTime.month, _initialTime.day,
                                 _initialTime.hour, _initialTime.minute);
                }
            }
            break;
        case INPUT_TIMESET_BACK:
            if (_initialTime.awaitingConfirm) {
                _initialTime.awaitingConfirm = false;
                emitInitialTimeEdit();
            } else if (_initialTime.fieldIndex > 0) {
                _initialTime.fieldIndex--;
                emitInitialTimeEdit();
            }
            break;
        default:
            break;
    }
}

void MenuController::emitInitialTimeEdit() {
    safeCallback(&UICallbacks::onInitialTimeEdit,
                 _initialTime.year, _initialTime.month, _initialTime.day,
                 _initialTime.hour, _initialTime.minute, _initialTime.fieldIndex,
                 _initialTime.awaitingConfirm);
}

void MenuController::incrementInitialTimeField() {
    switch (_initialTime.fieldIndex) {
        case 0:
            _initialTime.year++;
            if (_initialTime.year > 2099) _initialTime.year = 2000;
            break;
        case 1:
            _initialTime.month++;
            if (_initialTime.month > 12) _initialTime.month = 1;
            break;
        case 2: {
            uint8_t dim = daysInMonthForSetup(_initialTime.year, _initialTime.month);
            _initialTime.day++;
            if (_initialTime.day > dim) _initialTime.day = 1;
            break;
        }
        case 3:
            _initialTime.hour = (uint8_t)((_initialTime.hour + 1) % 24);
            break;
        case 4:
            _initialTime.minute = (uint8_t)((_initialTime.minute + 1) % 60);
            break;
        default:
            break;
    }
    uint8_t dim = daysInMonthForSetup(_initialTime.year, _initialTime.month);
    if (_initialTime.day > dim) _initialTime.day = dim;
}

void MenuController::injectButton(ButtonId btn, ButtonEventType type) {
    // 注入事件到 ButtonDriver
    buttonDriver.injectEvent(btn, type, (type == BTN_EVENT_LONG_PRESS) ? BTN_LONG_PRESS_MS : 0);

    // 通过 InputManager 映射 (绕过 GPIO 读取)
    inputManager.processInjected();

    // 获取映射后的 GameInput 动作
    uint8_t count = inputManager.getActionCount();
    const GameInput* actions = inputManager.getActions();
    for (uint8_t i = 0; i < count; i++) {
        handleAction(actions[i]);
    }
}
