#pragma once
// Peripheral-only test adapter. Rules, menu, button state machine, calendar,
// display manager, serial renderer, assets and workflows are production sources.
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>
#define PROGMEM
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
extern uint32_t hostMillis;
extern int hostPins[64];
inline uint32_t millis(){return hostMillis;}
inline void pinMode(uint8_t,int){}
inline int digitalRead(uint8_t pin){return hostPins[pin];}
inline void* ps_malloc(size_t size){return std::malloc(size);}
inline uint16_t pgm_read_word(const void* p){return *static_cast<const uint16_t*>(p);}
inline uint8_t pgm_read_byte(const void* p){return *static_cast<const uint8_t*>(p);}
struct HostSerial {
    std::string output, input;
    void begin(int){}
    void flush(){}
    int available(){return int(input.size());}
    int read(){auto c=input.front();input.erase(0,1);return c;}
    void print(const char* text){output+=text;}
    void println(const char* text=""){output+=text;output+='\n';}
    void printf(const char* format,...){char buffer[512];va_list args;va_start(args,format);std::vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);output+=buffer;}
};
extern HostSerial Serial;

inline void delay(uint32_t ms){hostMillis+=ms;}
