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

static PetState pet;
static char cmdBuf[64];
static uint8_t cmdLen = 0;

static FeedDraw currentDraw;
static bool drawPending = false;
static FeedOutcome lastFeedOutcome;
static bool comboPending = false;

// ============================================================================
//  调试用 UICallbacks (串口输出)
// ============================================================================

static UICallbacks debugCallbacks = {
    // onStatusOpen
    [](const PetState& p) {
        Serial.println("[MC] Status opened");
    },
    // onStatusClose
    []() {
        Serial.println("[MC] Status closed");
    },
    // onFeedDrawStart
    [](const FeedDraw& draw) {
        Serial.println("[MC] Feed draw started:");
        for (uint8_t i = 0; i < FEED_DRAW_COUNT; i++) {
            uint8_t id = draw.food_ids[i];
            Serial.printf("  [%d] %s %s (%+d)\n", i, FOOD_TABLE[id].name,
                          FOOD_TABLE[id].is_healthy ? "[H]" : "[J]", FOOD_TABLE[id].health_delta);
        }
        Serial.println("[MC] (auto-advancing to FEED_PICK)");
        // 模拟动画完成, 直接进入选择
        menuController.onAnimationComplete(UI_FEED_DRAW);
    },
    // onFeedCursorMove
    [](uint8_t cursor, const bool selected[4]) {
        Serial.printf("[MC] Feed cursor -> %d  [", cursor);
        for (uint8_t i = 0; i < 4; i++) Serial.printf("%c", selected[i] ? 'X' : '.');
        Serial.println("]");
    },
    // onFeedSlotToggle
    [](uint8_t slot, bool sel) {
        Serial.printf("[MC] Feed slot %d %s\n", slot, sel ? "SELECTED" : "DESELECTED");
    },
    // onFeedConfirm
    [](const FeedOutcome& outcome) {
        Serial.printf("[MC] Feed confirmed! HP: %d->%d\n", outcome.health_before, outcome.health_after);
        if (outcome.combo_triggered)
            Serial.printf("[MC] *** COMBO: %s ***\n", COMBO_NAMES[outcome.combo]);
    },
    // onFeedCancel
    []() {
        Serial.println("[MC] Feed cancelled");
    },
    // onSpecialFoodShow
    [](uint8_t count) {
        Serial.printf("[MC] Special food selection (%d items):\n", count);
        for (uint8_t i = 0; i < count; i++)
            Serial.printf("  %d: %s\n", i, SPECIAL_FOOD_TABLE[i].name);
    },
    // onSpecialFoodCursor
    [](uint8_t cursor) {
        Serial.printf("[MC] Special food cursor -> %d\n", cursor);
    },
    // onSpecialFoodSelect
    [](uint8_t id) {
        Serial.printf("[MC] Special food selected: %d (%s)\n", id, SPECIAL_FOOD_TABLE[id].name);
    },
    // onPokeStart
    []() {
        Serial.println("[MC] Poke animation started");
        // 模拟动画完成
        menuController.onAnimationComplete(UI_POKE_ANIM);
    },
    // onPokeResult
    [](bool valueChanged, int16_t srBefore, int16_t srAfter) {
        if (valueChanged)
            Serial.printf("[MC] Poke result: SR %d->%d\n", srBefore, srAfter);
        else
            Serial.println("[MC] Poke: cooldown (animation only)");
    },
    // onDestroyConfirmShow
    [](uint8_t cursor) {
        Serial.printf("[MC] Destroy confirm shown (cursor=%d, 0=yes 1=no)\n", cursor);
    },
    // onDestroyCursorMove
    [](uint8_t cursor) {
        Serial.printf("[MC] Destroy cursor -> %d (%s)\n", cursor, cursor == 0 ? "YES" : "NO");
    },
    // onDestroyExecuted
    []() {
        Serial.println("[MC] *** DESTROYED ***");
    },
    // onDestroyCancelled
    []() {
        Serial.println("[MC] Destroy cancelled");
    },
    // onEvolution
    [](const EvolutionResult& r) {
        Serial.printf("[MC] Evolution: %s -> %s\n", FORM_NAMES[r.form_before], FORM_NAMES[r.form_after]);
    },
    // onContextChange
    [](UIContext from, UIContext to) {
        Serial.printf("[MC] Context: %s -> %s\n", UI_CONTEXT_NAMES[from], UI_CONTEXT_NAMES[to]);
    }
};

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
    Serial.printf("  Feed stats: win[H=%d J=%d] out[H=%d J=%d]\n",
                  pet.daily_feed.healthy_in_window,
                  pet.daily_feed.junk_in_window,
                  pet.daily_feed.healthy_outside_window,
                  pet.daily_feed.junk_outside_window);
    Serial.println("========================================");

}

