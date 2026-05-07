// src/pet/evolution.cpp

#include "evolution.h"
#include "../config/game_config.h"
#include <Arduino.h>
#include <esp_random.h>

EvolutionSystem evolutionSystem;

Form EvolutionSystem::rollWhiteFunForm(PetState& pet) {
    if (pet.white_fun_form_locked) return pet.white_fun_form;
    pet.white_fun_form = (esp_random() % 2 == 0) ? FORM_WHITE_ARCHER : FORM_WHITE_RULER;
    pet.white_fun_form_locked = true;
    Serial.printf("[Evolution] White fun form: %s (locked)\n", FORM_NAMES[pet.white_fun_form]);
    return pet.white_fun_form;
}

Form EvolutionSystem::resolveAdultForm(PetState& pet) {
    SeriousnessTier tier = seriousnessSystem.getTier(pet.seriousness);
    if (pet.alignment == ALIGN_WHITE) {
        switch (tier) {
            case TIER_LOW:  return rollWhiteFunForm(pet);
            case TIER_MID:  return FORM_WHITE_SABER;
            case TIER_HIGH:
            case TIER_MAX:  return FORM_WHITE_LANCER;
        }
    } else if (pet.alignment == ALIGN_BLACK) {
        switch (tier) {
            case TIER_LOW:  return FORM_BLACK_RIDER;
            case TIER_MID:  return FORM_BLACK_SABER;
            case TIER_HIGH:
            case TIER_MAX:  return FORM_BLACK_LANCER;
        }
    }
    return pet.form;
}

bool EvolutionSystem::canInteract(const PetState& pet) {
    return !pet.is_rhongomyniad && !pet.is_black_rhongomyniad;
}

EvolutionResult EvolutionSystem::checkChildGraduation(PetState& pet) {
    EvolutionResult result = {};
    result.event = EVO_NONE;
    result.form_before = pet.form;
    result.form_after = pet.form;

    if (pet.stage != STAGE_CHILD) return result;
    if (pet.age_days < CHILD_PERIOD_DAYS) return result;

    // nobu 不会自然成长
    if (pet.is_nobu) return result;

    pet.stage = STAGE_ADULT;

    if (pet.health >= HEALTH_INITIAL) {
        pet.alignment = ALIGN_WHITE;
        pet.base_form = FORM_WHITE_SABER;
        pet.form = FORM_WHITE_SABER;
        result.event = EVO_CHILD_TO_WHITE;
    } else {
        pet.alignment = ALIGN_BLACK;
        pet.base_form = FORM_BLACK_SABER;
        pet.form = FORM_BLACK_SABER;
        result.event = EVO_CHILD_TO_BLACK;
    }

    result.form_after = pet.form;
    result.tier = seriousnessSystem.getTier(pet.seriousness);

    Serial.println("[Evolution] === CHILD PERIOD OVER ===");
    Serial.printf("[Evolution] Health %d %s %d -> %s line\n",
                  pet.health,
                  pet.health >= HEALTH_INITIAL ? ">=" : "<",
                  HEALTH_INITIAL,
                  ALIGNMENT_NAMES[pet.alignment]);
    Serial.printf("[Evolution] %s -> %s\n",
                  FORM_NAMES[result.form_before],
                  FORM_NAMES[result.form_after]);

    Form resolved = resolveAdultForm(pet);
    if (resolved != pet.form) {
        pet.form = resolved;
        result.form_after = resolved;
        Serial.printf("[Evolution] Immediate tier resolve: -> %s\n",
                      FORM_NAMES[resolved]);
    }

    return result;
}

EvolutionResult EvolutionSystem::checkMapoCurse(PetState& pet) {
    EvolutionResult result = {};
    result.event = EVO_NONE;
    result.form_before = pet.form;
    result.form_after = pet.form;

    if (pet.is_rhongomyniad || pet.is_black_rhongomyniad) return result;

    if (pet.mapo_tofu_count >= MAPO_TOFU_CURSE_THRESHOLD) {
        pet.is_black_rhongomyniad = true;
        pet.form = FORM_BLACK_LANCER_RHONGOMYNIAD;
        result.event = EVO_BLACK_RHONGOMYNIAD;
        result.form_after = FORM_BLACK_LANCER_RHONGOMYNIAD;

        Serial.println("[Evolution] ================================");
        Serial.println("[Evolution] *** BLACK RHONGOMYNIAD ***");
        Serial.println("[Evolution] The curse of Mapo Tofu is complete.");
        Serial.println("[Evolution] Yorokobe, shounen.");
        Serial.println("[Evolution] Irreversible. No more interactions.");
        Serial.println("[Evolution] ================================");
    }

    return result;
}

