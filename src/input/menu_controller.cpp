// src/input/menu_controller.cpp
// �˵��뽻��������ʵ��

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
    _destroy.cursor = 1;  // Ĭ��ѡ�� "no"

    _sfood_cursor = 0;
    _sfood_count = 0;
    _combo_pending = false;

    inputManager.init();
}

void MenuController::update() {
    // ������������� (ˢ��Ӳ��״̬)
    inputManager.update();

    // ��� display �������������� (����������/boot/evolution), ����������
    if (DisplayManager::isPageBlockingInput()) {
        return;
    }

    // ���ȼ�������������
    if (inputManager.isDestroyComboTriggered()) {
        if (!_destroy.active) {
            enterDestroyConfirm();
        }
        inputManager.resetDestroyCombo();
        return;
    }

    // ������ͨ���붯��
    uint8_t count = inputManager.getActionCount();
    const GameInput* actions = inputManager.getActions();
    if (count > 0) {
        // �κΰ������֪ͨ��Դ������
        powerManager.onUserActivity();
    }
    for (uint8_t i = 0; i < count; i++) {
        handleAction(actions[i]);
    }
}

void MenuController::onAnimationComplete(UIContext animContext) {
    switch (animContext) {
        case UI_FEED_DRAW:
            // �鿨�������, ����ѡ�����
            switchContext(UI_FEED_PICK);
            break;
        case UI_POKE_ANIM:
            // ��һ���������, �ص�������
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
            // combo �������, ��������ʳ��ѡ��
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
//  UI_IDLE ����
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
//  UI_STATUS ����
// ============================================================================

void MenuController::handleStatus(GameInput action) {
    switch (action) {
        case INPUT_STATUS_VIEW:
            // �ٴΰ��м��ر�״̬���
            switchContext(UI_IDLE);
            safeCallback(_callbacks->onStatusClose);
            break;
        default:
            break;
    }
}

// ============================================================================
//  UI_FEED_PICK ����
// ============================================================================

void MenuController::handleFeedPick(GameInput action) {
    switch (action) {
        case INPUT_FOOD_SLOT_PREV:
            if (_feed.cursor > 0) {
                _feed.cursor--;
            } else {
                _feed.cursor = FEED_DRAW_COUNT - 1;  // ѭ��
            }
            safeCallback(_callbacks->onFeedCursorMove, _feed.cursor, _feed.selected);
            break;

        case INPUT_FOOD_SLOT_NEXT:
            if (_feed.cursor < FEED_DRAW_COUNT - 1) {
                _feed.cursor++;
            } else {
                _feed.cursor = 0;  // ѭ��
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
//  UI_SPECIAL_FOOD ����
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

                // ��֪ͨ UI ��ʾ����ʳ��ȷ�� (������ ANIM_MAPO_TOFU �������)
                safeCallback(_callbacks->onSpecialFoodSelect, _sfood_cursor, _last_feed_outcome);

                // nobu ·��: ���Ŷ�����������Ϊ Oda Nobunaga
                if (_pet->is_nobu && _last_feed_outcome.mapo_tofu_triggered) {
                    EvolutionResult eR = evolutionSystem.checkNobuMapo(*_pet);
                    if (eR.event != EVO_NONE) {
                        safeCallback(_callbacks->onEvolution, eR, _pet->seriousness);
                    }
                }
                // ����·��: ����������伤��, �� mapo ����֮���ŶӲ��Ž�������
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
//  UI_DESTROY_CONFIRM ����
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
                // yes - ִ������
                executeDestroy();
            } else {
                // no - ȡ��
                cancelDestroy();
            }
            break;

        default:
            break;
    }
}

// ============================================================================
//  ҵ���߼�
// ============================================================================

void MenuController::startFeed() {
    if (!_pet) return;
    if (!evolutionSystem.canInteract(*_pet)) return;

    uint32_t now = timeManager.now();
    FeedResult check = feedingSystem.canFeed(*_pet, now);
    if (check != FEED_OK) return;

    // ��ȡ4��ʳ��
    _feed.draw = feedingSystem.drawFood();
    _feed.cursor = 1;  // Ĭ�Ϲ�����м�ƫ�� (4�ſ�: 0,1,2,3, �м�Ϊ1��2)
    _feed.selected_count = 0;
    _feed.active = true;
    for (uint8_t i = 0; i < 4; i++) _feed.selected[i] = false;

    // �л����鿨չʾ���� (���Ŷ���)
    // ��������� DisplayManager ���� onAnimationComplete(UI_FEED_DRAW)
    switchContext(UI_FEED_DRAW);
    safeCallback(_callbacks->onFeedDrawStart, _feed.draw);
}

void MenuController::toggleFoodSlot() {
    uint8_t slot = _feed.cursor;
    if (slot >= FEED_DRAW_COUNT) return;

    if (_feed.selected[slot]) {
        // ȡ��ѡȡ
        _feed.selected[slot] = false;
        _feed.selected_count--;
        safeCallback(_callbacks->onFeedSlotToggle, slot, false);
    } else {
        // ѡȡ
        if (_feed.selected_count >= FEED_PICK_COUNT) {
            // ��ѡ��3��, ������ѡ
            return;
        }
        _feed.selected[slot] = true;
        _feed.selected_count++;
        safeCallback(_callbacks->onFeedSlotToggle, slot, true);

        // ѡ��3�����Զ��ύ
        if (_feed.selected_count >= FEED_PICK_COUNT) {
            confirmFeed();
        }
    }
}

void MenuController::confirmFeed() {
    if (!_pet) return;
    if (_feed.selected_count < FEED_PICK_COUNT) return;

    // �ռ���ѡʳ��ID
    uint8_t picked[3];
    uint8_t pickIdx = 0;
    for (uint8_t i = 0; i < FEED_DRAW_COUNT && pickIdx < FEED_PICK_COUNT; i++) {
        if (_feed.selected[i]) {
            picked[pickIdx++] = _feed.draw.food_ids[i];
        }
    }

    uint32_t now = timeManager.now();
    uint8_t hour = timeManager.getHour();

    // ִ��Ͷι
    FeedOutcome outcome = feedingSystem.feed(*_pet, picked, now, hour);
    if (outcome.result != FEED_OK) {
        _feed.active = false;
        switchContext(UI_IDLE);
        return;
    }

    // ����ֵ����
    InteractResult sR = seriousnessSystem.onInteract(*_pet, INTERACT_FEED, now);

    // �������
    EvolutionResult eR = evolutionSystem.check(*_pet, now);

    // ֪ͨUI: �������� seriousness
    safeCallback(_callbacks->onFeedConfirm, outcome, _pet->seriousness);

    // �����Я
    if (outcome.combo_triggered) {
        _last_feed_outcome = outcome;
        _combo_pending = true;
        _sfood_cursor = 0;
        _sfood_count = SFOOD_COUNT;

        // �������л��� UI_SPECIAL_FOOD, �� combo ������������ onAnimationComplete ����
        // ���л� idle ������ (��������������ᱻ����)
        switchContext(UI_IDLE);
    } else {
        _feed.active = false;
        switchContext(UI_IDLE);
    }

    // �����¼�
    if (eR.event != EVO_NONE) {
        safeCallback(_callbacks->onEvolution, eR, _pet->seriousness);
    }

    // �浵
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

    // �л�����һ������
    // ��������� DisplayManager ���� onAnimationComplete(UI_POKE_ANIM)
    switchContext(UI_POKE_ANIM);
    safeCallback(_callbacks->onPokeStart);

    // ִ�д�һ���߼�
    InteractResult sR = seriousnessSystem.onInteract(*_pet, INTERACT_POKE, now);
    bool valueChanged = (sR.seriousness_before != sR.seriousness_after);

    // �������
    EvolutionResult eR = evolutionSystem.check(*_pet, now);

    safeCallback(_callbacks->onPokeResult, valueChanged, sR.seriousness_before, sR.seriousness_after);

    if (eR.event != EVO_NONE) {
        safeCallback(_callbacks->onEvolution, eR, _pet->seriousness);
    }
}

void MenuController::enterDestroyConfirm() {
    _destroy.active = true;
    _destroy.cursor = 1;  // Ĭ��ѡ�� "no" (��ȫ)
    switchContext(UI_DESTROY_CONFIRM);
    safeCallback(_callbacks->onDestroyConfirmShow, _destroy.cursor);
}

void MenuController::executeDestroy() {
    if (!_pet) return;

    uint32_t now = timeManager.now();
    Form destroyedForm = _pet->form;
    uint16_t prevAgeDays = _pet->age_days;

    // nobu �ʵ��ж�
    uint32_t roll = esp_random() % 1000;
    uint32_t threshold = 25;  // 2.5%
    if (prevAgeDays >= 5) {   // ��6��
        threshold = 50;       // 5%
    }

    Serial.println("[MC] ��ʼ���� Nobu ��������...");
    Serial.printf("[MC] ��ǰ�����: %lu, Ŀ����ֵ: %lu (�� roll < threshold �Ŵ���)\n", roll, threshold);

    if (roll < threshold) {
        Serial.println("[MC] �����ж����: �ɹ�! ���� Nobu ·��");
        evolutionSystem.destroyToNobu(*_pet, now, prevAgeDays);
        Serial.printf("[MC] NOBU triggered! (roll=%lu, threshold=%lu)\n", roll, threshold);
    } else {
        Serial.println("[MC] �����ж����: ʧ��, ��������Ϊ Lily");
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
    // ע���¼��� ButtonDriver
    buttonDriver.injectEvent(btn, type, (type == BTN_EVENT_LONG_PRESS) ? BTN_LONG_PRESS_MS : 0);

    // ͨ�� InputManager ӳ�� (���� GPIO ��ȡ)
    inputManager.processInjected();

    // ����ӳ���� GameInput ����
    uint8_t count = inputManager.getActionCount();
    const GameInput* actions = inputManager.getActions();
    for (uint8_t i = 0; i < count; i++) {
        handleAction(actions[i]);
    }
}