void printFoodList() {
    Serial.println("--- Normal Food ---");
    for (uint8_t i = 0; i < FOOD_COUNT; i++)
        Serial.printf("  %d: %-16s %s (%+d hp)\n", i, FOOD_TABLE[i].name,
                      FOOD_TABLE[i].is_healthy ? "[H]" : "[J]", FOOD_TABLE[i].health_delta);
    Serial.println("--- Special Food (combo reward) ---");
    for (uint8_t i = 0; i < SFOOD_COUNT; i++)
        Serial.printf("  %d: %-16s %s\n", i, SPECIAL_FOOD_TABLE[i].name,
                      SPECIAL_FOOD_TABLE[i].description);
    Serial.printf("--- Curse: %s ---\n", MAPO_TOFU.name);
}

void printHelp() {
    Serial.println("=== Fate Tamagotchi Console ===");
    Serial.println("  s              Status");
    Serial.println("  fl             Food list");
    Serial.println("  f              Start feed (draw 4)");
    Serial.println("  pick a b c     Pick 3 (slot 0-3)");
    Serial.println("  sf <id>        Special food (combo)");
    Serial.println("  p              Poke");
    Serial.println("  t <min>        Advance N minutes");
    Serial.println("  d              Advance 1 day");
    Serial.println("  save / load / erase");
    Serial.println("  reset          Destroy & reset");
    Serial.println("  stime Y M D H m");
    Serial.println("  hp/sr/age <val>  Debug set");
    Serial.println("  grad           Force graduation");
    Serial.println("  mapo           Debug +1 mapo count");
    Serial.println("--- Button Simulation ---");
    Serial.println("  btn l|m|r      Simulate short press");
    Serial.println("  btnl l|m|r     Simulate long press");
    Serial.println("  btnr l|m|r     Simulate repeat");
    Serial.println("  ctx            Show current UI context");
    Serial.println("  h              Help");
    Serial.println("===============================");
}

void doFeedDraw() {
    if (!evolutionSystem.canInteract(pet)) { Serial.println("[Feed] Cannot interact."); DisplayManager::showCannotInteract(); return; }
    uint32_t now = timeManager.now();
    FeedResult check = feedingSystem.canFeed(pet, now);
    if (check != FEED_OK) {
        Serial.printf("[Feed] %s\n", FEED_RESULT_NAMES[check]);
        if (check == FEED_ERR_TOO_SOON)
            Serial.printf("[Feed] Wait %lus\n", feedingSystem.secondsUntilNextFeed(pet, now));
        DisplayManager::showFeedCheckFailed(check, feedingSystem.secondsUntilNextFeed(pet, now));
        return;
    }
    currentDraw = feedingSystem.drawFood();
    drawPending = true;
    comboPending = false;
    Serial.println("[Feed] === DRAWN 4 FOODS ===");
    for (uint8_t i = 0; i < FEED_DRAW_COUNT; i++) {
        uint8_t id = currentDraw.food_ids[i];
        Serial.printf("  [%d] %s %s (%+d)\n", i, FOOD_TABLE[id].name,
                      FOOD_TABLE[id].is_healthy ? "[H]" : "[J]", FOOD_TABLE[id].health_delta);
    }
    Serial.println("[Feed] Use 'pick a b c'");
    DisplayManager::showFeedDraw(currentDraw);
    DisplayManager::switchPage(PAGE_FEED_DRAW);
    DisplayManager::playAnimation(ANIM_EATING);
}

