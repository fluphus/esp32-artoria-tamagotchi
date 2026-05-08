#include <Arduino.h>
#include "config/game_config.h"
#include "config/food_table.h"
#include "core/game_state.h"
#include "core/time_manager.h"
#include "core/save_manager.h"
#include "pet/feeding.h"
#include "pet/seriousness.h"
#include "pet/evolution.h"
#include "pet/gallery.h"
#include "input/menu_controller.h"
#include "display/DisplayManager.h"
#include "core/power_manager.h"
#include <esp_sleep.h>
#include <esp_random.h>

// ============================================================================
//  Serial input debug switch (set to 0 to disable serial button commands)
// ============================================================================
#ifndef ENABLE_SERIAL_INPUT_DEBUG
#define ENABLE_SERIAL_INPUT_DEBUG 1
#endif

static PetState pet;
static char cmdBuf[64];
static uint8_t cmdLen = 0;

// 离线补偿: 等待串口设置时间
static bool waitingForTimeSet = false;
static uint32_t loadedSaveTime = 0;  // 存档中记录的时间戳

static void persistGalleryUnlockFromEvolution(const EvolutionResult& r) {
    if (r.event == EVO_NONE) return;
    if (gallerySystem.unlockForm(r.form_after)) {
        SaveResult g = saveManager.saveGallery(gallerySystem.getData());
        if (g != SAVE_OK) {
            Serial.println("[Gallery] ERROR: Failed to persist unlock");
        }
        if (gallerySystem.getData().getUnlockedCount() >= FORM_COUNT) {
            DisplayManager::showGalleryCompleteUnlocked();
        }
    }
}

static void persistCurrentFormUnlock() {
    if (gallerySystem.unlockForm(pet.form)) {
        SaveResult g = saveManager.saveGallery(gallerySystem.getData());
        if (g != SAVE_OK) {
            Serial.println("[Gallery] ERROR: Failed to persist current form unlock");
        }
        if (gallerySystem.getData().getUnlockedCount() >= FORM_COUNT) {
            DisplayManager::showGalleryCompleteUnlocked();
        }
    }
}

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
        // 图鉴: 解锁新形态并持久化
        persistGalleryUnlockFromEvolution(r);
    },
    // onContextChange
    [](UIContext from, UIContext to) {
        Serial.printf("[MC] Context: %s -> %s\n", UI_CONTEXT_NAMES[from], UI_CONTEXT_NAMES[to]);
        // 同步 DisplayPage ??UIContext
        switch (to) {
            case UI_IDLE:
                // 动画播放中不切页?? 等动画结束后再切
                // Page hold 期间也不切页?? ??hold 结束后自动切
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
            case UI_GALLERY:
                DisplayManager::switchPage(PAGE_GALLERY);
                break;
            default:
                // UI_FEED_DRAW, UI_POKE_ANIM, UI_EVOLUTION 由具??show*() 负责切页
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
    if (pet.is_nobu) {
        Serial.printf("  Time:       %s\n", timeBuf);
        Serial.println("  Name:       nobu");
        Serial.println("  HP:         ?");
        Serial.println("  SR:         ?");
        Serial.println("  Age:        ?");
        Serial.println("  Stage:      ?");
        Serial.println("  Align:      ?");
        Serial.println("========================================");
        return;
    }
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
    Serial.printf("  Rounds:     %d\n", pet.rounds);
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
    Serial.println("  SET_TIME <epoch>  Set system time (unix timestamp)");
    Serial.println("  hp/sr/age <val>  Debug set");
    Serial.println("  grad           Force graduation");
    Serial.println("  mapo           Debug +1 mapo count");
    Serial.println("  FORCE_NOBU     Force nobu route");
    Serial.println("--- Gallery ---");
    Serial.println("  UNLOCK_ALL     Unlock all gallery forms");
    Serial.println("  RESET_GALLERY  Reset gallery (lock all)");
    Serial.println("--- Power ---");
    Serial.println("  bright <0-15>  Set screen brightness");
    Serial.println("  dim <0-15>     Set dim brightness");
    Serial.println("  dim_t <sec>    Set dim timeout");
    Serial.println("  off_t <sec>    Set off timeout");
    Serial.println("  pwrsave        Save power config to NVS");
    Serial.println("  pwrinfo        Print power config");
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

    bool wasRhongo = pet.is_rhongomyniad;
    IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, 1440, now);
    bool rhongoTriggeredInDayEnd = (!wasRhongo && pet.is_rhongomyniad);
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
        if (rhongoTriggeredInDayEnd) {
            persistCurrentFormUnlock();
        }
        feedingSystem.resetDaily(pet, timeManager.getDay());
        pet.age_days++;
        saveManager.save(pet, timeManager.now());
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
        persistGalleryUnlockFromEvolution(evo);
    } else {
        // 只有非毕业情况才检查成年进??
        if (evo.event == EVO_NONE && pet.stage == STAGE_ADULT)
            evo = evolutionSystem.check(pet, now);
        if (evo.event != EVO_NONE) {
            Serial.printf("[DayEnd] Evo: %s\n", EVO_EVENT_NAMES[evo.event]);
            DisplayManager::showEvolutionEvent(evo, pet.seriousness);
            persistGalleryUnlockFromEvolution(evo);
        }
    }

    feedingSystem.resetDaily(pet, timeManager.getDay());
    Serial.printf("[DayEnd] Day %d done.\n", pet.age_days);
    DisplayManager::showDayEndComplete(pet.age_days);

    saveManager.save(pet, timeManager.now());
    saveManager.markSaved(now);
}

