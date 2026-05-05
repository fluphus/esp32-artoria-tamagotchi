// src/config/food_table.h

#ifndef FOOD_TABLE_H
#define FOOD_TABLE_H

#include <stdint.h>

struct FoodItem {
    uint8_t id;
    const char* name;
    bool is_healthy;
    int8_t health_delta;
};

// --- 普通食物 ---
enum FoodId : uint8_t {
    FOOD_RICE_BALL = 0,
    FOOD_SALAD,
    FOOD_GRILLED_FISH,
    FOOD_BREAD,
    FOOD_CAKE,
    FOOD_FRIED_CHICKEN,
    FOOD_SODA,
    FOOD_CANDY,
    FOOD_COUNT
};

inline const FoodItem FOOD_TABLE[FOOD_COUNT] = {
    { FOOD_RICE_BALL,      "Rice Ball",      true,   +8  },
    { FOOD_SALAD,          "Salad",          true,   +5  },
    { FOOD_GRILLED_FISH,   "Grilled Fish",   true,   +10 },
    { FOOD_BREAD,          "Bread",          true,   +3  },
    { FOOD_CAKE,           "Cake",           false,  -5  },
    { FOOD_FRIED_CHICKEN,  "Fried Chicken",  false,  -8  },
    { FOOD_SODA,           "Soda",           false,  -3  },
    { FOOD_CANDY,          "Candy",          false,  -6  },
};

// --- 特殊食物 (连携奖励) ---
enum SpecialFoodId : uint8_t {
    SFOOD_GOLDEN_APPLE = 0,
    SFOOD_HOLY_GRAIL_MUG,
    SFOOD_EMIYA_COOKING,
    SFOOD_JAGUAR_SNACK,
    SFOOD_COUNT
};

struct SpecialFoodItem {
    uint8_t id;
    const char* name;
    const char* description;
};

inline const SpecialFoodItem SPECIAL_FOOD_TABLE[SFOOD_COUNT] = {
    { SFOOD_GOLDEN_APPLE,   "Golden Apple",     "A gift from Avalon"        },
    { SFOOD_HOLY_GRAIL_MUG, "Holy Grail Mug",   "Warm cocoa in a grail"     },
    { SFOOD_EMIYA_COOKING,  "Emiya Cooking",     "Archer's home cooking"     },
    { SFOOD_JAGUAR_SNACK,   "Jaguar Snack",      "Taiga's mystery snack"     },
};

// --- 麻婆豆腐 (彩蛋) ---
struct CurseFoodItem {
    const char* name;
    const char* description;
};

inline const CurseFoodItem MAPO_TOFU = {
    "Mapo Tofu",
    "Yorokobe, shounen."
};

#endif // FOOD_TABLE_H
