#pragma once
#include "Arduino.h"
#define SERIAL_8N1 0
class HardwareSerial;
inline HardwareSerial* testMavPort = nullptr;
class HardwareSerial : public Stream {
public:
    explicit HardwareSerial(int) { testMavPort = this; }
    void setRxBufferSize(size_t) {}
    void begin(uint32_t, int, uint8_t, uint8_t) {}
    void end() {}
};
inline void delay(uint32_t ms) { testMillis += ms; }