void doReset() {
    uint32_t now = timeManager.now();
    Form destroyedForm = pet.form;
    uint16_t prevAgeDays = pet.age_days;
    uint16_t prevRounds = pet.rounds;
    DisplayManager::showDestroyExecuted(destroyedForm);

    // 指定轮次保底: round 329 触发 reset 后 (即 rounds=330) 必定进入 Nobu 路线
    if (((prevRounds > 0) ? prevRounds : 1) == 329) {
        Serial.println("[Reset] Forced Nobu: round 329 -> round 330");
        evolutionSystem.destroyToNobu(pet, now, prevAgeDays);
        pet.rounds = 330;
        if (gallerySystem.unlockForm(pet.form)) {
            saveManager.saveGallery(gallerySystem.getData());
        }
        feedingSystem.resetDaily(pet, timeManager.getDay());
        SaveResult r = saveManager.save(pet, timeManager.now());
        if (r == SAVE_OK) saveManager.markSaved(now);
        printStatus();
        return;
    }

    // nobu 彩蛋判定 (与 executeDestroy 逻辑一致)
    uint32_t roll = esp_random() % 1000;
    uint32_t threshold = NOBU_BASE_PERMILLE;
    if (prevAgeDays == 5) {   // 仅第6天概率翻倍
        threshold = NOBU_DAY6_PERMILLE;
    }

    Serial.println("[Reset] 开始计算 Nobu 触发概率...");
    Serial.printf("[Reset] 当前随机数: %lu, 目标阈值: %lu (需 roll < threshold 才触发)\n", roll, threshold);

    if (roll < threshold) {
        Serial.println("[Reset] 触发判定结果: 成功! 进入 Nobu 路线");
        evolutionSystem.destroyToNobu(pet, now, prevAgeDays);
    } else {
        Serial.println("[Reset] 触发判定结果: 失败, 正常重置为 Lily");
        evolutionSystem.destroy(pet, now);
    }
    pet.rounds = ((prevRounds > 0) ? prevRounds : 1) + 1;
    if (gallerySystem.unlockForm(pet.form)) {
        saveManager.saveGallery(gallerySystem.getData());
    }

    feedingSystem.resetDaily(pet, timeManager.getDay());
    SaveResult r = saveManager.save(pet, timeManager.now());
    if (r == SAVE_OK) saveManager.markSaved(now);
    printStatus();
}

// ============================================================================
//  离线时间补偿 - 模拟离线期间经过的天数
// ============================================================================

