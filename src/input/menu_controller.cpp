// src/input/menu_controller.cpp
// 菜单和交互控制器实现

#include "menu_controller.h"
#include "../config/game_config.h"
#include <esp_random.h>
#include "../config/food_table.h"
#include "../display/DisplayManager.h"
#include "../core/power_manager.h"

MenuController menuController;

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
    if (inputManager.isDestroyComboTriggered()) {
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
                safeCallback(_callbacks->onContextChange, UI_EVOLUTION, UI_IDLE);
            } else {
                switchContext(UI_IDLE);
            }
            break;
        case UI_SPECIAL_FOOD:
            // combo 动画完成, 进入特殊食物选择
            if (_combo_pending) {
                switchContext(UI_SPECIAL_FOOD);
                safeCallback(_callbacks->onSpecialFoodShow, _sfood_count);
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
    safeCallback(_callbacks->onContextChange, oldCtx, newCtx);
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
        default:
            break;
    }
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
            safeCallback(_callbacks->onStatusOpen, *_pet);
            break;
        case INPUT_POKE:
            doPoke();
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
            safeCallback(_callbacks->onStatusClose);
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
            safeCallback(_callbacks->onFeedCursorMove, _feed.cursor, _feed.selected);
            break;

        case INPUT_FOOD_SLOT_NEXT:
            if (_feed.cursor < FEED_DRAW_COUNT - 1) {
                _feed.cursor++;
            } else {
                _feed.cursor = 0;  // 循环
            }
            safeCallback(_callbacks->onFeedCursorMove, _feed.cursor, _feed.selected);
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
            safeCallback(_callbacks->onSpecialFoodCursor, _sfood_cursor);
            break;

        case INPUT_SFOOD_NEXT:
            if (_sfood_cursor < _sfood_count - 1) {
                _sfood_cursor++;
            } else {
                _sfood_cursor = 0;
            }
            safeCallback(_callbacks->onSpecialFoodCursor, _sfood_cursor);
            break;

        case INPUT_SPECIAL_FOOD_SELECT:
            if (_combo_pending) {
                feedingSystem.applySpecialFood(*_pet, _last_feed_outcome, _sfood_cursor);
                _combo_pending = false;

                // 通知 UI 显示特殊食物确认 (触发 ANIM_MAPO_TOFU 动画)
                safeCallback(_callbacks->onSpecialFoodSelect, _sfood_cursor, _last_feed_outcome);

                // nobu 路线: 如果触发了麻婆豆腐为 Oda Nobunaga
                if (_pet->is_nobu && _last_feed_outcome.mapo_tofu_triggered) {
                    EvolutionResult eR = evolutionSystem.checkNobuMapo(*_pet);
                    if (eR.event != EVO_NONE) {
                        safeCallback(_callbacks->onEvolution, eR, _pet->seriousness);
                    }
                }
                // 普通路线: 如果触发了麻婆诅咒, 在 mapo 动画之后检查进化
                else if (_last_feed_outcome.mapo_tofu_curse_activated) {
                    EvolutionResult eR = evolutionSystem.checkMapoCurse(*_pet);
                    if (eR.event != EVO_NONE) {
                        safeCallback(_callbacks->onEvolution, eR, _pet->seriousness);
                    }
                }

                saveManager.save(*_pet, timeManager.now());
                saveManager.markSaved(timeManager.now());
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
                safeCallback(_callbacks->onDestroyCursorMove, _destroy.cursor);
            }
            break;

        case INPUT_NAV_RIGHT:
            if (_destroy.cursor < 1) {
                _destroy.cursor++;
                safeCallback(_callbacks->onDestroyCursorMove, _destroy.cursor);
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
    safeCallback(_callbacks->onFeedDrawStart, _feed.draw);
}

void MenuController::toggleFoodSlot() {
    uint8_t slot = _feed.cursor;
    if (slot >= FEED_DRAW_COUNT) return;

    if (_feed.selected[slot]) {
        // 取消选取
        _feed.selected[slot] = false;
        _feed.selected_count--;
        safeCallback(_callbacks->onFeedSlotToggle, slot, false);
    } else {
        // 选取
        if (_feed.selected_count >= FEED_PICK_COUNT) {
            // 已选满3个, 不能再选
            return;
        }
        _feed.selected[slot] = true;
        _feed.selected_count++;
        safeCallback(_callbacks->onFeedSlotToggle, slot, true);

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
    safeCallback(_callbacks->onFeedConfirm, outcome, _pet->seriousness);

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
        safeCallback(_callbacks->onEvolution, eR, _pet->seriousness);
    }

    // 保存
    saveManager.save(*_pet, timeManager.now());
    saveManager.markSaved(now);
}

void MenuController::cancelFeed() {
    _feed.active = false;
    _feed.selected_count = 0;
    for (uint8_t i = 0; i < 4; i++) _feed.selected[i] = false;
    switchContext(UI_IDLE);
    safeCallback(_callbacks->onFeedCancel);
}

void MenuController::doPoke() {
    if (!_pet) return;
    if (!evolutionSystem.canInteract(*_pet)) return;

    uint32_t now = timeManager.now();

    // 切换到戳一下动画
    // 动画完成后 DisplayManager 调用 onAnimationComplete(UI_POKE_ANIM)
    switchContext(UI_POKE_ANIM);
    safeCallback(_callbacks->onPokeStart);

    // 执行戳一下逻辑
    InteractResult sR = seriousnessSystem.onInteract(*_pet, INTERACT_POKE, now);
    bool valueChanged = (sR.seriousness_before != sR.seriousness_after);

    // 检查进化
    EvolutionResult eR = evolutionSystem.check(*_pet, now);

    safeCallback(_callbacks->onPokeResult, valueChanged, sR.seriousness_before, sR.seriousness_after);

    if (eR.event != EVO_NONE) {
        safeCallback(_callbacks->onEvolution, eR, _pet->seriousness);
    }
}

void MenuController::enterDestroyConfirm() {
    _destroy.active = true;
    _destroy.cursor = 1;  // 默认选择 "no" (安全)
    switchContext(UI_DESTROY_CONFIRM);
    safeCallback(_callbacks->onDestroyConfirmShow, _destroy.cursor);
}

void MenuController::executeDestroy() {
    if (!_pet) return;

    uint32_t now = timeManager.now();
    Form destroyedForm = _pet->form;
    uint16_t prevAgeDays = _pet->age_days;

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

    feedingSystem.resetDaily(*_pet, timeManager.getDay());

    _feed.active = false;
    _combo_pending = false;
    _destroy.active = false;

    saveManager.save(*_pet, timeManager.now());
    saveManager.markSaved(now);

    safeCallback(_callbacks->onDestroyExecuted, destroyedForm);
    switchContext(UI_IDLE);
}

void MenuController::cancelDestroy() {
    _destroy.active = false;
    safeCallback(_callbacks->onDestroyCancelled);
    switchContext(UI_IDLE);
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
