// src/pet/evolution.h

#ifndef EVOLUTION_H
#define EVOLUTION_H

#include <stdint.h>
#include "../core/game_state.h"
#include "seriousness.h"

enum EvolutionEvent : uint8_t {
    EVO_NONE = 0,
    EVO_CHILD_TO_WHITE,
    EVO_CHILD_TO_BLACK,
    EVO_FORM_CHANGED,
    EVO_RHONGOMYNIAD,
    EVO_BLACK_RHONGOMYNIAD,         // 新增: 麻婆豆腐诅咒
    EVO_NOBU_EVOLUTION,             // nobu -> Oda Nobunaga
    EVO_DESTROYED
};

inline const char* EVO_EVENT_NAMES[] = {
    "None",
    "Child -> White line",
    "Child -> Black line",
    "Form changed",
    "RHONGOMYNIAD (irreversible)",
    "BLACK RHONGOMYNIAD (curse)",   // 新增
    "nobu -> Oda Nobunaga",
    "Destroyed & Reset"
};

struct EvolutionResult {
    EvolutionEvent event;
    Form form_before;
    Form form_after;
    SeriousnessTier tier;
};

class EvolutionSystem {
public:
    EvolutionResult check(PetState& pet, uint32_t currentTime);
    EvolutionResult checkChildGraduation(PetState& pet);
    EvolutionResult checkMapoCurse(PetState& pet);     // 新增
    EvolutionResult checkNobuMapo(PetState& pet);
    void destroy(PetState& pet, uint32_t currentTime);
    void destroyToNobu(PetState& pet, uint32_t currentTime, uint16_t prevAgeDays);
    Form resolveAdultForm(PetState& pet);
    bool canInteract(const PetState& pet);

private:
    Form rollWhiteFunForm(PetState& pet);
};

extern EvolutionSystem evolutionSystem;

#endif // EVOLUTION_H
