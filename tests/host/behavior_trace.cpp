#include "pet/evolution.h"
#include "pet/feeding.h"
#include "pet/seriousness.h"
#include <iostream>
static uint32_t randomValue = 0, randomCalls = 0;
uint32_t esp_random() {
    ++randomCalls;
    return randomValue;
}
void emit(const char *label, const PetState &p) {
    std::cout << label << ',' << int(p.form) << ',' << int(p.base_form) << ',' << int(p.alignment)
              << ',' << int(p.stage) << ',' << p.health << ',' << p.seriousness << ','
              << p.idle_minute_remainder << ',' << p.idle_paused_until << ','
              << p.last_poke_effect_time << ',' << p.rhongo_timer_start << ',' << p.is_rhongomyniad
              << ',' << int(p.white_fun_form) << ',' << p.white_fun_form_locked << ','
              << int(p.mapo_tofu_count) << ',' << p.is_black_rhongomyniad << ',' << p.is_nobu << ','
              << p.is_oda_nobunaga << ',' << p.birth_timestamp << ',' << p.last_interact_time << ','
              << p.age_days << ',' << int(p.daily_feed.date_day) << ','
              << int(p.daily_feed.feed_count) << ',' << p.daily_feed.last_feed_time << ','
              << int(p.daily_feed.healthy_in_window) << ',' << int(p.daily_feed.junk_in_window)
              << ',' << int(p.daily_feed.healthy_outside_window) << ','
              << int(p.daily_feed.junk_outside_window) << ',' << randomCalls << '\n';
}
int main() {
    constexpr uint32_t T = 1700000000;
    PetState p{};
    p.initNew(T);
    p.seriousness = 50;
    emit("init", p);
    auto draw = feedingSystem.drawFood();
    std::cout << "draw";
    for (auto f : draw.food_ids)
        std::cout << ',' << int(f);
    std::cout << '\n';
    uint8_t healthy[] = {0, 1, 3}, junk[] = {4, 5, 6};
    for (int i = 0; i < 4; ++i) {
        auto r = feedingSystem.feed(p, healthy, T + i * 1800, 8);
        std::cout << "feed-result," << int(r.result) << ',' << r.health_from_food << ','
                  << r.health_from_combo << ',' << r.seriousness_from_dislike << ','
                  << r.seriousness_from_combo << '\n';
        if (r.result == FEED_OK)
            seriousnessSystem.onInteract(p, INTERACT_FEED, T + i * 1800);
        emit("child-feed", p);
    }
    feedingSystem.resetDaily(p, 14);
    p.age_days = 3;
    evolutionSystem.checkChildGraduation(p);
    emit("graduated", p);
    p.seriousness = 50;
    feedingSystem.feed(p, junk, T + 10000, 20);
    seriousnessSystem.onInteract(p, INTERACT_FEED, T + 10000);
    emit("adult-junk", p);
    auto day = feedingSystem.processDayEnd(p);
    if (day.missed_feed_penalty)
        seriousnessSystem.applyMissedFeedPenalty(p);
    emit("day-effects", p);
    seriousnessSystem.onInteract(p, INTERACT_POKE, T + 12000);
    emit("poke", p);
    seriousnessSystem.onInteract(p, INTERACT_POKE, T + 13799);
    emit("poke-cooling", p);
    seriousnessSystem.onInteract(p, INTERACT_POKE, T + 13800);
    emit("poke-boundary", p);
    seriousnessSystem.onIdleBatch(p, 60, T + 17400);
    emit("advance", p);
    for (int sr : {0, 32, 33, 65, 66, 99, 100}) {
        p.seriousness = sr;
        evolutionSystem.check(p, T + 20000);
        emit("tier", p);
    }
    p.seriousness = 100;
    p.rhongo_timer_start = T;
    seriousnessSystem.onIdleTick(p, T + 172799);
    emit("rhongo-before", p);
    seriousnessSystem.onIdleTick(p, T + 172800);
    emit("rhongo-at", p);
    evolutionSystem.destroy(p, T + 200000);
    p.stage = STAGE_ADULT;
    p.alignment = ALIGN_BLACK;
    p.seriousness = 50;
    feedingSystem.feed(p, healthy, T + 200001, 12);
    seriousnessSystem.onInteract(p, INTERACT_FEED, T + 200001);
    emit("black-healthy", p);
    FeedOutcome special{};
    special.combo_triggered = true;
    for (int i = 0; i < 3; ++i) {
        feedingSystem.applySpecialFood(p, special, 0);
        evolutionSystem.checkMapoCurse(p);
        emit("mapo", p);
    }
    evolutionSystem.destroyToNobu(p, T + 300000, 5);
    emit("nobu", p);
    special = {};
    special.combo_triggered = true;
    feedingSystem.applySpecialFood(p, special, 0);
    evolutionSystem.checkNobuMapo(p);
    emit("oda", p);
    feedingSystem.applySpecialFood(p, special, 0);
    emit("oda-locked", p);
    // Stable white entertainment form consumes RNG once and remains locked.
    evolutionSystem.destroy(p, T + 400000);
    p.stage = STAGE_ADULT;
    p.alignment = ALIGN_WHITE;
    p.seriousness = 0;
    randomValue = 1;
    evolutionSystem.check(p, T + 400000);
    emit("white-fun", p);
    randomValue = 0;
    evolutionSystem.check(p, T + 400001);
    emit("white-fun-locked", p);
}
