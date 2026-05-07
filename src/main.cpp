#include <Arduino.h>
#include "config/game_config.h"
#include "config/food_table.h"
#include "core/game_state.h"
#include "core/time_manager.h"
#include "core/save_manager.h"
#include "pet/feeding.h"
#include "pet/seriousness.h"
#include "pet/evolution.h"
#include "input/menu_controller.h"
#include "display/DisplayManager.h"

// ============================================================================
//  Serial input debug switch (set to 0 to disable serial button commands)
// ============================================================================
#ifndef ENABLE_SERIAL_INPUT_DEBUG
#define ENABLE_SERIAL_INPUT_DEBUG 1
#endif

static PetState pet;
static char cmdBuf[64];
static uint8_t cmdLen = 0;

// ============================================================================
//  UICallbacks implementation (updates DisplayManager)
// ============================================================================

static UICallbacks gameCallbacks = {
    // onStatusOpen
    [](const PetState& p) {
        Serial.println("[MC] Status opened");
        DisplayManager::showStatusPanel(p);
    },
    // onStatusClose
    []() {
        Serial.println("[MC] Status closed");
        DisplayManager::hideStatusPanel();
    },
    // onFeedDrawStart
    [](const FeedDraw& draw) {
        Serial.println("[MC] Feed draw started");
        DisplayManager::showFeedDraw(draw);
        // Animation complete will be triggered by DisplayManager::update()
    },
    // onFeedCursorMove
    [](uint8_t cursor, const bool selected[4]) {
        Serial.printf("[MC] Feed cursor -> %d\n", cursor);
        DisplayManager::showFeedCursorMove(cursor, selected);
    },
    // onFeedSlotToggle
    [](uint8_t slot, bool sel) {
        Serial.printf("[MC] Feed slot %d %s\n", slot, sel ? "SELECTED" : "DESELECTED");
        DisplayManager::showFeedSlotToggle(slot, sel);
    },
    // onFeedConfirm (new signature: outcome + srAfter)
    [](const FeedOutcome& outcome, int16_t srAfter) {
        Serial.printf("[MC] Feed confirmed! HP: %d->%d  SR after: %d\n",
            outcome.health_before, outcome.health_after, srAfter);
        if (outcome.combo_triggered)
            Serial.printf("[MC] *** COMBO: %s ***\n", COMBO_NAMES[outcome.combo]);
        DisplayManager::showFeedResult(outcome, srAfter);
        if (outcome.combo_triggered) {
            DisplayManager::showFeedComboTriggered(outcome.combo);
        }
    },
    // onFeedCancel
    []() {
        Serial.println("[MC] Feed cancelled");
        DisplayManager::showFeedCancel();
    },
    // onSpecialFoodShow
    [](uint8_t count) {
        Serial.printf("[MC] Special food selection (%d items)\n", count);
        DisplayManager::showSpecialFoodSelection(count);
    },
    // onSpecialFoodCursor
    [](uint8_t cursor) {
        Serial.printf("[MC] Special food cursor -> %d\n", cursor);
        DisplayManager::showSpecialFoodCursor(cursor);
    },
    // onSpecialFoodSelect (new signature: id + full outcome)
    [](uint8_t id, const FeedOutcome& outcome) {
        Serial.printf("[MC] Special food selected: %d (%s)\n", id, SPECIAL_FOOD_TABLE[id].name);
        DisplayManager::showSpecialFoodConfirm(id, outcome);
    },
    // onPokeStart
    []() {
        Serial.println("[MC] Poke animation started");
        DisplayManager::showPokeAnimation();
        // Animation complete will be triggered by DisplayManager::update()
    },
    // onPokeResult
    [](bool valueChanged, int16_t srBefore, int16_t srAfter) {
        if (valueChanged)
            Serial.printf("[MC] Poke result: SR %d->%d\n", srBefore, srAfter);
        else
            Serial.println("[MC] Poke: cooldown (animation only)");
        DisplayManager::showPokeResult(valueChanged, srBefore, srAfter);
    },
    // onDestroyConfirmShow
    [](uint8_t cursor) {
        Serial.printf("[MC] Destroy confirm shown (cursor=%d)\n", cursor);
        DisplayManager::showDestroyConfirm(cursor);
    },
    // onDestroyCursorMove
    [](uint8_t cursor) {
        Serial.printf("[MC] Destroy cursor -> %d (%s)\n", cursor, cursor == 0 ? "YES" : "NO");
        DisplayManager::showDestroyCursorMove(cursor);
    },
    // onDestroyExecuted
    [](Form destroyedForm) {
        Serial.println("[MC] *** DESTROYED ***");
        DisplayManager::showDestroyExecuted(destroyedForm);
    },
    // onDestroyCancelled
    []() {
        Serial.println("[MC] Destroy cancelled");
        DisplayManager::showDestroyCancelled();
    },
    // onEvolution (new signature: result + srAfter)
    [](const EvolutionResult& r, int16_t srAfter) {
        Serial.printf("[MC] Evolution: %s -> %s (SR=%d)\n",
            FORM_NAMES[r.form_before], FORM_NAMES[r.form_after], srAfter);
        DisplayManager::showEvolutionEvent(r, srAfter);
    },
    // onContextChange
    [](UIContext from, UIContext to) {
        Serial.printf("[MC] Context: %s -> %s\n", UI_CONTEXT_NAMES[from], UI_CONTEXT_NAMES[to]);
        // 同步 DisplayPage 与 UIContext
        switch (to) {
            case UI_IDLE:
                // 动画播放中不切页面, 等动画结束后再切
                // Page hold 期间也不切页面, 等 hold 结束后自动切
                if (!DisplayManager::isAnimationPlaying() && !DisplayManager::isPageHoldActive())
                    DisplayManager::switchPage(PAGE_IDLE);
                break;
            case UI_STATUS:
                DisplayManager::switchPage(PAGE_STATUS);
                break;
            case UI_FEED_PICK:
                DisplayManager::switchPage(PAGE_FEED_PICK);
                break;
            case UI_SPECIAL_FOOD:
                DisplayManager::switchPage(PAGE_SPECIAL_FOOD);
                break;
            case UI_DESTROY_CONFIRM:
                DisplayManager::switchPage(PAGE_DESTROY_CONFIRM);
                break;
            default:
                // UI_FEED_DRAW, UI_POKE_ANIM, UI_EVOLUTION 由具体 show*() 负责切页
                break;
        }
    }
};