void doFeedPick(uint8_t slotA, uint8_t slotB, uint8_t slotC) {
    if (!drawPending) { Serial.println("[Feed] No draw. Use 'f'."); return; }
    if (slotA > 3 || slotB > 3 || slotC > 3) { Serial.println("[Feed] Slot 0-3."); return; }
    if (slotA == slotB || slotA == slotC || slotB == slotC) { Serial.println("[Feed] No dupes."); return; }
    uint8_t picked[3] = { currentDraw.food_ids[slotA], currentDraw.food_ids[slotB], currentDraw.food_ids[slotC] };
    uint32_t now = timeManager.now();
    uint8_t hour = timeManager.getHour();
    FeedOutcome fR = feedingSystem.feed(pet, picked, now, hour);
    if (fR.result != FEED_OK) { Serial.printf("[Feed] FAILED: %s\n", FEED_RESULT_NAMES[fR.result]); drawPending = false; return; }
    InteractResult sR = seriousnessSystem.onInteract(pet, INTERACT_FEED, now);
    EvolutionResult eR = evolutionSystem.check(pet, now);
    Serial.println("[Feed] === RESULT ===");
    for (uint8_t i = 0; i < FEED_PICK_COUNT; i++)
        Serial.printf("  %s (%+d)\n", FOOD_TABLE[picked[i]].name, FOOD_TABLE[picked[i]].health_delta);
    Serial.printf("[Feed] HP: %d->%d (food %+d", fR.health_before, fR.health_after, fR.health_from_food);
    if (fR.health_from_combo != 0) Serial.printf(", combo %+d", fR.health_from_combo);
    Serial.println(")");
    Serial.printf("[Feed] SR: %d->%d (interact %+d", fR.seriousness_before, pet.seriousness, sR.seriousness_after - sR.seriousness_before);
    if (fR.seriousness_from_dislike != 0) Serial.printf(", dislike %+d", fR.seriousness_from_dislike);
    if (fR.seriousness_from_combo != 0) Serial.printf(", combo %+d", fR.seriousness_from_combo);
    Serial.println(")");
    Serial.printf("[Feed] Window: %s | %d/%d\n", fR.in_correct_window ? "YES" : "no", pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
    DisplayManager::showFeedResult(fR, pet.seriousness);
    DisplayManager::switchPage(PAGE_FEED_RESULT);
    if (fR.combo_triggered) {
        Serial.printf("[Feed] *** COMBO: %s ***\n", COMBO_NAMES[fR.combo]);
        Serial.println("[Feed] Use 'sf <id>' for special food.");
        for (uint8_t i = 0; i < SFOOD_COUNT; i++)
            Serial.printf("  %d: %s\n", i, SPECIAL_FOOD_TABLE[i].name);
        lastFeedOutcome = fR;
        comboPending = true;
        DisplayManager::showFeedComboTriggered(fR.combo);
        DisplayManager::playAnimation(ANIM_COMBO);
    }
    if (sR.tier_changed) Serial.printf("[Feed] Tier: %s -> %s\n", TIER_NAMES[sR.tier_before], TIER_NAMES[sR.tier_after]);
    if (eR.event == EVO_FORM_CHANGED) Serial.printf("[Feed] Form: %s -> %s\n", FORM_NAMES[eR.form_before], FORM_NAMES[eR.form_after]);
    if (eR.event == EVO_RHONGOMYNIAD) Serial.println("[Feed] *** RHONGOMYNIAD ***");
    if (eR.event == EVO_BLACK_RHONGOMYNIAD) Serial.println("[Feed] *** BLACK RHONGOMYNIAD ***");
    if (eR.event == EVO_FORM_CHANGED) DisplayManager::showFormChange(eR.form_before, eR.form_after, pet.seriousness, seriousnessSystem.getCurrentTier(pet));
    if (eR.event == EVO_RHONGOMYNIAD) DisplayManager::showRhongomyniadTriggered();
    if (eR.event == EVO_BLACK_RHONGOMYNIAD) DisplayManager::showBlackRhongomyniadTriggered();
    drawPending = false;
    saveManager.save(pet);
    saveManager.markSaved(now);
}

void doSpecialFood(uint8_t sfId) {
    if (!comboPending) { Serial.println("[Feed] No combo pending."); return; }
    if (sfId >= SFOOD_COUNT) { Serial.printf("[Feed] Invalid (0-%d).\n", SFOOD_COUNT - 1); return; }
    feedingSystem.applySpecialFood(pet, lastFeedOutcome, sfId);
    if (lastFeedOutcome.mapo_tofu_curse_activated) {
        EvolutionResult eR = evolutionSystem.checkMapoCurse(pet);
        if (eR.event == EVO_BLACK_RHONGOMYNIAD) {
            saveManager.save(pet);
            saveManager.markSaved(timeManager.now());
        }
    }
    comboPending = false;
}

void doPoke() {
    if (!evolutionSystem.canInteract(pet)) { Serial.println("[Poke] Cannot interact."); DisplayManager::showCannotInteract(); return; }
    uint32_t now = timeManager.now();
    InteractResult sR = seriousnessSystem.onInteract(pet, INTERACT_POKE, now);
    EvolutionResult eR = evolutionSystem.check(pet, now);
    bool valueChanged = (sR.seriousness_before != sR.seriousness_after);
    if (valueChanged)
        Serial.printf("[Poke] SR: %d->%d | %s\n", sR.seriousness_before, sR.seriousness_after, TIER_NAMES[sR.tier_after]);
    else
        Serial.printf("[Poke] Cooldown active (%ds remaining, animation only)\n",
                      POKE_COOLDOWN_SEC - (now - pet.last_poke_effect_time));
    Serial.printf("[Poke] Idle growth paused for %d min.\n", POKE_IDLE_PAUSE_SEC / 60);
    DisplayManager::showPokeAnimation();
    DisplayManager::playAnimation(ANIM_POKE);
    DisplayManager::showPokeResult(valueChanged, sR.seriousness_before, sR.seriousness_after);
    if (!valueChanged)
        DisplayManager::showPokeCooldown(POKE_COOLDOWN_SEC - (now - pet.last_poke_effect_time));
    DisplayManager::showPokeIdlePaused(POKE_IDLE_PAUSE_SEC / 60);
    if (sR.tier_changed) Serial.printf("[Poke] Tier: %s -> %s\n", TIER_NAMES[sR.tier_before], TIER_NAMES[sR.tier_after]);
    if (eR.event == EVO_FORM_CHANGED) Serial.printf("[Poke] Form: %s -> %s\n", FORM_NAMES[eR.form_before], FORM_NAMES[eR.form_after]);
    if (sR.tier_changed) DisplayManager::showIdleTierChange(sR.tier_before, sR.tier_after);
    if (eR.event == EVO_FORM_CHANGED) DisplayManager::showFormChange(eR.form_before, eR.form_after, pet.seriousness, seriousnessSystem.getCurrentTier(pet));
}


void doDayEnd() {
    Serial.println("[DayEnd] --- Processing ---");
    DisplayManager::showDayEndStart();
    DisplayManager::switchPage(PAGE_DAY_END);
    DisplayManager::playAnimation(ANIM_DAY_END);
    uint32_t now = timeManager.now();

    // 推进一天的待机严肃值 (1440分钟)
    IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, 1440, now);
    if (iR.tier_changed)
        Serial.printf("[DayEnd] Idle SR: %d->%d | Tier: %s -> %s\n",
                      iR.seriousness_before, iR.seriousness_after,
                      TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
    if (iR.tier_changed)
        DisplayManager::showDayEndIdleSR(iR.seriousness_before, iR.seriousness_after, iR.tier_before, iR.tier_after);

    // 检查是否在待机过程中触发了狮子王
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
    if (fO.window_bonus_applied)
        Serial.printf("[DayEnd] Window bonus +%d HP\n", CORRECT_WINDOW_BONUS);
    if (fO.window_bonus_applied)
        DisplayManager::showDayEndWindowBonus(CORRECT_WINDOW_BONUS);
    if (fO.window_penalty_applied)
        Serial.printf("[DayEnd] Window penalty -%d HP\n", WRONG_WINDOW_PENALTY);
    if (fO.window_penalty_applied)
        DisplayManager::showDayEndWindowPenalty(WRONG_WINDOW_PENALTY);
    if (fO.missed_feed_penalty) {
        seriousnessSystem.applyMissedFeedPenalty(pet);
        Serial.printf("[DayEnd] Missed feeds (%d/%d)\n",
                      pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
        DisplayManager::showDayEndMissedFeed(pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
    }

    pet.age_days++;

    EvolutionResult evo = evolutionSystem.checkChildGraduation(pet);
    if (evo.event == EVO_NONE && pet.stage == STAGE_ADULT)
        evo = evolutionSystem.check(pet, now);
    if (evo.event != EVO_NONE)
        Serial.printf("[DayEnd] Evo: %s\n", EVO_EVENT_NAMES[evo.event]);
    if (evo.event != EVO_NONE)
        DisplayManager::showEvolutionEvent(evo);

    feedingSystem.resetDaily(pet, timeManager.getDay());
    Serial.printf("[DayEnd] Day %d done.\n", pet.age_days);
    DisplayManager::showDayEndComplete(pet.age_days);

    drawPending = false;
    comboPending = false;
    saveManager.save(pet);
    saveManager.markSaved(now);
}


void doReset() {
    uint32_t now = timeManager.now();
    DisplayManager::showDestroyExecuted(pet.form);
    evolutionSystem.destroy(pet, now);
    feedingSystem.resetDaily(pet, timeManager.getDay());
    drawPending = false;
    comboPending = false;
    saveManager.save(pet);
    saveManager.markSaved(now);
    DisplayManager::showDestroyReset();
    DisplayManager::switchPage(PAGE_IDLE);
    printStatus();
}

void doForceGrad() {
    if (pet.stage != STAGE_CHILD) { Serial.println("[Debug] Already adult."); return; }
    pet.age_days = CHILD_PERIOD_DAYS;
    EvolutionResult r = evolutionSystem.checkChildGraduation(pet);
    Serial.printf("[Debug] Grad: %s\n", EVO_EVENT_NAMES[r.event]);
    EvolutionResult r2 = evolutionSystem.check(pet, timeManager.now());
    if (r2.event != EVO_NONE) Serial.printf("[Debug] Form: %s\n", FORM_NAMES[r2.form_after]);
    printStatus();
}

void processCommand(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (strlen(cmd) == 0) return;

    if (strcmp(cmd, "s") == 0) { printStatus(); return; }
    if (strcmp(cmd, "h") == 0) { printHelp(); return; }
    if (strcmp(cmd, "fl") == 0) { printFoodList(); return; }
    if (strcmp(cmd, "f") == 0) { doFeedDraw(); return; }
    if (strcmp(cmd, "p") == 0) { doPoke(); return; }
    if (strcmp(cmd, "d") == 0) { doDayEnd(); timeManager.advanceDays(1); timeManager.checkNewDay(); return; }

    if (strcmp(cmd, "save") == 0) {
        saveManager.save(pet);
        saveManager.markSaved(timeManager.now());
        Serial.println("[Save] OK");
        DisplayManager::showSaveSuccess();
        return;
    }
    if (strcmp(cmd, "load") == 0) {
        if (saveManager.load(pet) == SAVE_OK) { Serial.println("[Load] OK"); DisplayManager::showLoadSuccess(); printStatus(); }
        else { Serial.println("[Load] FAILED"); DisplayManager::showLoadFailed(); }
        return;
    }
    if (strcmp(cmd, "erase") == 0) { saveManager.erase(); Serial.println("[Save] Erased"); DisplayManager::showSaveErased(); return; }
    if (strcmp(cmd, "reset") == 0) { doReset(); return; }
    if (strcmp(cmd, "grad") == 0) { doForceGrad(); return; }

    // mapo - 调试: 麻婆豆腐计数+1
    if (strcmp(cmd, "mapo") == 0) {
        pet.mapo_tofu_count++;
        Serial.printf("[Debug] Mapo Tofu: %d / %d\n", pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
        if (pet.mapo_tofu_count >= MAPO_TOFU_CURSE_THRESHOLD) {
            EvolutionResult eR = evolutionSystem.checkMapoCurse(pet);
            if (eR.event == EVO_BLACK_RHONGOMYNIAD) {
                saveManager.save(pet);
                saveManager.markSaved(timeManager.now());
            }
        }
        return;
    }

    // pick a b c
    if (strncmp(cmd, "pick ", 5) == 0) {
        int a, b, c;
        if (sscanf(cmd + 5, "%d %d %d", &a, &b, &c) == 3)
            doFeedPick((uint8_t)a, (uint8_t)b, (uint8_t)c);
        else Serial.println("[Feed] Usage: pick 0 1 2");
        return;
    }

    // sf <id>
    if (strncmp(cmd, "sf ", 3) == 0) { doSpecialFood((uint8_t)atoi(cmd + 3)); return; }

    // t <min>
    if (cmd[0] == 't' && cmd[1] == ' ') {
        int minutes = atoi(cmd + 2);
        if (minutes > 0) {
            timeManager.advanceMinutes(minutes);
            uint32_t now = timeManager.now();
            IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, (uint32_t)minutes, now);
            Serial.printf("[Idle] SR: %d->%d (+%dm)\n", iR.seriousness_before, iR.seriousness_after, minutes);
            if (iR.tier_changed)
                Serial.printf("[Idle] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
            if (iR.tier_changed)
                DisplayManager::showIdleTierChange(iR.tier_before, iR.tier_after);
            EvolutionResult eR = evolutionSystem.check(pet, now);
            if (eR.event == EVO_FORM_CHANGED)
                Serial.printf("[Idle] Form: %s -> %s\n", FORM_NAMES[eR.form_before], FORM_NAMES[eR.form_after]);
            if (eR.event == EVO_FORM_CHANGED)
                DisplayManager::showIdleFormChange(eR.form_before, eR.form_after);
            if (eR.event == EVO_RHONGOMYNIAD) Serial.println("[Idle] *** RHONGOMYNIAD ***");
            if (eR.event == EVO_RHONGOMYNIAD) DisplayManager::showRhongomyniadTriggered();
            if (iR.rhongo_state == RHONGO_COUNTING)
                Serial.printf("[Idle] Rhongo: %luh left\n", iR.rhongo_remaining_sec / 3600);
            if (iR.rhongo_state == RHONGO_COUNTING)
                DisplayManager::showIdleRhongoCountdown(iR.rhongo_remaining_sec / 3600);
        } else Serial.println("[Time] Invalid");
        return;
    }

    // stime Y M D H m
    if (strncmp(cmd, "stime ", 6) == 0) {
        int y, mo, da, h, mi;
        if (sscanf(cmd + 6, "%d %d %d %d %d", &y, &mo, &da, &h, &mi) == 5)
            timeManager.setSimulatedTime(y, mo, da, h, mi);
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
    //  模拟按键注入 (调试用, 绕过GPIO)
    //  btn l / btn m / btn r       - 模拟短按
    //  btnl l / btnl m / btnl r    - 模拟长按
    //  btnr l / btnr m / btnr r    - 模拟连按(repeat)
    //  ctx                         - 显示当前UI上下文
    // ========================================================================
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

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("================================");
    Serial.println("  Fate Tamagotchi");
    Serial.println("  Full Game Logic");
    Serial.println("================================");
    DisplayManager::showBootScreen();
    timeManager.init();
    saveManager.init();
    if (saveManager.hasSave()) {
        if (saveManager.load(pet) == SAVE_OK) { Serial.println("[Main] Save loaded"); DisplayManager::showSaveLoaded(); }
        else { Serial.println("[Main] Save corrupted, new game"); DisplayManager::showSaveCorruptedNewGame(); pet.initNew(timeManager.now()); }
    } else {
        Serial.println("[Main] New game");
        DisplayManager::showNewGame();
        pet.initNew(timeManager.now());
    }
    printHelp();
    printStatus();
    menuController.init(&pet, &debugCallbacks);
    Serial.println("[Main] MenuController initialized (btn/btnl/btnr commands ready)");
    Serial.println("\nReady.\n> ");
    DisplayManager::showSystemReady();
    DisplayManager::switchPage(PAGE_IDLE);
}

void loop() {
    readSerialCommand();
    uint32_t now = timeManager.now();

    // 每分钟 idle tick: 严肃值增长 + 狮子王计时
    if (timeManager.checkNewMinute()) {
        if (!pet.is_rhongomyniad && !pet.is_black_rhongomyniad) {
            IdleTickResult iR = seriousnessSystem.onIdleTick(pet, now);
            if (iR.tier_changed) {
                Serial.printf("[Idle] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
                DisplayManager::showIdleTierChange(iR.tier_before, iR.tier_after);
                EvolutionResult eR = evolutionSystem.check(pet, now);
                if (eR.event == EVO_FORM_CHANGED)
                    Serial.printf("[Idle] Form: %s -> %s\n", FORM_NAMES[eR.form_before], FORM_NAMES[eR.form_after]);
                if (eR.event == EVO_FORM_CHANGED)
                    DisplayManager::showIdleFormChange(eR.form_before, eR.form_after);
                if (eR.event == EVO_RHONGOMYNIAD)
                    Serial.println("[Idle] *** RHONGOMYNIAD ***");
                if (eR.event == EVO_RHONGOMYNIAD)
                    DisplayManager::showRhongomyniadTriggered();
            }
        }
    }

    // 每日结算检测
    if (timeManager.checkNewDay()) {
        Serial.println("[Auto] New day detected, running day-end...");
        DisplayManager::showNewDayDetected();
        doDayEnd();
    }

    // 自动存档
    if (saveManager.shouldAutoSave(now)) {
        SaveResult r = saveManager.save(pet);
        if (r == SAVE_OK) saveManager.markSaved(now);
        if (r == SAVE_OK) DisplayManager::showAutoSave();
    }
    delay(10);
}
