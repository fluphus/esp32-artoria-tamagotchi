#include <Arduino.h>
#include "config/game_config.h"
#include "config/food_table.h"
#include "core/game_state.h"
#include "core/time_manager.h"
#include "core/save_manager.h"
#include "core/save_workflows.h"
#include "pet/feeding.h"
#include "pet/seriousness.h"
#include "pet/evolution.h"
#include "pet/gallery.h"
#include "input/menu_controller.h"
#include "display/DisplayManager.h"
#include "core/power_manager.h"
#include <esp_sleep.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp32/rtc.h>
#include <driver/rtc_io.h>
#include <string.h>

// ============================================================================
//  Serial input debug switch (set to 0 to disable serial button commands)
// ============================================================================
#ifndef ENABLE_SERIAL_INPUT_DEBUG
#define ENABLE_SERIAL_INPUT_DEBUG 1
#endif

static PetState pet;
static DeviceState deviceState;
static char cmdBuf[64];
static uint8_t cmdLen = 0;
static bool cmdDiscard=false;
static uint32_t cmdLastActivity=0;

// 离线补偿: 等待设置时间 (仅首次开机/断电恢复时)
static bool waitingForTimeSet = false;
// 仅用于新游戏/坏档重建: 在初始时间确认后按最终时间重建 pet 时间基线
static bool reinitPetAfterInitialTimeConfirm = false;

static constexpr size_t SLOT_RAW_BASE_BYTES = sizeof(SaveHeader) + sizeof(PetState);
static constexpr size_t SLOT_RAW_WITH_GALLERY_BYTES = SLOT_RAW_BASE_BYTES + sizeof(GalleryData);
static constexpr size_t SLOT_RAW_MAX_BYTES = SLOT_RAW_WITH_GALLERY_BYTES;
static constexpr size_t EXPORT_LINE_BYTES = 16;
struct SlotImportSession {
    bool active = false;
    uint8_t slot = 0;
    size_t received = 0;
    uint8_t invalidCount = 0;
    uint32_t lastActivity = 0;
    uint8_t raw[SLOT_RAW_MAX_BYTES];
};
static SlotImportSession g_slotImport;
static constexpr uint32_t MAX_OFFLINE_SECONDS=366u*86400u;

// Shared persistence policy: failures lock writes instead of pretending success.
static SaveResult persistDeviceState() {
    return save_workflow::persistDevice(saveManager,deviceState);
}

// deep sleep 计时基准（RTC 慢时钟在 deep sleep 期间仍会继续走）
RTC_DATA_ATTR static uint64_t rtc_us_at_sleep = 0;
RTC_DATA_ATTR static uint32_t epoch_at_sleep = 0;

static const char* wakeupCauseName(esp_sleep_wakeup_cause_t c) {
    switch (c) {
        case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
        case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        case ESP_SLEEP_WAKEUP_WIFI: return "WIFI";
        case ESP_SLEEP_WAKEUP_COCPU: return "COCPU";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG: return "COCPU_TRAP";
        case ESP_SLEEP_WAKEUP_BT: return "BT";
        default: return "UNDEFINED";
    }
}

static const char* resetReasonName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

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

// 白狮子王: updateRhongoTimer 在 tick/批量内先置位时 check() 常拿不到 EVO_RHONGOMYNIAD。调用方须保证
// 本步刚由「非白狮终态」切入 is_rhongomyniad（与 !wasRhongo && pet.is_rhongomyniad 一致）。
static void showWhiteRhongoJustEntered(Form formBefore, int16_t srAfter) {
    EvolutionResult rh = {};
    rh.event = EVO_RHONGOMYNIAD;
    rh.form_before = formBefore;
    rh.form_after = FORM_WHITE_LANCER_RHONGOMYNIAD;
    rh.tier = seriousnessSystem.getTier(srAfter);
    DisplayManager::showEvolutionEvent(rh, srAfter);
    persistGalleryUnlockFromEvolution(rh);
}

void printStatus();
bool skipTime(uint32_t offlineSeconds);
static bool confirmTime(uint16_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t minute,uint32_t requestedEpoch=0);
static void printStatusForSnapshot(const PetState& p, uint32_t epochNow);

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static bool decodeHexToBytes(const char* hex, uint8_t* out, size_t maxOut, size_t* outLen) {
    size_t n = strlen(hex);
    if (n == 0 || (n % 2) != 0) return false;
    size_t bytes = n / 2;
    if (bytes > maxOut) return false;
    for (size_t i = 0; i < bytes; i++) {
        int hi = hexNibble(hex[i * 2]);
        int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *outLen = bytes;
    return true;
}

static void printHexLine(const uint8_t* data, size_t len) {
    static const char HEX_MAP[] = "0123456789ABCDEF";
    char line[EXPORT_LINE_BYTES * 2 + 1];
    if (len > EXPORT_LINE_BYTES) len = EXPORT_LINE_BYTES;
    for (size_t i = 0; i < len; i++) {
        line[i * 2] = HEX_MAP[(data[i] >> 4) & 0x0F];
        line[i * 2 + 1] = HEX_MAP[data[i] & 0x0F];
    }
    line[len * 2] = '\0';
    Serial.printf("SAVE_EXPORT_DATA %s\n", line);
}

static bool parseSlotArg(const char* arg, uint8_t* slotOut) {
    while (*arg == ' ') arg++;
    char* end = nullptr;
    long v = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || v < 0 || v >= (long)SAVE_SLOT_COUNT) return false;
    *slotOut = (uint8_t)v;
    return true;
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
        // 注意: 不再调用 showFeedResult/showFeedComboTriggered
        // 动画由 AnimationDirector::buildFeedSequence() 通过队列驱动
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
        if (outcome.mapo_tofu_triggered)
            Serial.printf("[MC] !! MAPO TOFU triggered (%d/%d) !!\n",
                outcome.mapo_tofu_total, MAPO_TOFU_CURSE_THRESHOLD);
        // 注意: 不再调用 showSpecialFoodConfirm()
        // 动画由 AnimationDirector::buildComboEatingSequence() 通过队列驱动
    },
    // onPokeStart
    []() {
        Serial.println("[MC] Poke animation started");
        // 注意: 不再调用 showPokeAnimation()
        // 动画由 AnimationDirector::buildPokeSequence() 通过队列驱动
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

        // 串门结束: 恢复主人宠物
        if (deviceState.is_visiting) {
            save_workflow::restoreOwner(saveManager,pet,deviceState,timeManager.now(),timeManager.getDay());
        }
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
    },
    // onInitialTimeEdit
    [](uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t fieldIndex,
       bool awaitingConfirm) {
        DisplayManager::showInitialTimeSetup(year, month, day, hour, minute, fieldIndex, awaitingConfirm);
    },
    // onInitialTimeConfirm
    [](uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
        confirmTime(year,month,day,hour,minute);
    }
};