// ============================================================================
//  Serial command processing
// ============================================================================

void printStatus() {
    char timeBuf[24];
    timeManager.getFormattedFull(timeBuf, sizeof(timeBuf));
    uint32_t now = timeManager.now();

    Serial.println("========================================");
    Serial.printf("  Time:       %s\n", timeBuf);
    Serial.printf("  Form:       %s\n", FORM_NAMES[pet.form]);
    Serial.printf("  Base:       %s\n", FORM_NAMES[pet.base_form]);
    Serial.printf("  Stage:      %s\n", STAGE_NAMES[pet.stage]);
    Serial.printf("  Alignment:  %s\n", ALIGNMENT_NAMES[pet.alignment]);
    Serial.printf("  Health:     %d / %d\n", pet.health, HEALTH_MAX);
    Serial.printf("  Seriousness:%d / %d\n", pet.seriousness, SERIOUSNESS_MAX);
    SeriousnessTier tier = seriousnessSystem.getCurrentTier(pet);
    Serial.printf("  Tier:       %s\n", TIER_NAMES[tier]);
    Serial.printf("  Age:        Day %d", pet.age_days + 1);
    if (pet.stage == STAGE_CHILD)
        Serial.printf(" / %d (child)\n", CHILD_PERIOD_DAYS);
    else
        Serial.println(" (adult)");
    Serial.printf("  Fed today:  %d / %d\n", pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
    if (pet.last_poke_effect_time == 0) {
        Serial.println("  Poke:       available");
    } else {
        uint32_t elapsed = now - pet.last_poke_effect_time;
        if (elapsed >= POKE_COOLDOWN_SEC)
            Serial.println("  Poke:       available");
        else
            Serial.printf("  Poke:       cooldown %lus left\n", POKE_COOLDOWN_SEC - elapsed);
    }
    if (pet.idle_paused_until > now)
        Serial.printf("  Idle pause: %lus left\n", pet.idle_paused_until - now);
    if (pet.mapo_tofu_count > 0)
        Serial.printf("  Mapo Tofu:  %d / %d\n", pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
    if (pet.is_rhongomyniad)
        Serial.println("  *** RHONGOMYNIAD ***");
    if (pet.is_black_rhongomyniad)
        Serial.println("  *** BLACK RHONGOMYNIAD (Mapo Curse) ***");
    if (pet.white_fun_form_locked && pet.alignment == ALIGN_WHITE)
        Serial.printf("  Fun form:   %s (locked)\n", FORM_NAMES[pet.white_fun_form]);
    RhongoTimerState rS = seriousnessSystem.getRhongoState(pet, now);
    if (rS == RHONGO_COUNTING) {
        uint32_t rem = seriousnessSystem.getRhongoRemaining(pet, now);
        Serial.printf("  Rhongo:     %luh left\n", rem / 3600);
    } else if (rS == RHONGO_TRIGGERED) {
        Serial.println("  Rhongo:     TRIGGERED");
    }
    uint32_t wait = feedingSystem.secondsUntilNextFeed(pet, now);
    if (pet.daily_feed.feed_count >= DAILY_FEED_LIMIT)
        Serial.println("  Next feed:  LOCKED");
    else if (wait > 0)
        Serial.printf("  Next feed:  wait %lum\n", wait / 60);
    else
        Serial.println("  Next feed:  READY");
    int8_t wIdx = feedingSystem.getWindowIndex(timeManager.getHour());
    const char* wN[] = {"Breakfast", "Lunch", "Dinner"};
    if (wIdx >= 0) Serial.printf("  Window:     %s\n", wN[wIdx]);
    else Serial.println("  Window:     None");
    Serial.println("========================================");
}

void printHelp() {
    Serial.println("=== Fate Tamagotchi Console ===");
    Serial.println("  s              Status");
    Serial.println("  fl             Food list");
    Serial.println("  t <min>        Advance N minutes");
    Serial.println("  d              Advance 1 day");
    Serial.println("  save / load / erase");
    Serial.println("  reset          Destroy & reset");
    Serial.println("  stime Y M D H m");
    Serial.println("  hp/sr/age <val>  Debug set");
    Serial.println("  grad           Force graduation");
    Serial.println("  mapo           Debug +1 mapo count");
#if ENABLE_SERIAL_INPUT_DEBUG
    Serial.println("--- Button Simulation ---");
    Serial.println("  btn l|m|r      Simulate short press");
    Serial.println("  btnl l|m|r     Simulate long press");
    Serial.println("  btnr l|m|r     Simulate repeat");
    Serial.println("  ctx            Show current UI context");
#endif
    Serial.println("  h              Help");
    Serial.println("===============================");
}

void doDayEnd() {
    Serial.println("[DayEnd] --- Processing ---");
    DisplayManager::showDayEndStart();
    uint32_t now = timeManager.now();

    IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, 1440, now);
    if (iR.tier_changed) {
        Serial.printf("[DayEnd] Idle SR: %d->%d | Tier: %s -> %s\n",
                      iR.seriousness_before, iR.seriousness_after,
                      TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
        DisplayManager::showDayEndIdleSR(iR.seriousness_before, iR.seriousness_after,
                                          iR.tier_before, iR.tier_after);
    }

    if (pet.is_rhongomyniad || pet.is_black_rhongomyniad) {
        Serial.println("[DayEnd] Terminal state reached during idle.");
        DisplayManager::showDayEndTerminalState();
        feedingSystem.resetDaily(pet, timeManager.getDay());
        pet.age_days++;
        saveManager.save(pet);
        saveManager.markSaved(now);
        return;
    }

    DayEndOutcome fO = feedingSystem.processDayEnd(pet);
    if (fO.window_bonus_applied) {
        Serial.printf("[DayEnd] Window bonus +%d HP\n", CORRECT_WINDOW_BONUS);
        DisplayManager::showDayEndWindowBonus(CORRECT_WINDOW_BONUS);
    }
    if (fO.window_penalty_applied) {
        Serial.printf("[DayEnd] Window penalty -%d HP\n", WRONG_WINDOW_PENALTY);
        DisplayManager::showDayEndWindowPenalty(WRONG_WINDOW_PENALTY);
    }
    if (fO.missed_feed_penalty) {
        int16_t srBefore = pet.seriousness;
        seriousnessSystem.applyMissedFeedPenalty(pet);
        Serial.printf("[DayEnd] Missed feeds (%d/%d)\n",
                      pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
        DisplayManager::showDayEndMissedFeed(pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
        DisplayManager::showMissedFeedPenalty(srBefore, pet.seriousness, MISSED_FEED_SERIOUSNESS);
    }

    pet.age_days++;

    EvolutionResult evo = evolutionSystem.checkChildGraduation(pet);
    if (evo.event == EVO_CHILD_TO_WHITE || evo.event == EVO_CHILD_TO_BLACK) {
        Serial.printf("[DayEnd] Graduation: %s\n", EVO_EVENT_NAMES[evo.event]);
        DisplayManager::showChildGraduation(evo, pet.alignment);
    } else {
        // 只有非毕业情况才检查成年进化
        if (evo.event == EVO_NONE && pet.stage == STAGE_ADULT)
            evo = evolutionSystem.check(pet, now);
        if (evo.event != EVO_NONE) {
            Serial.printf("[DayEnd] Evo: %s\n", EVO_EVENT_NAMES[evo.event]);
            DisplayManager::showEvolutionEvent(evo, pet.seriousness);
        }
    }

    feedingSystem.resetDaily(pet, timeManager.getDay());
    Serial.printf("[DayEnd] Day %d done.\n", pet.age_days);
    DisplayManager::showDayEndComplete(pet.age_days);

    saveManager.save(pet);
    saveManager.markSaved(now);
}

void doReset() {
    uint32_t now = timeManager.now();
    Form destroyedForm = pet.form;
    DisplayManager::showDestroyExecuted(destroyedForm);
    evolutionSystem.destroy(pet, now);
    feedingSystem.resetDaily(pet, timeManager.getDay());
    SaveResult r = saveManager.save(pet);
    if (r == SAVE_OK) saveManager.markSaved(now);
    // 不再立即调用 showDestroyReset(), 动画结束后 DisplayManager::update() 会自动切回 PAGE_IDLE
    printStatus();
}

void processCommand(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (strlen(cmd) == 0) return;

    if (strcmp(cmd, "s") == 0) { printStatus(); return; }
    if (strcmp(cmd, "h") == 0) { printHelp(); return; }
    if (strcmp(cmd, "fl") == 0) {
        Serial.println("--- Normal Food ---");
        for (uint8_t i = 0; i < FOOD_COUNT; i++)
            Serial.printf("  %d: %-16s %s (%+d hp)\n", i, FOOD_TABLE[i].name,
                          FOOD_TABLE[i].is_healthy ? "[H]" : "[J]", FOOD_TABLE[i].health_delta);
        Serial.println("--- Special Food ---");
        for (uint8_t i = 0; i < SFOOD_COUNT; i++)
            Serial.printf("  %d: %-16s %s\n", i, SPECIAL_FOOD_TABLE[i].name,
                          SPECIAL_FOOD_TABLE[i].description);
        return;
    }
    if (strcmp(cmd, "d") == 0) {
        doDayEnd();
        timeManager.advanceDays(1);
        DisplayManager::showToast("Day advanced", 1000);
        timeManager.checkNewDay();
        return;
    }
    if (strcmp(cmd, "save") == 0) {
        SaveResult r = saveManager.save(pet);
        if (r == SAVE_OK) {
            saveManager.markSaved(timeManager.now());
            Serial.println("[Save] OK");
            DisplayManager::showToast("Saved", 1000);
        } else {
            Serial.println("[Save] FAILED");
            DisplayManager::showToast("Save failed!", 2000);
        }
        return;
    }
    if (strcmp(cmd, "load") == 0) {
        if (saveManager.load(pet) == SAVE_OK) {
            Serial.println("[Load] OK");
            DisplayManager::showToast("Loaded", 1000);
            printStatus();
        } else {
            Serial.println("[Load] FAILED");
            DisplayManager::showToast("Load failed!", 2000);
        }
        return;
    }
    if (strcmp(cmd, "erase") == 0) {
        saveManager.erase();
        Serial.println("[Save] Erased");
        DisplayManager::showToast("Save erased", 1500);
        return;
    }
    if (strcmp(cmd, "reset") == 0) { doReset(); return; }
    if (strcmp(cmd, "grad") == 0) {
        if (pet.stage != STAGE_CHILD) { Serial.println("[Debug] Already adult."); return; }
        pet.age_days = CHILD_PERIOD_DAYS;
        EvolutionResult r = evolutionSystem.checkChildGraduation(pet);
        Serial.printf("[Debug] Grad: %s\n", EVO_EVENT_NAMES[r.event]);
        if (r.event == EVO_CHILD_TO_WHITE || r.event == EVO_CHILD_TO_BLACK) {
            DisplayManager::showChildGraduation(r, pet.alignment);
        }
        EvolutionResult r2 = evolutionSystem.check(pet, timeManager.now());
        if (r2.event != EVO_NONE) Serial.printf("[Debug] Form: %s\n", FORM_NAMES[r2.form_after]);
        printStatus();
        return;
    }
    if (strcmp(cmd, "mapo") == 0) {
        pet.mapo_tofu_count++;
        Serial.printf("[Debug] Mapo Tofu: %d / %d\n", pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
        DisplayManager::showMapoTofuTriggered(pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
        if (pet.mapo_tofu_count >= MAPO_TOFU_CURSE_THRESHOLD) {
            EvolutionResult eR = evolutionSystem.checkMapoCurse(pet);
            if (eR.event == EVO_BLACK_RHONGOMYNIAD) {
                DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                saveManager.save(pet);
                saveManager.markSaved(timeManager.now());
            }
        }
        return;
    }

    // t <min>
    if (cmd[0] == 't' && cmd[1] == ' ') {
        int minutes = atoi(cmd + 2);
        if (minutes > 0) {
            timeManager.advanceMinutes(minutes);
            DisplayManager::showToast("Time advanced", 1000);
            uint32_t now = timeManager.now();
            IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, (uint32_t)minutes, now);
            Serial.printf("[Idle] SR: %d->%d (+%dm)\n", iR.seriousness_before, iR.seriousness_after, minutes);
            if (iR.tier_changed) {
                Serial.printf("[Idle] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
                DisplayManager::showIdleTierChange(iR.tier_before, iR.tier_after);
            }
            EvolutionResult eR = evolutionSystem.check(pet, now);
            if (eR.event != EVO_NONE) {
                Serial.printf("[Idle] Evo: %s\n", EVO_EVENT_NAMES[eR.event]);
                DisplayManager::showEvolutionEvent(eR, pet.seriousness);
            }
        } else Serial.println("[Time] Invalid");
        return;
    }

    // stime Y M D H m
    if (strncmp(cmd, "stime ", 6) == 0) {
        int y, mo, da, h, mi;
        if (sscanf(cmd + 6, "%d %d %d %d %d", &y, &mo, &da, &h, &mi) == 5) {
            timeManager.setSimulatedTime(y, mo, da, h, mi);
            DisplayManager::showToast("Time set", 1000);
        }
        else Serial.println("[Time] Usage: stime YYYY MM DD HH mm");
        return;
    }

    // hp <val>
    if (strncmp(cmd, "hp ", 3) == 0) {
        int val = atoi(cmd + 3);
        if (val >= HEALTH_MIN && val <= HEALTH_MAX) {
            int16_t old = pet.health; pet.health = val;
            Serial.printf("[Debug] HP: %d->%d\n", old, pet.health);
        }
        return;
    }

    // sr <val>
    if (strncmp(cmd, "sr ", 3) == 0) {
        int val = atoi(cmd + 3);
        if (val >= SERIOUSNESS_MIN && val <= SERIOUSNESS_MAX) {
            int16_t old = pet.seriousness; pet.seriousness = val;
            Serial.printf("[Debug] SR: %d->%d | %s\n", old, pet.seriousness,
                          TIER_NAMES[seriousnessSystem.getCurrentTier(pet)]);
            EvolutionResult eR = evolutionSystem.check(pet, timeManager.now());
            if (eR.event != EVO_NONE)
                Serial.printf("[Debug] Evo: %s -> %s\n", FORM_NAMES[eR.form_before], FORM_NAMES[eR.form_after]);
        }
        return;
    }

    // age <val>
    if (strncmp(cmd, "age ", 4) == 0) {
        int val = atoi(cmd + 4);
        if (val >= 0) {
            uint16_t old = pet.age_days; pet.age_days = val;
            Serial.printf("[Debug] Age: %d->%d\n", old, pet.age_days);
        }
        return;
    }

    // ========================================================================
    //  Button simulation (serial debug, guarded by macro)
    // ========================================================================
#if ENABLE_SERIAL_INPUT_DEBUG
    if (strcmp(cmd, "ctx") == 0) {
        Serial.printf("[MC] Context: %s\n", UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        return;
    }

    if (strncmp(cmd, "btn ", 4) == 0) {
        ButtonId btn_id;
        const char* arg = cmd + 4;
        if (strcmp(arg, "l") == 0) btn_id = BTN_L;
        else if (strcmp(arg, "m") == 0) btn_id = BTN_M;
        else if (strcmp(arg, "r") == 0) btn_id = BTN_R;
        else { Serial.println("[Btn] Usage: btn l|m|r"); return; }
        Serial.printf("[Btn] Inject PRESS %s (ctx=%s)\n", BTN_NAMES[btn_id], UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        menuController.injectButton(btn_id, BTN_EVENT_PRESS);
        return;
    }

    if (strncmp(cmd, "btnl ", 5) == 0) {
        ButtonId btn_id;
        const char* arg = cmd + 5;
        if (strcmp(arg, "l") == 0) btn_id = BTN_L;
        else if (strcmp(arg, "m") == 0) btn_id = BTN_M;
        else if (strcmp(arg, "r") == 0) btn_id = BTN_R;
        else { Serial.println("[Btn] Usage: btnl l|m|r"); return; }
        Serial.printf("[Btn] Inject LONG_PRESS %s (ctx=%s)\n", BTN_NAMES[btn_id], UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        menuController.injectButton(btn_id, BTN_EVENT_LONG_PRESS);
        return;
    }

    if (strncmp(cmd, "btnr ", 5) == 0) {
        ButtonId btn_id;
        const char* arg = cmd + 5;
        if (strcmp(arg, "l") == 0) btn_id = BTN_L;
        else if (strcmp(arg, "m") == 0) btn_id = BTN_M;
        else if (strcmp(arg, "r") == 0) btn_id = BTN_R;
        else { Serial.println("[Btn] Usage: btnr l|m|r"); return; }
        Serial.printf("[Btn] Inject REPEAT %s (ctx=%s)\n", BTN_NAMES[btn_id], UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        menuController.injectButton(btn_id, BTN_EVENT_REPEAT);
        return;
    }
#endif

    Serial.printf("[Cmd] Unknown: '%s'\n", cmd);
}

void readSerialCommand() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (cmdLen > 0) { cmdBuf[cmdLen] = '\0'; processCommand(cmdBuf); cmdLen = 0; }
        } else if (cmdLen < sizeof(cmdBuf) - 1) {
            cmdBuf[cmdLen++] = c;
        }
    }
}

// ============================================================================
//  Setup & Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("================================");
    Serial.println("  Fate Tamagotchi");
    Serial.println("  Full Game Logic + Display");
    Serial.println("================================");

    // Initialize display first
    DisplayManager::init();

    timeManager.init();
    saveManager.init();
    if (saveManager.hasSave()) {
        if (saveManager.load(pet) == SAVE_OK) {
            Serial.println("[Main] Save loaded");
            DisplayManager::showSaveLoaded();
        } else {
            Serial.println("[Main] Save corrupted, new game");
            DisplayManager::showSaveCorruptedNewGame();
            pet.initNew(timeManager.now());
        }
    } else {
        Serial.println("[Main] New game");
        DisplayManager::showNewGame();
        pet.initNew(timeManager.now());
    }

    menuController.init(&pet, &gameCallbacks);
    Serial.println("[Main] MenuController initialized");

    printHelp();
    printStatus();

    DisplayManager::showSystemReady();
    Serial.println("\nReady.\n> ");
}

void loop() {
    // 1. Serial commands
    readSerialCommand();

    // 2. MenuController (processes button input -> game actions)
    menuController.update();

    // 3. Time-based game logic
    uint32_t now = timeManager.now();

    // Idle tick: seriousness growth + rhongo timer
    if (timeManager.checkNewMinute()) {
        if (!pet.is_rhongomyniad && !pet.is_black_rhongomyniad) {
            IdleTickResult iR = seriousnessSystem.onIdleTick(pet, now);
            if (iR.tier_changed) {
                Serial.printf("[Idle] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
                DisplayManager::showIdleTierChange(iR.tier_before, iR.tier_after);
                EvolutionResult eR = evolutionSystem.check(pet, now);
                if (eR.event != EVO_NONE) {
                    Serial.printf("[Idle] Evo: %s\n", EVO_EVENT_NAMES[eR.event]);
                    DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                }
            }
        }
    }

    // Day-end check
    if (timeManager.checkNewDay()) {
        Serial.println("[Auto] New day detected, running day-end...");
        DisplayManager::showNewDayDetected();
        doDayEnd();
    }

    // Auto-save
    if (saveManager.shouldAutoSave(now)) {
        SaveResult r = saveManager.save(pet);
        if (r == SAVE_OK) {
            saveManager.markSaved(now);
            DisplayManager::showAutoSave();
        }
    }

    // 4. Display update (state machine + render)
    DisplayManager::updatePetSnapshot(pet);
    DisplayManager::update(millis());

    delay(10);
}
