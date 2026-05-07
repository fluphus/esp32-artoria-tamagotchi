// src/core/game_state.h

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdint.h>

// --- 灵基形态 ---
enum Form : uint8_t {
    FORM_LILY = 0,
    FORM_WHITE_SABER,
    FORM_BLACK_SABER,
    FORM_WHITE_LANCER,
    FORM_BLACK_LANCER,
    FORM_WHITE_ARCHER,
    FORM_BLACK_RIDER,
    FORM_WHITE_RULER,
    FORM_WHITE_LANCER_RHONGOMYNIAD,
    FORM_BLACK_LANCER_RHONGOMYNIAD,     // 新增: 麻婆豆腐诅咒终态
    FORM_COUNT
};

inline const char* FORM_NAMES[FORM_COUNT] = {
    "Lily",
    "White Saber",
    "Black Saber",
    "White Lancer",
    "Black Lancer",
    "White Archer",
    "Black Rider",
    "White Ruler",
    "Rhongomyniad",
    "Black Rhongomyniad"                // 新增
};

// --- 阵营 ---
enum Alignment : uint8_t {
    ALIGN_UNDETERMINED = 0,
    ALIGN_WHITE,
    ALIGN_BLACK
};

inline const char* ALIGNMENT_NAMES[] = {
    "Undetermined",
    "White",
    "Black"
};

// --- 生命阶段 ---
enum LifeStage : uint8_t {
    STAGE_CHILD = 0,
    STAGE_ADULT
};

inline const char* STAGE_NAMES[] = {
    "Child",
    "Adult"
};

// --- 每日投喂状态 ---
struct DailyFeedState {
    uint8_t date_day;
    uint8_t feed_count;
    uint32_t last_feed_time;

    // 按投喂次数统计 (不是食物份数)
    // 每次投喂根据3份食物的多数属性判定
    uint8_t healthy_in_window;          // 正确窗口内 健康投喂次数
    uint8_t junk_in_window;             // 正确窗口内 垃圾投喂次数
    uint8_t healthy_outside_window;     // 窗口外 健康投喂次数
    uint8_t junk_outside_window;        // 窗口外 垃圾投喂次数

    void reset(uint8_t newDay) {
        date_day = newDay;
        feed_count = 0;
        last_feed_time = 0;
        healthy_in_window = 0;
        junk_in_window = 0;
        healthy_outside_window = 0;
        junk_outside_window = 0;
    }
};


// --- 宠物状态 ---
struct PetState {
    Form form;
    Form base_form;
    Alignment alignment;
    LifeStage stage;

    int16_t health;
    int16_t seriousness;
    uint16_t idle_minute_remainder;     // 严肃值增长计数器余数
    uint32_t idle_paused_until;         // 严肃值暂停增长截止时间戳, 0=未暂停
    uint32_t last_poke_effect_time;     // 上次poke生效(扣减严肃值)的时间戳, 0=从未生效

    uint32_t rhongo_timer_start;
    bool is_rhongomyniad;

    Form white_fun_form;
    bool white_fun_form_locked;

    // 麻婆豆腐诅咒
    uint8_t mapo_tofu_count;            // 当局累计进食次数
    bool is_black_rhongomyniad;         // 黑狮子王不可逆标记

    uint32_t birth_timestamp;
    uint32_t last_interact_time;
    uint16_t age_days;

    DailyFeedState daily_feed;

    void initNew(uint32_t now) {
        form = FORM_LILY;
        base_form = FORM_LILY;
        alignment = ALIGN_UNDETERMINED;
        stage = STAGE_CHILD;

        health = 50;
        seriousness = 0;
        idle_minute_remainder = 0;
        idle_paused_until = 0;
        last_poke_effect_time = 0;

        rhongo_timer_start = 0;
        is_rhongomyniad = false;

        white_fun_form = FORM_WHITE_SABER;
        white_fun_form_locked = false;

        mapo_tofu_count = 0;
        is_black_rhongomyniad = false;

        birth_timestamp = now;
        last_interact_time = now;
        age_days = 0;

        daily_feed.reset(0);
    }
};

// 存档头
struct SaveHeader {
    uint8_t version;
    uint16_t data_size;
    uint16_t checksum;
    uint32_t sequence;      // 单调递增序号, 用于判断新旧
    uint32_t save_time;     // 存档时的 unix timestamp (用于离线补偿)
};

#endif // GAME_STATE_H
