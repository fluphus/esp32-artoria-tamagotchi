#pragma once
#include <cstdint>
extern uint32_t hostRandom;
inline uint32_t esp_random(){return hostRandom;}