// Time setup uses the original v9 recovery guard, including offline writes.
static bool confirmTime(uint16_t year,uint8_t month,uint8_t day,uint8_t hour,uint8_t minute,uint32_t requestedEpoch) {
    if(!saveManager.initialized()||saveManager.recoveryRequired()||saveManager.operationPending()) {
        Serial.println("[Time] FAILED: storage unavailable/recovery required; saved records preserved.");
        return false;
    }
    const auto oldPet=pet;const auto oldDevice=deviceState;
    const auto oldGallery=gallerySystem.getData();const auto oldClock=timeManager;
    const uint32_t timestamp=timeManager.timeInfoToEpoch(year,month,day,hour,minute,0);
    const uint32_t last=deviceState.device_clock_epoch;
    const bool wasWaiting=waitingForTimeSet;
    const uint32_t duration=(wasWaiting&&!reinitPetAfterInitialTimeConfirm&&last&&timestamp>last)?timestamp-last:0;
    if(duration>MAX_OFFLINE_SECONDS||uint32_t(pet.age_days)+duration/86400u>UINT16_MAX) {
        Serial.println("[Time] FAILED: offline/age capacity exceeded; no changes applied.");return false;
    }
    auto result=saveManager.beginOperation(deviceState.is_visiting?deviceState.owner_frozen_slot:255);
    auto failed=[&](){
        saveManager.requireRecovery();pet=oldPet;deviceState=oldDevice;
        gallerySystem.getData()=oldGallery;timeManager=oldClock;
        animQueue.clear();DisplayManager::stopAnimation();
        DisplayManager::showToast("Recovery: export then erase",5000);
        Serial.println("[Time] FAILED: persistence incomplete; recovery required, normal operation blocked.");
        return false;
    };
    if(result!=SAVE_OK)return failed();
    if(duration){
        const auto base=timeManager.epochToTimeInfo(last);
        timeManager.setSimulatedTime(base.year,base.month,base.day,base.hour,base.minute);
        if(!skipTime(duration))return failed();
    }else timeManager.setSimulatedTime(year,month,day,hour,minute);
    deviceState.device_clock_epoch=(!wasWaiting&&requestedEpoch)?requestedEpoch:timeManager.now();
    if(reinitPetAfterInitialTimeConfirm){pet.initNew(timeManager.now());gallerySystem.unlockForm(pet.form);}
    result=saveManager.saveGallery(gallerySystem.getData());
    if(result!=SAVE_OK)return failed();
    result=persistDeviceState();if(result!=SAVE_OK)return failed();
    const auto now=timeManager.now();result=saveManager.save(pet,now);
    if(result!=SAVE_OK)return failed();
    result=saveManager.finishOperation();if(result!=SAVE_OK)return failed();
    waitingForTimeSet=false;reinitPetAfterInitialTimeConfirm=false;saveManager.markSaved(now);
    if(wasWaiting){menuController.switchContext(UI_IDLE);DisplayManager::showSystemReady();
        Serial.println("[Main] Initial time confirmed, entering normal operation.");}
    else Serial.println("[Time] System time saved.");
    printStatus();return true;
}

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
    Serial.printf("  Rounds:     %d\n", deviceState.rounds);
    Serial.printf("  Visiting:   %s\n", deviceState.is_visiting ? "YES" : "NO");
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
    Serial.printf("  Gallery:    %d / %d unlocked\n",
                  gallerySystem.getData().getUnlockedCount(), FORM_COUNT);
    Serial.println("========================================");
}