void skipTime(uint32_t offlineSeconds) {
    if (offlineSeconds < 60) {
        Serial.println("[Offline] Less than 1 minute offline, no compensation needed.");
        return;
    }

    uint32_t offlineDays = offlineSeconds / 86400;
    uint32_t remainingMinutes = (offlineSeconds % 86400) / 60;

    Serial.printf("[Offline] Compensating: %lu days + %lu minutes\n", offlineDays, remainingMinutes);

    // 逐天结算 (与 d 命令等效)
    for (uint32_t i = 0; i < offlineDays; i++) {
        doDayEnd();
        timeManager.advanceDays(1);
        timeManager.checkNewDay();  // 消耗 newDay 标记
    }

    // 结算剩余分钟的严肃值增长
    if (remainingMinutes > 0) {
        bool wasRhongo = pet.is_rhongomyniad;
        timeManager.advanceMinutes(remainingMinutes);
        uint32_t now = timeManager.now();
        IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, remainingMinutes, now);
        if (!wasRhongo && pet.is_rhongomyniad) {
            persistCurrentFormUnlock();
        }
        if (iR.tier_changed) {
            Serial.printf("[Offline] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
            EvolutionResult eR = evolutionSystem.check(pet, now);
            if (eR.event != EVO_NONE) {
                Serial.printf("[Offline] Evo: %s -> %s\n", FORM_NAMES[eR.form_before], FORM_NAMES[eR.form_after]);
            }
        }
    }

    Serial.printf("[Offline] Compensation complete. Age: Day %d\n", pet.age_days);
    saveManager.save(pet, timeManager.now());
    saveManager.markSaved(timeManager.now());
    printStatus();
}

