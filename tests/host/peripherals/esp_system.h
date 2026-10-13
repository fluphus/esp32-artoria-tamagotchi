#pragma once
// Host declarations only; ESP sleep/reset are never exercised as hardware tests.
#include <cstdint>
using esp_err_t=int;constexpr int ESP_OK=0;
enum esp_reset_reason_t {ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,ESP_RST_BROWNOUT,ESP_RST_SDIO};
inline esp_reset_reason_t esp_reset_reason(){return ESP_RST_POWERON;}
inline const char* esp_err_to_name(int){return "HOST";}