EvolutionResult EvolutionSystem::check(PetState& pet, uint32_t currentTime) {
    EvolutionResult result = {};
    result.event = EVO_NONE;
    result.form_before = pet.form;
    result.form_after = pet.form;
    result.tier = seriousnessSystem.getTier(pet.seriousness);

    // nobu 路线: 不进行正常进化检查
    if (pet.is_nobu || pet.is_oda_nobunaga) {
        return result;
    }

        // 终态检查
    if (pet.is_rhongomyniad) {
        result.form_after = FORM_WHITE_LANCER_RHONGOMYNIAD;
        return result;
    }
    if (pet.is_black_rhongomyniad) {
        result.form_after = FORM_BLACK_LANCER_RHONGOMYNIAD;
        return result;
    }

    // 麻婆豆腐诅咒优先检查
    EvolutionResult mapoResult = checkMapoCurse(pet);
    if (mapoResult.event != EVO_NONE) return mapoResult;

    // 幼年期不检查成体形态
    if (pet.stage != STAGE_ADULT) return result;

    // 白线狮子王检查
    if (pet.alignment == ALIGN_WHITE && pet.seriousness >= RHONGOMYNIAD_THRESHOLD) {
        if (pet.rhongo_timer_start > 0) {
            uint32_t elapsed = currentTime - pet.rhongo_timer_start;
            if (elapsed >= RHONGOMYNIAD_SUSTAIN_SEC) {
                pet.is_rhongomyniad = true;
                pet.form = FORM_WHITE_LANCER_RHONGOMYNIAD;
                result.event = EVO_RHONGOMYNIAD;
                result.form_after = FORM_WHITE_LANCER_RHONGOMYNIAD;

                Serial.println("[Evolution] *** RHONGOMYNIAD ***");
                Serial.println("[Evolution] Irreversible.");
                return result;
            }
        }
    }

    // 严肃值区间形态切换
    Form resolved = resolveAdultForm(pet);
    if (resolved != pet.form) {
        Form oldForm = pet.form;
        pet.form = resolved;
        result.event = EVO_FORM_CHANGED;
        result.form_after = resolved;

        Serial.printf("[Evolution] Form: %s -> %s (SR=%d, %s)\n",
                      FORM_NAMES[oldForm], FORM_NAMES[resolved],
                      pet.seriousness, TIER_NAMES[result.tier]);
    }

    return result;
}

void EvolutionSystem::destroy(PetState& pet, uint32_t currentTime) {
    Serial.println("[Evolution] *** DESTROY ***");
    Serial.printf("[Evolution] Destroying %s...\n", FORM_NAMES[pet.form]);
    pet.initNew(currentTime);
    Serial.println("[Evolution] Reset to Lily. New game.");
}


EvolutionResult EvolutionSystem::checkNobuMapo(PetState& pet) {
    EvolutionResult result = {};
    result.event = EVO_NONE;
    result.form_before = pet.form;
    result.form_after = pet.form;

    if (!pet.is_nobu) return result;
    if (pet.is_oda_nobunaga) return result;

    // nobu 的麻婆豆腐事件触发进化为 Oda Nobunaga
    if (pet.mapo_tofu_count >= 1) {
        pet.is_nobu = false;
        pet.is_oda_nobunaga = true;
        pet.form = FORM_ODA_NOBUNAGA;
        pet.stage = STAGE_ADULT;
        pet.mapo_tofu_count = 1;  // 锁定为 1, 防止触发黑狮子王
        result.event = EVO_NOBU_EVOLUTION;
        result.form_after = FORM_ODA_NOBUNAGA;

        Serial.println("[Evolution] ================================");
        Serial.println("[Evolution] *** ODA NOBUNAGA ***");
        Serial.println("[Evolution] nobu has evolved!");
        Serial.println("[Evolution] Form locked. Mapo count locked at 1.");
        Serial.println("[Evolution] ================================");
    }

    return result;
}

void EvolutionSystem::destroyToNobu(PetState& pet, uint32_t currentTime, uint16_t prevAgeDays) {
    Serial.println("[Evolution] *** NOBU EASTER EGG ***");
    pet.initNew(currentTime);
    pet.form = FORM_NOBU;
    pet.base_form = FORM_NOBU;
    pet.is_nobu = true;
    pet.stage = STAGE_CHILD;
    Serial.printf("[Evolution] Reset to nobu (prev age: %d days)\n", prevAgeDays);
}