void processCommand(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (strlen(cmd) == 0) return;

    // SET_TIME 命令: 设置系统时间 (离线补偿用, 任何状态下可用)
    if (strncmp(cmd, "SET_TIME ", 9) == 0) {
        uint32_t timestamp = strtoul(cmd + 9, nullptr, 10);
        if (timestamp < 1000000000UL) {
            Serial.println("[Time] Invalid timestamp (too small). Use unix epoch seconds.");
            return;
        }
        TimeInfo t = timeManager.epochToTimeInfo(timestamp);
        timeManager.setSimulatedTime(t.year, t.month, t.day, t.hour, t.minute);
        // 补偿秒数精度
        Serial.printf("[Time] System time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.year, t.month, t.day, t.hour, t.minute, t.second);

        if (waitingForTimeSet) {
            waitingForTimeSet = false;
            // 计算离线时长并补偿
            if (loadedSaveTime > 0 && timestamp > loadedSaveTime) {
                uint32_t offlineDuration = timestamp - loadedSaveTime;
                Serial.printf("[Offline] Duration: %lu seconds (%.1f days)\n",
                              offlineDuration, (float)offlineDuration / 86400.0f);
                skipTime(offlineDuration);
            } else {
                Serial.println("[Offline] No compensation needed (no save time or time went backwards).");
            }
            // 进入正常运行
            DisplayManager::showSystemReady();
            Serial.println("[Main] Time set, entering normal operation.");
            printStatus();
        }
        return;
    }

    // 等待时间设置期间, 只允许 SET_TIME 和 s/h 命令
    if (waitingForTimeSet) {
        if (strcmp(cmd, "s") == 0) { printStatus(); return; }
        if (strcmp(cmd, "h") == 0) { printHelp(); return; }
        Serial.println("[Main] Waiting for SET_TIME <unix_timestamp>. Other commands blocked.");
        return;
    }

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
        SaveResult r = saveManager.save(pet, timeManager.now());
        if (r == SAVE_OK) {
            saveManager.markSaved(timeManager.now());
            saveManager.saveGallery(gallerySystem.getData());
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
    if (strcmp(cmd, "FORCE_NOBU") == 0) {
        uint32_t now = timeManager.now();
        uint16_t prevAgeDays = pet.age_days;
        uint16_t prevRounds = pet.rounds;
        evolutionSystem.destroyToNobu(pet, now, prevAgeDays);
        pet.rounds = ((prevRounds > 0) ? prevRounds : 1) + 1;
        feedingSystem.resetDaily(pet, timeManager.getDay());
        saveManager.save(pet, timeManager.now());
        saveManager.markSaved(now);
        Serial.println("[Debug] 已强制切换为 Nobu");
        printStatus();
        return;
    }
    if (strcmp(cmd, "UNLOCK_ALL") == 0) {
        gallerySystem.unlockAll();
        saveManager.saveGallery(gallerySystem.getData());
        if (gallerySystem.getData().getUnlockedCount() >= FORM_COUNT) {
            DisplayManager::showGalleryCompleteUnlocked();
        }
        Serial.println("[Debug] All gallery forms unlocked and saved");
        return;
    }
    if (strcmp(cmd, "RESET_GALLERY") == 0) {
        gallerySystem.resetGallery();
        saveManager.saveGallery(gallerySystem.getData());
        Serial.println("[Debug] Gallery reset (all forms locked) and saved");
        return;
    }
    if (strcmp(cmd, "grad") == 0) {
        if (pet.stage != STAGE_CHILD) { Serial.println("[Debug] Already adult."); return; }
        pet.age_days = CHILD_PERIOD_DAYS;
        EvolutionResult r = evolutionSystem.checkChildGraduation(pet);
        Serial.printf("[Debug] Grad: %s\n", EVO_EVENT_NAMES[r.event]);
        if (r.event == EVO_CHILD_TO_WHITE || r.event == EVO_CHILD_TO_BLACK) {
            DisplayManager::showChildGraduation(r, pet.alignment);
            persistGalleryUnlockFromEvolution(r);
        }
        EvolutionResult r2 = evolutionSystem.check(pet, timeManager.now());
        if (r2.event != EVO_NONE) {
            Serial.printf("[Debug] Form: %s\n", FORM_NAMES[r2.form_after]);
            persistGalleryUnlockFromEvolution(r2);
        }
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
                persistGalleryUnlockFromEvolution(eR);
                saveManager.save(pet, timeManager.now());
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
                persistGalleryUnlockFromEvolution(eR);
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
    //  Power management commands
    // ========================================================================
    if (strncmp(cmd, "bright ", 7) == 0) {
        int val = atoi(cmd + 7);
        if (val >= 0 && val <= 15) {
            powerManager.setBrightness((uint8_t)val);
            powerManager.onUserActivity();
        } else {
            Serial.println("[Power] Usage: bright <0-15>");
        }
        return;
    }
    if (strncmp(cmd, "dim ", 4) == 0 && strncmp(cmd, "dim_t ", 6) != 0) {
        int val = atoi(cmd + 4);
        if (val >= 0 && val <= 15) {
            powerManager.setDimBrightness((uint8_t)val);
        } else {
            Serial.println("[Power] Usage: dim <0-15>");
        }
        return;
    }
    if (strncmp(cmd, "dim_t ", 6) == 0) {
        uint32_t val = strtoul(cmd + 6, nullptr, 10);
        powerManager.setDimTimeout(val);
        return;
    }
    if (strncmp(cmd, "off_t ", 6) == 0) {
        uint32_t val = strtoul(cmd + 6, nullptr, 10);
        powerManager.setOffTimeout(val);
        return;
    }
    if (strcmp(cmd, "pwrsave") == 0) {
        powerManager.saveConfig();
        return;
    }
    if (strcmp(cmd, "pwrinfo") == 0) {
        Serial.println("--- Power Config ---");
        Serial.printf("  Brightness:    %d / 15\n", powerManager.getBrightness());
        Serial.printf("  Dim brightness:%d / 15\n", powerManager.getDimBrightness());
        Serial.printf("  Dim timeout:   %lu sec\n", powerManager.getDimTimeout());
        Serial.printf("  Off timeout:   %lu sec\n", powerManager.getOffTimeout());
        const char* stateNames[] = {"ACTIVE", "DIM", "OFF"};
        Serial.printf("  State:         %s\n", stateNames[powerManager.getState()]);
        Serial.println("--------------------");
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

    // Initialize power manager
    powerManager.init();

    timeManager.init();
    saveManager.init();

    // 初始化图鉴系统
    gallerySystem.init();

    if (saveManager.hasSave()) {
        if (saveManager.load(pet) == SAVE_OK) {
            Serial.println("[Main] Save loaded");
            DisplayManager::showSaveLoaded();
            loadedSaveTime = saveManager.getLastSaveTime();
            if (loadedSaveTime > 0) {
                // 有存档时间记录, 进入等待时间设置状态
                waitingForTimeSet = true;
                Serial.printf("[Main] Last save time: %lu\n", loadedSaveTime);
                Serial.println("[Main] *** Please set current time via: SET_TIME <unix_timestamp> ***");
            }
            // 加载图鉴数据 (旧存档兼容: 无数据则初始化为空)
            saveManager.loadGallery(gallerySystem.getData());
            // 确保当前形态已解锁
            gallerySystem.unlockForm(pet.form);
        } else {
            Serial.println("[Main] Save corrupted, new game");
            DisplayManager::showSaveCorruptedNewGame();
            pet.initNew(timeManager.now());
        }
    } else {
        Serial.println("[Main] New game");
        DisplayManager::showNewGame();
        pet.initNew(timeManager.now());
        // 新游戏: 解锁初始形态
        gallerySystem.unlockForm(pet.form);
    }

    menuController.init(&pet, &gameCallbacks);
    Serial.println("[Main] MenuController initialized");

    printHelp();
    printStatus();

    if (waitingForTimeSet) {
        DisplayManager::showWaitTimeSet();
        Serial.println("\n[Main] Waiting for SET_TIME command...\n> ");
    } else {
        DisplayManager::showSystemReady();
        Serial.println("\nReady.\n> ");
    }
}

// ============================================================================
//  Deep Sleep 进入 (保持内部计时)
// ============================================================================

void enterDeepSleep() {
    // 保存当前状态
    SaveResult r = saveManager.save(pet, timeManager.now());
    if (r == SAVE_OK) saveManager.markSaved(timeManager.now());
    Serial.println("[Power] Entering deep sleep...");
    Serial.flush();

    // 配置唤醒源: 任意按键 (GPIO) 唤醒
    uint64_t wakeupMask = 0;
    wakeupMask |= (1ULL << PIN_BTN_L);
    wakeupMask |= (1ULL << PIN_BTN_M);
    wakeupMask |= (1ULL << PIN_BTN_R);
    esp_sleep_enable_ext1_wakeup(wakeupMask, ESP_EXT1_WAKEUP_ANY_LOW);

    // 配置定时器唤醒: 每60秒唤醒一次以维持游戏计时
    esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);

    // 进入 deep sleep
    esp_deep_sleep_start();
}

void loop() {
    // 1. Serial commands (always active, even during time-set wait)
    readSerialCommand();

    // 等待时间设置期间, 只处理串口命令和显示更新
    if (waitingForTimeSet) {
        powerManager.update(millis());
        DisplayManager::update(millis());
        delay(10);
        return;
    }

    // 2. Power management update
    powerManager.update(millis());

    // 检查是否需要进入 deep sleep
    if (powerManager.shouldEnterDeepSleep()) {
        powerManager.clearSleepFlag();
        enterDeepSleep();
        return;
    }

    // 3. MenuController (processes button input -> game actions)
    menuController.update();

    // 3. Time-based game logic
    uint32_t now = timeManager.now();

    // Idle tick: seriousness growth + rhongo timer
    if (timeManager.checkNewMinute()) {
        if (!pet.is_rhongomyniad && !pet.is_black_rhongomyniad) {
            bool wasRhongo = pet.is_rhongomyniad;
            IdleTickResult iR = seriousnessSystem.onIdleTick(pet, now);
            if (!wasRhongo && pet.is_rhongomyniad) {
                persistCurrentFormUnlock();
            }
            if (iR.tier_changed) {
                Serial.printf("[Idle] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
                DisplayManager::showIdleTierChange(iR.tier_before, iR.tier_after);
                EvolutionResult eR = evolutionSystem.check(pet, now);
                if (eR.event != EVO_NONE) {
                    Serial.printf("[Idle] Evo: %s\n", EVO_EVENT_NAMES[eR.event]);
                    DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                    persistGalleryUnlockFromEvolution(eR);
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
        SaveResult r = saveManager.save(pet, timeManager.now());
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
