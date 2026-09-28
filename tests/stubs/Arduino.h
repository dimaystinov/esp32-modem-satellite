#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <vector>
#include <utility>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
inline std::vector<std::pair<int,int>> writes;
inline bool outputConfigured = false;
inline void digitalWrite(int pin, int value) { writes.emplace_back(pin,value); }
inline void pinMode(int pin, int mode) { outputConfigured = pin==4 && mode==OUTPUT && !writes.empty() && writes.back().second==LOW; }
#include <cstdarg>
#include <deque>
inline uint32_t testMillis=0;
inline uint32_t millis(){ return testMillis; }
class Stream {
public:
    std::deque<uint8_t> rx;
    std::vector<uint8_t> tx;
    int available(){return rx.size();}
    int read(){if(rx.empty())return -1;auto b=rx.front();rx.pop_front();return b;}
    size_t write(const uint8_t* p,size_t n){tx.insert(tx.end(),p,p+n);return n;}
};
inline Stream Serial;
