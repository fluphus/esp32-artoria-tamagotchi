// src/config/game_config.h

#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// --- 幼年期 ---
#define CHILD_PERIOD_DAYS           3
#define HEALTH_INITIAL              50
#define HEALTH_MAX                  100
#define HEALTH_MIN                  0

// --- 投喂 ---
#define DAILY_FEED_LIMIT            3
#define FEED_INTERVAL_MIN_SEC       1800
#define FEED_DRAW_COUNT             4
#define FEED_PICK_COUNT             3

#define FEED_WINDOW_BREAKFAST_START 8
#define FEED_WINDOW_BREAKFAST_END   9
#define FEED_WINDOW_LUNCH_START     12
#define FEED_WINDOW_LUNCH_END       13
#define FEED_WINDOW_DINNER_START    18
#define FEED_WINDOW_DINNER_END      19

#define CORRECT_WINDOW_BONUS        5
#define WRONG_WINDOW_PENALTY        5
#define MISSED_FEED_SERIOUSNESS     10

// --- 连携 ---
#define COMBO_HEALTH_BONUS          15
#define COMBO_SERIOUSNESS_DELTA     25

// --- 食物喜好 ---
#define DISLIKE_FOOD_SERIOUSNESS    3
#define CHILD_LIKED_FOOD_SR_DELTA   (-5)    // 幼体组吃到喜欢的食物 SR 变化 (负数=减少)

// --- 麻婆豆腐 ---
#define MAPO_TOFU_CHANCE_PERCENT    10      // 彩蛋触发概率
#define MAPO_TOFU_CURSE_THRESHOLD   3       // 累计N次触发黑狮子王

// --- 严肃值 ---
#define SERIOUSNESS_MAX             100
#define SERIOUSNESS_MIN             0
#define SERIOUSNESS_TIER_LOW_UPPER  32
#define SERIOUSNESS_TIER_MID_UPPER  65

#define SERIOUSNESS_IDLE_INTERVAL_MIN  5    // 每5分钟增长一次
#define SERIOUSNESS_IDLE_PER_TICK      1    // 每次增长1点
#define SERIOUSNESS_INTERACT_DELTA  15

#define POKE_SERIOUSNESS_DELTA      15      // 每次poke减少的严肃值
#define POKE_COOLDOWN_SEC           1800    // poke减少严肃值的冷却时间(秒), 30分钟
#define POKE_IDLE_PAUSE_SEC         1800    // 戳一戳暂停严肃值增长30分钟

// --- HP 分级阈值 (供演出层待机动画判定) ---
#define HP_TIER_CRITICAL_MAX        15      // < 15 = 濒危
#define HP_TIER_LOW_MAX             35      // < 35 = 低
#define HP_TIER_MID_MAX             65      // < 65 = 中, >= 65 = 高

// --- 狮子王 ---
#define RHONGOMYNIAD_THRESHOLD      100
#define RHONGOMYNIAD_SUSTAIN_SEC    172800
#define RHONGOMYNIAD_SAFE_DROP      80
#define LION_KING_SR_CRITICAL_MIN   80      // 狮子王计时中 SR >= 80 的特殊待机阈值

// --- 存档 ---
#define SAVE_NVS_NAMESPACE          "fate_tama"
#define SAVE_DATA_VERSION           9       // 版本号升级 (rounds 移入 DeviceState, 串门机制)

// --- Nobu 彩蛋触发概率 (‰, 千分比) ---
#define NOBU_BASE_PERMILLE          10      // 基础触发概率 10/1000 = 1% 某六字紧张刺激的指令卡战斗神秘游戏的神奇妙妙概率
#define NOBU_DAY6_PERMILLE          250      // 第6天(仅当天)触发概率 250/1000 = 25%

// --- 销毁 ---
#define DESTROY_COMBO_HOLD_MS       3000

// --- 时间模拟 ---
#define TIME_SIMULATED              1
#define SIM_MINUTES_PER_REAL_SEC    0

#endif // GAME_CONFIG_H
