#pragma once
using gpio_num_t=int;
inline bool rtc_gpio_is_valid_gpio(int){return true;}
inline void rtc_gpio_hold_dis(int){}
inline void rtc_gpio_hold_en(int){}
inline void rtc_gpio_pullup_en(int){}
inline void rtc_gpio_pulldown_dis(int){}