static void printStatusForSnapshot(const PetState& p, uint32_t epochNow) {
    TimeInfo ti = timeManager.epochToTimeInfo(epochNow);
    char timeBuf[24];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             ti.year, ti.month, ti.day, ti.hour, ti.minute, ti.second);

    Serial.println("========================================");
    if (p.is_nobu) {
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
    Serial.printf("  Form:       %s\n", FORM_NAMES[p.form]);
    Serial.printf("  Base:       %s\n", FORM_NAMES[p.base_form]);
    Serial.printf("  Stage:      %s\n", STAGE_NAMES[p.stage]);
    Serial.printf("  Alignment:  %s\n", ALIGNMENT_NAMES[p.alignment]);
    Serial.printf("  Health:     %d / %d\n", p.health, HEALTH_MAX);
    Serial.printf("  Seriousness:%d / %d\n", p.seriousness, SERIOUSNESS_MAX);
    SeriousnessTier tier = seriousnessSystem.getCurrentTier(p);
    Serial.printf("  Tier:       %s\n", TIER_NAMES[tier]);
    Serial.printf("  Age:        Day %d", p.age_days + 1);
    if (p.stage == STAGE_CHILD)
        Serial.printf(" / %d (child)\n", CHILD_PERIOD_DAYS);
    else
        Serial.println(" (adult)");
    Serial.printf("  Rounds:     %d (device)\n", deviceState.rounds);
    Serial.printf("  Fed today:  %d / %d\n", p.daily_feed.feed_count, DAILY_FEED_LIMIT);
    if (p.last_poke_effect_time == 0) {
        Serial.println("  Poke:       available");
    } else {
        uint32_t elapsed = epochNow - p.last_poke_effect_time;
        if (elapsed >= POKE_COOLDOWN_SEC)
            Serial.println("  Poke:       available");
        else
            Serial.printf("  Poke:       cooldown %lus left\n", POKE_COOLDOWN_SEC - elapsed);
    }
    if (p.idle_paused_until > epochNow)
        Serial.printf("  Idle pause: %lus left\n", p.idle_paused_until - epochNow);
    if (p.mapo_tofu_count > 0)
        Serial.printf("  Mapo Tofu:  %d / %d\n", p.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
    if (p.is_rhongomyniad)
        Serial.println("  *** RHONGOMYNIAD ***");
    if (p.is_black_rhongomyniad)
        Serial.println("  *** BLACK RHONGOMYNIAD (Mapo Curse) ***");
    if (p.white_fun_form_locked && p.alignment == ALIGN_WHITE)
        Serial.printf("  Fun form:   %s (locked)\n", FORM_NAMES[p.white_fun_form]);
    RhongoTimerState rS = seriousnessSystem.getRhongoState(p, epochNow);
    if (rS == RHONGO_COUNTING) {
        uint32_t rem = seriousnessSystem.getRhongoRemaining(p, epochNow);
        Serial.printf("  Rhongo:     %luh left\n", rem / 3600);
    } else if (rS == RHONGO_TRIGGERED) {
        Serial.println("  Rhongo:     TRIGGERED");
    }
    uint32_t wait = feedingSystem.secondsUntilNextFeed(p, epochNow);
    if (p.daily_feed.feed_count >= DAILY_FEED_LIMIT)
        Serial.println("  Next feed:  LOCKED");
    else if (wait > 0)
        Serial.printf("  Next feed:  wait %lum\n", wait / 60);
    else
        Serial.println("  Next feed:  READY");
    int8_t wIdx = feedingSystem.getWindowIndex(ti.hour);
    const char* wN[] = {"Breakfast", "Lunch", "Dinner"};
    if (wIdx >= 0) Serial.printf("  Window:     %s\n", wN[wIdx]);
    else Serial.println("  Window:     None");
    Serial.printf("  Gallery:    %d / %d unlocked\n",
                  gallerySystem.getData().getUnlockedCount(), FORM_COUNT);
    Serial.println("========================================");
}

void printHelp() {
    Serial.println("=== Fate Tamagotchi Console ===");
    Serial.println("  s              Status");
    Serial.println("  fl             Food list");
    Serial.println("  t <min>        Advance N minutes");
    Serial.println("  d              Advance 1 day");
    Serial.println("  save / load / erase");
    Serial.println("  s0 / s1 / s2  Show per-slot snapshot status");
    Serial.println("  SAVE_SLOT_STATUS");
    Serial.println("  SAVE_EXPORT <slot>       Export full backup (pet + gallery)");
    Serial.println("  SAVE_EXPORT_PET <slot>   Export pet only (for visit/trade)");
    Serial.println("  SAVE_IMPORT_BEGIN <slot>");
    Serial.println("  SAVE_IMPORT_DATA <hex>");
    Serial.println("  SAVE_IMPORT_COMMIT / SAVE_IMPORT_ABORT");
    Serial.println("  reset          Destroy & reset (ends visit if visiting)");
    Serial.println("  stime Y M D H m");
    Serial.println("  SET_TIME <epoch>  Set system time (unix timestamp)");
    Serial.println("  hp/sr/age <val>  Debug set");
    Serial.println("  grad           Force graduation");
    Serial.println("  mapo           Debug +1 mapo count");
    Serial.println("  FORCE_NOBU     Force nobu route");
    Serial.println("--- Gallery ---");
    Serial.println("  UNLOCK_ALL     Unlock all gallery forms");
    Serial.println("  RESET_GALLERY  Reset gallery (lock all)");
    Serial.println("--- Device ---");
    Serial.println("  devinfo        Show device state");
    Serial.println("--- Power ---");
    Serial.println("  bright <0-15>  Set screen brightness");
    Serial.println("  dim <0-15>     Set dim brightness");
    Serial.println("  dim_t <sec>    Set dim timeout");
    Serial.println("  off_t <sec>    Set off timeout");
    Serial.println("  pwrsave        Save power config to NVS (bright/dim/dim_t/off_t 已自动保存)");
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

bool doDayEnd() {
    if(pet.age_days==UINT16_MAX){Serial.println("[DayEnd] REJECTED: age capacity exhausted.");return false;}
    Serial.println("[DayEnd] --- Processing ---");
    DisplayManager::showDayEndStart();
    uint32_t now = timeManager.now();

    // 清除 poke 暂停: 日结算模拟的是"过去一天"的时间流逝,
    // idle_paused_until 是相对于当前时间的未来时间戳,
    // 如果不清除会导致整天的 SR 增长被错误阻塞
    pet.idle_paused_until = 0;

    bool wasRhongo = pet.is_rhongomyniad;
    Form formBeforeDayEndIdle = pet.form;
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
        if (rhongoTriggeredInDayEnd) {
            showWhiteRhongoJustEntered(formBeforeDayEndIdle, pet.seriousness);
        }
        DisplayManager::showDayEndTerminalState();
        feedingSystem.resetDaily(pet, timeManager.getDay());
        pet.age_days++;
        uint32_t ts = timeManager.now();
        SaveResult dR = saveManager.save(pet, ts);
        if (dR == SAVE_OK) {
            saveManager.markSaved(ts);
        } else {
            Serial.printf("[DayEnd] WARN: save failed (%d)\n", (int)dR);
        }
        return dR==SAVE_OK;
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

    uint32_t ts = timeManager.now();
    SaveResult dR = saveManager.save(pet, ts);
    if (dR == SAVE_OK) {
        saveManager.markSaved(ts);
    } else {
        Serial.printf("[DayEnd] WARN: save failed (%d)\n", (int)dR);
    }
    return dR==SAVE_OK;
}

void doReset() {
    uint32_t now = timeManager.now();
    Form destroyedForm = pet.form;
    uint16_t prevAgeDays = pet.age_days;
    DisplayManager::showDestroyExecuted(destroyedForm);

    // === 串门结束逻辑 ===
    if (deviceState.is_visiting) {
        save_workflow::restoreOwner(saveManager,pet,deviceState,now,timeManager.getDay());
        printStatus();
        return;
    }

    // === 正常 reset 逻辑 ===
    uint16_t prevRounds = deviceState.rounds;

    // 指定轮次保底: round 329 触发 reset 后 (即 rounds=330) 必定进入 Nobu 路线
    if (prevRounds == 329) {
        Serial.println("[Reset] Forced Nobu: round 329 -> round 330");
        evolutionSystem.destroyToNobu(pet, now, prevAgeDays);
        deviceState.rounds = 330;
        persistDeviceState();
        if (gallerySystem.unlockForm(pet.form)) {
            saveManager.saveGallery(gallerySystem.getData());
        }
        feedingSystem.resetDaily(pet, timeManager.getDay());
        uint32_t ts = timeManager.now();
        SaveResult r = saveManager.save(pet, ts);
        if (r == SAVE_OK) saveManager.markSaved(ts);
        printStatus();
        return;
    }

    // nobu 彩蛋判定
    uint32_t roll = esp_random() % 1000;
    uint32_t threshold = NOBU_BASE_PERMILLE;
    if (prevAgeDays == 5) {
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

    deviceState.rounds = prevRounds + 1;
    persistDeviceState();

    if (gallerySystem.unlockForm(pet.form)) {
        saveManager.saveGallery(gallerySystem.getData());
    }

    feedingSystem.resetDaily(pet, timeManager.getDay());
    uint32_t ts = timeManager.now();
    SaveResult r = saveManager.save(pet, ts);
    if (r == SAVE_OK) saveManager.markSaved(ts);
    printStatus();
}

// ============================================================================
//  离线时间补偿 - 模拟离线期间经过的天数
// ============================================================================

bool skipTime(uint32_t offlineSeconds) {
    if(offlineSeconds>MAX_OFFLINE_SECONDS||uint32_t(pet.age_days)+offlineSeconds/86400u>UINT16_MAX||
       uint64_t(timeManager.now())+offlineSeconds>UINT32_MAX) {
        Serial.println("[Offline] REJECTED: offline/age/time capacity exceeded; no changes applied.");return false;
    }
    if (offlineSeconds < 60) {
        Serial.println("[Offline] Less than 1 minute offline, no compensation needed.");
        return true;
    }

    uint32_t offlineDays = offlineSeconds / 86400;
    uint32_t remainingMinutes = (offlineSeconds % 86400) / 60;

    Serial.printf("[Offline] Compensating: %lu days + %lu minutes\n", offlineDays, remainingMinutes);

    // 逐天结算 (与 d 命令等效)
    for (uint32_t i = 0; i < offlineDays; i++) {
        if(!doDayEnd())return false;
        timeManager.advanceDays(1);
        timeManager.checkNewDay();  // 消耗 newDay 标记
    }

    // 结算剩余分钟的严肃值增长
    if (remainingMinutes > 0) {
        // 防守式恢复：若 idle_paused_until 丢失，但 poke 生效时间存在，
        // 则可推导出本次应有的暂停截止，避免离线补偿把暂停窗口算成普通增长。
        if (pet.last_poke_effect_time > 0) {
            uint32_t derivedPauseUntil = pet.last_poke_effect_time + POKE_IDLE_PAUSE_SEC;
            if (derivedPauseUntil > pet.idle_paused_until) {
                Serial.printf("[Offline] Recover idle pause: %lu -> %lu (from poke)\n",
                              pet.idle_paused_until, derivedPauseUntil);
                pet.idle_paused_until = derivedPauseUntil;
            }
        }

        bool wasRhongo = pet.is_rhongomyniad;
        Form formBeforeOfflineBatch = pet.form;
        uint32_t beforeEpoch = timeManager.now();
        Serial.printf("[Offline] Before batch: now=%lu pause_until=%lu last_poke_effect=%lu rem=%lu min\n",
                      beforeEpoch, pet.idle_paused_until, pet.last_poke_effect_time, remainingMinutes);
        timeManager.advanceMinutes(remainingMinutes);
        uint32_t now = timeManager.now();
        IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, remainingMinutes, now);
        Serial.printf("[Offline] After batch: now=%lu SR %d->%d remainder=%u\n",
                      now, iR.seriousness_before, iR.seriousness_after, pet.idle_minute_remainder);
        if (!wasRhongo && pet.is_rhongomyniad) {
            showWhiteRhongoJustEntered(formBeforeOfflineBatch, pet.seriousness);
        }
        if (iR.tier_changed) {
            Serial.printf("[Offline] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
            EvolutionResult eR = evolutionSystem.check(pet, now);
            if (eR.event != EVO_NONE) {
                Serial.printf("[Offline] Evo: %s -> %s\n", FORM_NAMES[eR.form_before], FORM_NAMES[eR.form_after]);
                DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                persistGalleryUnlockFromEvolution(eR);
            }
        }
    }

    Serial.printf("[Offline] Compensation complete. Age: Day %d\n", pet.age_days);
    uint32_t ts = timeManager.now();
    SaveResult oR = saveManager.save(pet, ts);
    if (oR == SAVE_OK) {
        saveManager.markSaved(ts);
    } else {
        Serial.printf("[Offline] WARN: save failed (%d)\n", (int)oR);
    }
    printStatus();
    return oR==SAVE_OK;
}

static void expireSerialInput() {
    const auto now=millis();
    if(g_slotImport.active&&uint32_t(now-g_slotImport.lastActivity)>30000){
        g_slotImport={};Serial.println("[SaveImport] Session timeout; ABORTED.");
    }
    if((cmdLen||cmdDiscard)&&uint32_t(now-cmdLastActivity)>30000){
        cmdLen=0;cmdDiscard=false;g_slotImport={};Serial.println("[Cmd] Partial line timeout; import ABORTED.");
    }
}
void processCommand(const char* cmd) {
    expireSerialInput();
    while (*cmd == ' ') cmd++;
    if (strlen(cmd) == 0) return;

    if (strcmp(cmd, "SAVE_SLOT_STATUS") == 0) {
        for (uint8_t slot = 0; slot < SAVE_SLOT_COUNT; slot++) {
            SaveHeader hdr;
            SaveResult st = SAVE_OK;
            if (saveManager.getSlotStatus(slot, hdr, st)) {
                Serial.printf("slot%u:seq=%lu,time=%lu,ver=%u,size=%u,crc=0x%04X\n",
                              slot,
                              (unsigned long)hdr.sequence,
                              (unsigned long)hdr.save_time,
                              (unsigned)hdr.version,
                              (unsigned)hdr.data_size,
                              (unsigned)hdr.checksum);
            } else {
                Serial.printf("slot%u:ERR(%d)\n", slot, (int)st);
            }
        }
        return;
    }

    if (strncmp(cmd, "SAVE_EXPORT ", 12) == 0) {
        uint8_t slot = 0;
        if (!parseSlotArg(cmd + 12, &slot)) {
            Serial.println("[SaveExport] Usage: SAVE_EXPORT <slot(0|1|2)>");
            return;
        }
        SaveHeader hdr;
        PetState state;
        GalleryData g;
        bool galleryBound = false;
        SaveResult r = saveManager.exportSlotRawWithGallery(slot, hdr, state, g, galleryBound);
        if (r != SAVE_OK) {
            Serial.printf("[SaveExport] FAILED (%d)\n", (int)r);
            return;
        }
        uint8_t raw[SLOT_RAW_WITH_GALLERY_BYTES];
        if (!galleryBound) {
            Serial.println("[SaveExport] WARN: slot gallery snapshot missing, exporting minimal consistent gallery.");
        }
        memcpy(raw, &hdr, sizeof(SaveHeader));
        memcpy(raw + sizeof(SaveHeader), &state, sizeof(PetState));
        memcpy(raw + SLOT_RAW_BASE_BYTES, &g, sizeof(GalleryData));
        Serial.printf("SAVE_EXPORT_BEGIN slot=%u bytes=%u\n", slot, (unsigned)SLOT_RAW_WITH_GALLERY_BYTES);
        for (size_t off = 0; off < SLOT_RAW_WITH_GALLERY_BYTES; off += EXPORT_LINE_BYTES) {
            size_t take = SLOT_RAW_WITH_GALLERY_BYTES - off;
            if (take > EXPORT_LINE_BYTES) take = EXPORT_LINE_BYTES;
            printHexLine(raw + off, take);
        }
        Serial.printf("SAVE_EXPORT_END slot=%u bytes=%u\n", slot, (unsigned)SLOT_RAW_WITH_GALLERY_BYTES);
        return;
    }

    // 导出仅运行态 (用于串门/交换宠物)
    if (strncmp(cmd, "SAVE_EXPORT_PET ", 16) == 0) {
        uint8_t slot = 0;
        if (!parseSlotArg(cmd + 16, &slot)) {
            Serial.println("[SaveExport] Usage: SAVE_EXPORT_PET <slot(0|1|2)>");
            return;
        }
        SaveHeader hdr;
        PetState state;
        SaveResult r = saveManager.exportSlotRaw(slot, hdr, state);
        if (r != SAVE_OK) {
            Serial.printf("[SaveExport] FAILED (%d)\n", (int)r);
            return;
        }
        uint8_t raw[SLOT_RAW_BASE_BYTES];
        memcpy(raw, &hdr, sizeof(SaveHeader));
        memcpy(raw + sizeof(SaveHeader), &state, sizeof(PetState));
        Serial.printf("SAVE_EXPORT_BEGIN slot=%u bytes=%u\n", slot, (unsigned)SLOT_RAW_BASE_BYTES);
        for (size_t off = 0; off < SLOT_RAW_BASE_BYTES; off += EXPORT_LINE_BYTES) {
            size_t take = SLOT_RAW_BASE_BYTES - off;
            if (take > EXPORT_LINE_BYTES) take = EXPORT_LINE_BYTES;
            printHexLine(raw + off, take);
        }
        Serial.printf("SAVE_EXPORT_END slot=%u bytes=%u\n", slot, (unsigned)SLOT_RAW_BASE_BYTES);
        return;
    }

    if (strncmp(cmd, "SAVE_IMPORT_BEGIN ", 18) == 0) {
        g_slotImport={};
        uint8_t slot = 0;
        if (!parseSlotArg(cmd + 18, &slot)) {
            Serial.println("[SaveImport] Usage: SAVE_IMPORT_BEGIN <slot(0|1|2)>");
            return;
        }
        // 串门期间禁止导入
        if (deviceState.is_visiting || !saveManager.initialized() || saveManager.recoveryRequired() || saveManager.operationPending()) {
            g_slotImport={};
            Serial.println("[SaveImport] REJECTED: import forbidden during visit.");
            return;
        }
        g_slotImport.active = true;
        g_slotImport.lastActivity=millis();
        g_slotImport.slot = slot;
        g_slotImport.received = 0;
        g_slotImport.invalidCount = 0;
        memset(g_slotImport.raw, 0, sizeof(g_slotImport.raw));
        Serial.printf("[SaveImport] READY slot=%u target_bytes=%u (legacy=%u) offset=1\n",
                      slot, (unsigned)SLOT_RAW_WITH_GALLERY_BYTES, (unsigned)SLOT_RAW_BASE_BYTES);
        return;
    }

    if (strncmp(cmd, "SAVE_IMPORT_DATA ", 17) == 0) {
        if (!g_slotImport.active) {
            Serial.println("[SaveImport] No active session. Use SAVE_IMPORT_BEGIN first.");
            return;
        }
        const char* hex=cmd+17;size_t offset=g_slotImport.received;
        if(*hex=='@'){
            char* end=nullptr;const auto value=strtoul(hex+1,&end,10);
            if(end==hex+1||*end!=' '||value>SLOT_RAW_MAX_BYTES){g_slotImport={};Serial.println("[SaveImport] Invalid hex payload/offset; ABORTED.");return;}
            offset=value;hex=end+1;
        }
        uint8_t decoded[SLOT_RAW_MAX_BYTES];size_t wrote=0;
        if(!decodeHexToBytes(hex,decoded,sizeof(decoded),&wrote)||offset>g_slotImport.received||wrote>SLOT_RAW_MAX_BYTES-offset||
           (offset<g_slotImport.received&&(wrote>g_slotImport.received-offset||memcmp(decoded,g_slotImport.raw+offset,wrote)))){
            g_slotImport={};Serial.println("[SaveImport] Invalid hex payload/offset; ABORTED.");return;
        }
        if(offset==g_slotImport.received){memcpy(g_slotImport.raw+offset,decoded,wrote);g_slotImport.received+=wrote;}
        g_slotImport.lastActivity=millis();

        Serial.printf("[SaveImport] RECV %u/%u\n", (unsigned)g_slotImport.received, (unsigned)SLOT_RAW_WITH_GALLERY_BYTES);
        return;
    }

    if (strcmp(cmd, "SAVE_IMPORT_ABORT") == 0) {
        g_slotImport.active = false;
        g_slotImport.received = 0;
        g_slotImport.invalidCount = 0;
        Serial.println("[SaveImport] ABORTED");
        return;
    }

    if (strcmp(cmd, "SAVE_IMPORT_COMMIT") == 0) {
        if (!g_slotImport.active) {
            Serial.println("[SaveImport] No active session.");
            return;
        }

        // 串门期间禁止任何导入
        if (deviceState.is_visiting || !saveManager.initialized() || saveManager.recoveryRequired() || saveManager.operationPending()) {
            g_slotImport={};
            Serial.println("[SaveImport] REJECTED: import forbidden during visit (串门).");
            g_slotImport.active = false;
            g_slotImport.received = 0;
            g_slotImport.invalidCount = 0;
            return;
        }

        const bool alignClock=waitingForTimeSet&&deviceState.device_clock_epoch>0;
        const uint32_t importNow=alignClock?deviceState.device_clock_epoch:timeManager.now();
        const auto localTime=timeManager.epochToTimeInfo(importNow);
        const auto result=save_workflow::commitImport(saveManager,pet,deviceState,gallerySystem.getData(),
            g_slotImport.slot,g_slotImport.raw,g_slotImport.received,importNow,localTime.day);
        g_slotImport.active=false;g_slotImport.received=0;g_slotImport.invalidCount=0;
        if(result!=SAVE_OK){Serial.printf("[SaveImport] FAILED (%u); records retained.\n",unsigned(result));return;}
        if(alignClock)timeManager.setSimulatedTime(localTime.year,localTime.month,localTime.day,localTime.hour,localTime.minute);

        // 导入成功: 取消"新游戏初始化"标志, 避免后续设时回调覆盖刚导入的宠物
        reinitPetAfterInitialTimeConfirm = false;

        // 若当前在等待设时状态, 导入成功后直接进入正常运行 (继承设备时间)
        if (waitingForTimeSet) {
            waitingForTimeSet = false;

            menuController.switchContext(UI_IDLE);
            DisplayManager::showSystemReady();
            Serial.println("[SaveImport] Exited time-setup wait after successful import.");
        }

        g_slotImport.active = false;
        g_slotImport.received = 0;
        g_slotImport.invalidCount = 0;
        return;
    }

    if (strcmp(cmd, "erase") == 0) {
        const auto result=saveManager.erase();
        if(result==SAVE_OK){g_slotImport={};deviceState.init();waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=true;}
        Serial.println(result==SAVE_OK?"[Save] Erased":"[Save] Erase FAILED; records retained where possible.");
        DisplayManager::showToast(result==SAVE_OK?"Save erased":"Erase failed!",1500);
        return;
    }

    // SET_TIME 命令: 设置系统时间 (离线补偿用, 任何状态下可用)
    if (strncmp(cmd, "SET_TIME ", 9) == 0) {
        uint32_t timestamp = strtoul(cmd + 9, nullptr, 10);
        if (timestamp < 1000000000UL) {
            Serial.println("[Time] Invalid timestamp (too small). Use unix epoch seconds.");
            return;
        }
        TimeInfo t = timeManager.epochToTimeInfo(timestamp);
        if(deviceState.is_visiting&&!waitingForTimeSet){Serial.println("[Time] REJECTED: cannot change time during visit.");return;}
        confirmTime(t.year,t.month,t.day,t.hour,t.minute,timestamp);
        return;
    }

    // 等待时间设置期间, 只允许时间设置相关和 s/h 命令
    if (waitingForTimeSet || saveManager.recoveryRequired() || !saveManager.initialized()) {
        if (strcmp(cmd, "s") == 0) { printStatus(); return; }
        if (strcmp(cmd, "s0") == 0 || strcmp(cmd, "s1") == 0 || strcmp(cmd, "s2") == 0) {
            uint8_t slot = (uint8_t)(cmd[1] - '0');
            SaveHeader hdr;
            PetState snap;
            SaveResult sr = saveManager.exportSlotRaw(slot, hdr, snap);
            if (sr != SAVE_OK) {
                Serial.printf("[SlotStatus] slot%u unavailable (err=%d)\n", slot, (int)sr);
                return;
            }
            Serial.printf("[SlotStatus] slot%u seq=%lu save_time=%lu\n",
                          slot, (unsigned long)hdr.sequence, (unsigned long)hdr.save_time);
            printStatusForSnapshot(snap, hdr.save_time);
            return;
        }
        if (strcmp(cmd, "h") == 0) { printHelp(); return; }
        Serial.println("[Main] Waiting for date/time setup. Other commands blocked.");
        return;
    }

    if (strcmp(cmd, "s") == 0) { printStatus(); return; }
    if (strcmp(cmd, "s0") == 0 || strcmp(cmd, "s1") == 0 || strcmp(cmd, "s2") == 0) {
        uint8_t slot = (uint8_t)(cmd[1] - '0');
        SaveHeader hdr;
        PetState snap;
        SaveResult sr = saveManager.exportSlotRaw(slot, hdr, snap);
        if (sr != SAVE_OK) {
            Serial.printf("[SlotStatus] slot%u unavailable (err=%d)\n", slot, (int)sr);
            return;
        }
        Serial.printf("[SlotStatus] slot%u seq=%lu save_time=%lu\n",
                      slot, (unsigned long)hdr.sequence, (unsigned long)hdr.save_time);
        printStatusForSnapshot(snap, hdr.save_time);
        return;
    }
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
        if (deviceState.is_visiting) {
            Serial.println("[Time] REJECTED: cannot advance day during visit.");
            return;
        }
        doDayEnd();
        timeManager.advanceDays(1);
        DisplayManager::showToast("Day advanced", 1000);
        timeManager.checkNewDay();
        return;
    }
    if (strcmp(cmd, "save") == 0) {
        uint32_t ts = timeManager.now();
        SaveResult r = saveManager.save(pet, ts);
        if (r == SAVE_OK) {
            saveManager.markSaved(ts);
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

    if (strcmp(cmd, "reset") == 0) { doReset(); return; }
    if (strcmp(cmd, "FORCE_NOBU") == 0) {
        uint32_t now = timeManager.now();
        uint16_t prevAgeDays = pet.age_days;
        evolutionSystem.destroyToNobu(pet, now, prevAgeDays);
        deviceState.rounds++;
        persistDeviceState();
        feedingSystem.resetDaily(pet, timeManager.getDay());
        saveManager.save(pet, now);
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
    if (strcmp(cmd, "devinfo") == 0) {
        Serial.println("--- Device State ---");
        Serial.printf("  Rounds:       %u\n", deviceState.rounds);
        Serial.printf("  Visiting:     %s\n", deviceState.is_visiting ? "YES" : "NO");
        Serial.printf("  Clock epoch:  %lu\n", (unsigned long)deviceState.device_clock_epoch);
        if (deviceState.is_visiting) {
            Serial.printf("  Owner slot:   %u\n", deviceState.owner_frozen_slot);
            Serial.printf("  Visit start:  %lu\n", (unsigned long)deviceState.visit_start_epoch);
        }
        Serial.println("--------------------");
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
                uint32_t ts = timeManager.now();
                saveManager.save(pet, ts);
                saveManager.markSaved(ts);
            }
        }
        return;
    }

    // t <min>
    if (cmd[0] == 't' && cmd[1] == ' ') {
        if (deviceState.is_visiting) {
            Serial.println("[Time] REJECTED: cannot advance time during visit.");
            return;
        }
        int minutes = atoi(cmd + 2);
        if (minutes > 0) {
            const int MAX_SERIAL_ADVANCE_MIN = 10080;  // 7 days; avoids huge onIdleBatch loops
            if (minutes > MAX_SERIAL_ADVANCE_MIN) {
                Serial.printf("[Time] Clamped advance to %d min (requested %d)\n",
                              MAX_SERIAL_ADVANCE_MIN, minutes);
                minutes = MAX_SERIAL_ADVANCE_MIN;
            }
            timeManager.advanceMinutes(minutes);
            DisplayManager::showToast("Time advanced", 1000);
            uint32_t now = timeManager.now();
            bool wasRhongoT = pet.is_rhongomyniad;
            Form formBeforeT = pet.form;
            IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, (uint32_t)minutes, now);
            Serial.printf("[Idle] SR: %d->%d (+%dm)\n", iR.seriousness_before, iR.seriousness_after, minutes);
            if (!wasRhongoT && pet.is_rhongomyniad) {
                showWhiteRhongoJustEntered(formBeforeT, pet.seriousness);
            }
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
        if (deviceState.is_visiting) {
            Serial.println("[Time] REJECTED: cannot change time during visit.");
            return;
        }
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
        powerManager.onUserActivity();
        return;
    }
    if (strncmp(cmd, "off_t ", 6) == 0) {
        uint32_t val = strtoul(cmd + 6, nullptr, 10);
        powerManager.setOffTimeout(val);
        powerManager.onUserActivity();
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
    expireSerialInput();
    while(Serial.available()){
        const char c=Serial.read();cmdLastActivity=millis();
        if(c=='\n'||c=='\r'){
            if(cmdDiscard){cmdDiscard=false;cmdLen=0;continue;}
            if(cmdLen){cmdBuf[cmdLen]='\0';cmdLen=0;processCommand(cmdBuf);}
        }else if(!cmdDiscard){
            if(cmdLen<sizeof(cmdBuf)-1)cmdBuf[cmdLen++]=c;
            else{cmdLen=0;cmdDiscard=true;g_slotImport={};Serial.println("[Cmd] Line too long; discarded, import ABORTED.");}
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
    if (saveManager.init() != SAVE_OK) {
        Serial.println("[Main] WARNING: saveManager init failed; persistence may be unavailable");
    }

    // 初始化图鉴系统
    gallerySystem.init();

    esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    bool wokeFromDeepSleep = (wakeCause != ESP_SLEEP_WAKEUP_UNDEFINED);
    Serial.printf("[Boot] Wakeup cause: %s\n", wakeupCauseName(wakeCause));

    esp_reset_reason_t resetReason = esp_reset_reason();
    Serial.printf("[Boot] Reset reason: %s\n", resetReasonName(resetReason));

    const auto startup=save_workflow::loadStartup(saveManager,pet);
    if (startup!=save_workflow::Startup::NewGame) {
        if (startup==save_workflow::Startup::Loaded) {
            Serial.println("[Main] Save loaded");
            DisplayManager::showSaveLoaded();

            // 加载设备态
            saveManager.loadDeviceState(deviceState);
            // 自洽性: 有存档但无设备态时 rounds 不应为 0
            if (deviceState.rounds == 0) {
                deviceState.rounds = 1;
                persistDeviceState();
            }

            // 加载图鉴数据
            saveManager.loadGallery(gallerySystem.getData());
            // 确保当前形态已解锁
            gallerySystem.unlockForm(pet.form);

            // 时间恢复逻辑：
            // - deep sleep 唤醒：用 RTC 推进, 触发离线补偿
            // - 断电类复位 (POWERON/BROWNOUT)：需要用户设时 (设备态时间丢失)
            // - 非断电类复位：沿用设备态时间继续运行
            if (resetReason == ESP_RST_DEEPSLEEP || wokeFromDeepSleep) {
                uint64_t nowRtcUs = esp_rtc_get_time_us();
                uint32_t resumedEpoch = (deviceState.device_clock_epoch > 0)
                    ? deviceState.device_clock_epoch : timeManager.now();
                uint32_t sleptSec = 0;
                if (rtc_us_at_sleep != 0 && epoch_at_sleep != 0 && nowRtcUs >= rtc_us_at_sleep) {
                    sleptSec = (uint32_t)((nowRtcUs - rtc_us_at_sleep) / 1000000ULL);
                    resumedEpoch = epoch_at_sleep + sleptSec;
                    Serial.printf("[Boot] Deep sleep elapsed: %lu sec\n", sleptSec);
                } else {
                    Serial.println("[Boot] Deep sleep markers missing; falling back to device clock.");
                }

                if (epoch_at_sleep != 0 && sleptSec > 0) {
                    TimeInfo base = timeManager.epochToTimeInfo(epoch_at_sleep);
                    timeManager.setSimulatedTime(base.year, base.month, base.day, base.hour, base.minute);
                    // 访客与非串门一致走 skipTime；串门期间: 访客宠物正常接受离线补偿 主人仍在冻结槽内，不会被改写
                    skipTime(sleptSec);
                } else {
                    TimeInfo t = timeManager.epochToTimeInfo(resumedEpoch);
                    timeManager.setSimulatedTime(t.year, t.month, t.day, t.hour, t.minute);
                }

                // 更新设备时间
                deviceState.device_clock_epoch = timeManager.now();
                persistDeviceState();

                waitingForTimeSet = false;
                powerManager.onWakeFromSleep();
                Serial.println("[Boot] Resumed (deep sleep): RTC-based time restore.");
            } else if (resetReason == ESP_RST_POWERON || resetReason == ESP_RST_BROWNOUT) {
                if (deviceState.device_clock_epoch > 0) {
                    // 有设备时间记录: 需要用户确认当前时间以计算离线时长
                    waitingForTimeSet = true;
                    reinitPetAfterInitialTimeConfirm = false;
                    Serial.println("[Main] Power-loss boot: please set current date/time via UI.");
                } else {
                    // 全新设备, 无时间记录
                    waitingForTimeSet = true;
                    reinitPetAfterInitialTimeConfirm = false;
                    Serial.println("[Main] No device clock: please set date/time.");
                }
            } else {
                // 非断电复位: 沿用设备态时间
                if (deviceState.device_clock_epoch > 0) {
                    TimeInfo t = timeManager.epochToTimeInfo(deviceState.device_clock_epoch);
                    timeManager.setSimulatedTime(t.year, t.month, t.day, t.hour, t.minute);
                }
                waitingForTimeSet = false;
                reinitPetAfterInitialTimeConfirm = false;
                Serial.println("[Boot] Non-power reset: restored to device clock time.");
            }
        } else {
            Serial.println("[Main] Recovery required; saved records preserved, automatic writes blocked.");
            DisplayManager::showSaveCorruptedNewGame();
            pet.initNew(timeManager.now());
            saveManager.loadDeviceState(deviceState);
            if (deviceState.rounds == 0) {
                deviceState.rounds = 1;
                persistDeviceState();
            }
            waitingForTimeSet = true;
            reinitPetAfterInitialTimeConfirm = true;
        }
    } else {
        Serial.println("[Main] New game");
        DisplayManager::showNewGame();
        pet.initNew(timeManager.now());
        // 初始化设备态
        deviceState.init();
        deviceState.rounds = 1;
        // 新游戏: 解锁初始形态
        gallerySystem.unlockForm(pet.form);
        waitingForTimeSet = true;
        reinitPetAfterInitialTimeConfirm = true;
    }

    menuController.init(&pet, &deviceState, &gameCallbacks);
    Serial.println("[Main] MenuController initialized");

    printHelp();
    printStatus();

    if (waitingForTimeSet) {
        uint32_t baseEpoch = (deviceState.device_clock_epoch > 0)
            ? deviceState.device_clock_epoch
            : timeManager.timeInfoToEpoch(2026, 1, 1, 0, 0, 0);
        menuController.startInitialTimeSetup(baseEpoch);
        Serial.println("\n[Main] Waiting for date/time setup (buttons)...\n> ");
    } else {
        DisplayManager::showSystemReady();
        Serial.println("\nReady.\n> ");
    }
}

// ============================================================================
//  Deep Sleep 进入 (保持内部计时)
// ============================================================================

void enterDeepSleep() {
    // 入睡前存档：每次尝试先采一次游戏时间 nowEpoch，save 与 verify 共用该值（验证的是「本笔写入的时间戳」，非 verify 时刻再读时钟）
    // 读回校验 save_time + pet 一致；失败重试，至多 3 次（首次 + 重试 2 次）
    const int SLEEP_SAVE_ATTEMPTS = 3;
    bool sleepSaveVerified = false;
    uint32_t verifiedEpoch = 0;
    for (int attempt = 0; attempt < SLEEP_SAVE_ATTEMPTS; attempt++) {
        uint32_t nowEpoch = timeManager.now();  // 与本轮 save / verifyLatestSave 绑定，避免延迟导致时间比对误判
        SaveResult r = saveManager.save(pet, nowEpoch);
        if (r != SAVE_OK) {
            Serial.printf("[Sleep] pre-sleep save failed (try %d/%d, err=%d)\n",
                          attempt + 1, SLEEP_SAVE_ATTEMPTS, (int)r);
            continue;
        }
        if (!saveManager.verifyLatestSave(nowEpoch, pet)) {
            Serial.printf("[Sleep] pre-sleep save verify failed (try %d/%d)\n",
                          attempt + 1, SLEEP_SAVE_ATTEMPTS);
            continue;
        }
        sleepSaveVerified = true;
        verifiedEpoch = nowEpoch;
        saveManager.markSaved(nowEpoch);
        break;
    }
    if (!sleepSaveVerified) {
        Serial.println("[Sleep] ABORT: save not verified; not entering deep sleep");
        Serial.flush();
        return;
    }

    // 保存设备时间到设备态
    deviceState.device_clock_epoch = verifiedEpoch;
    persistDeviceState();

    Serial.println("[Power] Entering deep sleep...");
    Serial.flush();

    // 记录 deep sleep 基准：RTC us + epoch（与已通过校验的那次 save 的 saveTime 一致）
    epoch_at_sleep = verifiedEpoch;
    rtc_us_at_sleep = esp_rtc_get_time_us();

    // 确保 RTC 外设域保持供电，否则 RTC pullup 可能在 deep sleep 期间失效，导致 EXT1(ANY_LOW) 秒醒
    esp_err_t sleepCfg = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    if (sleepCfg != ESP_OK) {
        Serial.printf("[Sleep] esp_sleep_pd_config failed: %s\n", esp_err_to_name(sleepCfg));
    }

    // 防止“刚入睡就立刻唤醒”：
    // EXT1(ANY_LOW) 在入睡瞬间如果检测到唤醒脚为低电平会马上唤醒。
    // 运行态的 INPUT_PULLUP 在 deep sleep 下不一定保持，因此这里为可用的 RTC GPIO 显式开启 RTC 上拉。
    const gpio_num_t wakePins[] = {
        (gpio_num_t)PIN_BTN_L,
        (gpio_num_t)PIN_BTN_M,
        (gpio_num_t)PIN_BTN_R,
    };
    for (gpio_num_t pin : wakePins) {
        int level = digitalRead((int)pin);
        Serial.printf("[Sleep] Wake pin GPIO%d level=%d rtc_valid=%d\n",
                      (int)pin, level, rtc_gpio_is_valid_gpio(pin) ? 1 : 0);
        if (rtc_gpio_is_valid_gpio(pin)) {
            rtc_gpio_pulldown_dis(pin);
            rtc_gpio_pullup_en(pin);
            rtc_gpio_hold_en(pin);
        }
    }

    // 配置唤醒源: 任意按键 (GPIO) 唤醒
    uint64_t wakeupMask = 0;
    wakeupMask |= (1ULL << PIN_BTN_L);
    wakeupMask |= (1ULL << PIN_BTN_M);
    wakeupMask |= (1ULL << PIN_BTN_R);
    const int EXT1_WAKEUP_RETRIES = 8;
    for (int attempt = 0; attempt < EXT1_WAKEUP_RETRIES; ++attempt) {
        sleepCfg = esp_sleep_enable_ext1_wakeup(wakeupMask, ESP_EXT1_WAKEUP_ANY_LOW);
        if (sleepCfg == ESP_OK) break;
        Serial.printf("[Sleep] esp_sleep_enable_ext1_wakeup failed (try %d/%d): %s\n",
                      attempt + 1, EXT1_WAKEUP_RETRIES, esp_err_to_name(sleepCfg));
        delay(5);
    }
    if (sleepCfg != ESP_OK) {
        // 未真正入睡时释放 hold，避免唤醒脚长期卡在 RTC hold 影响按键
        for (gpio_num_t pin : wakePins) {
            if (rtc_gpio_is_valid_gpio(pin)) {
                rtc_gpio_hold_dis(pin);
            }
        }
        return;
    }

    // 不再用“每60秒唤醒”来维持计时：
    // deep sleep 期间由 RTC 计数器自然推进；唤醒时一次性计算经过秒数并补偿。

    // 进入 deep sleep
    esp_deep_sleep_start();
}

void loop() {
    // 1. Serial commands (always active, even during time-set wait)
    readSerialCommand();

    if(!saveManager.initialized()||saveManager.recoveryRequired()){
        DisplayManager::update(millis());DisplayManager::renderIfDirty();delay(10);return;
    }
    // 等待时间设置期间, 只处理串口命令和显示更新
    if (waitingForTimeSet) {
        menuController.update();
        powerManager.update(millis());
        DisplayManager::update(millis());
        delay(10);
        return;
    }

    // 2. Power management update
    powerManager.update(millis());

    uint32_t hwNow = millis();

    // 3. 输入在深睡尝试之前：熄屏后若入睡失败，重试间隙内用户可按键亮屏并取消重试
    menuController.update();

    bool firstDeepSleep = powerManager.shouldEnterDeepSleep();
    bool retryDeepSleep = (powerManager.getState() == POWER_OFF &&
                           powerManager.isDeepSleepRetryDue(hwNow));
    if (firstDeepSleep || retryDeepSleep) {
        if (firstDeepSleep) {
            powerManager.clearSleepFlag();
        }
        powerManager.cancelDeepSleepRetry();
        enterDeepSleep();
        if (powerManager.getState() == POWER_OFF) {
            powerManager.scheduleDeepSleepRetry(millis());
        }
        return;
    }

    // 4. Time-based game logic
    uint32_t now = timeManager.now();

    // Idle tick: seriousness growth + rhongo timer
    if (timeManager.checkNewMinute()) {
        if (!pet.is_rhongomyniad && !pet.is_black_rhongomyniad) {
            Form formBeforeIdleTick = pet.form;
            IdleTickResult iR = seriousnessSystem.onIdleTick(pet, now);
            // 外层已排除白/黑狮终态，此处 is_rhongomyniad 为真即本分钟刚切入（等价 !wasRhongo && …）
            if (pet.is_rhongomyniad) {
                showWhiteRhongoJustEntered(formBeforeIdleTick, pet.seriousness);
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
        uint32_t ts = timeManager.now();
        SaveResult r = saveManager.save(pet, ts);
        if (r == SAVE_OK) {
            saveManager.markSaved(ts);
            // 同步设备时间 (与本次存档 save_time 同一采样)
            deviceState.device_clock_epoch = ts;
            persistDeviceState();
            DisplayManager::showAutoSave();
        } else {
            static uint32_t lastAutoSaveFailLogMs = 0;
            uint32_t ms = millis();
            if (ms - lastAutoSaveFailLogMs >= 3000) {
                lastAutoSaveFailLogMs = ms;
                Serial.printf("[Save] WARN: auto-save failed (%d)\n", (int)r);
            }
        }
    }

    // 5. Display update (state machine + render)
    DisplayManager::updatePetSnapshot(pet);
    DisplayManager::setDeviceInfo(deviceState.rounds, deviceState.is_visiting);
    DisplayManager::update(millis());

    delay(10);
}
