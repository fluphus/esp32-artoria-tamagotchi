#pragma once
#include "esp_system.h"
#define RTC_DATA_ATTR
enum esp_sleep_wakeup_cause_t {ESP_SLEEP_WAKEUP_UNDEFINED,ESP_SLEEP_WAKEUP_EXT0,ESP_SLEEP_WAKEUP_EXT1,ESP_SLEEP_WAKEUP_TIMER,ESP_SLEEP_WAKEUP_TOUCHPAD,ESP_SLEEP_WAKEUP_ULP,ESP_SLEEP_WAKEUP_GPIO,ESP_SLEEP_WAKEUP_UART,ESP_SLEEP_WAKEUP_WIFI,ESP_SLEEP_WAKEUP_COCPU,ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG,ESP_SLEEP_WAKEUP_BT};
constexpr int ESP_PD_DOMAIN_RTC_PERIPH=0,ESP_PD_OPTION_ON=0,ESP_EXT1_WAKEUP_ANY_LOW=0;
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(){return ESP_SLEEP_WAKEUP_UNDEFINED;}
inline int esp_sleep_pd_config(int,int){return 0;}
inline int esp_sleep_enable_ext1_wakeup(uint64_t,int){return 0;}
inline void esp_deep_sleep_start(){}
