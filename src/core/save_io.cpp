#include "save_io.h"
#include <stdio.h>
#include <stdarg.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <Preferences.h>
namespace {Preferences preferences;}
#endif
namespace saveio {
#ifdef ARDUINO
Storage storage{&preferences,
 [](void*p,const char*n,bool ro){return static_cast<Preferences*>(p)->begin(n,ro);},
 [](void*p,const char*k){return static_cast<Preferences*>(p)->isKey(k);},
 [](void*p,const char*k){return static_cast<Preferences*>(p)->getBytesLength(k);},
 [](void*p,const char*k,void*b,size_t n){return static_cast<Preferences*>(p)->getBytes(k,b,n);},
 [](void*p,const char*k,const void*b,size_t n){return static_cast<Preferences*>(p)->putBytes(k,b,n);},
 [](void*p){return static_cast<Preferences*>(p)->clear();}};
void (*diagnostic)(const char*)=[](const char*s){Serial.print(s);};
#else
Storage storage;
void (*diagnostic)(const char*)=nullptr;
#endif
Log log;
void Log::println(const char*s) const {if(diagnostic){diagnostic(s);diagnostic("\n");}}
void Log::printf(const char*fmt,...) const {
    char buffer[256];va_list args;va_start(args,fmt);
    vsnprintf(buffer,sizeof(buffer),fmt,args);va_end(args);
    if(diagnostic)diagnostic(buffer);
}
}
