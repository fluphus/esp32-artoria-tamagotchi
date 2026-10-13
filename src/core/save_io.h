#pragma once
#include <stddef.h>
#include <stdint.h>
// Only storage/logging needed by the original v9 save code. No game engine.
namespace saveio {
struct Storage {
    void* context=nullptr;
    bool (*open)(void*,const char*,bool)=nullptr;
    bool (*contains)(void*,const char*)=nullptr;
    size_t (*length)(void*,const char*)=nullptr;
    size_t (*read)(void*,const char*,void*,size_t)=nullptr;
    size_t (*write)(void*,const char*,const void*,size_t)=nullptr;
    bool (*eraseAll)(void*)=nullptr;
    bool begin(const char*n,bool ro){return open&&open(context,n,ro);}
    bool isKey(const char*k){return contains&&contains(context,k);}
    size_t getBytesLength(const char*k){return length?length(context,k):0;}
    size_t getBytes(const char*k,void*b,size_t n){return read?read(context,k,b,n):0;}
    size_t putBytes(const char*k,const void*b,size_t n){return write?write(context,k,b,n):0;}
    bool clear(){return eraseAll&&eraseAll(context);}
};
extern Storage storage;
extern void (*diagnostic)(const char*);
struct Log {void println(const char*) const;void printf(const char*,...) const;};
extern Log log;
}
