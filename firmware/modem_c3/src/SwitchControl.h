#pragma once
#include <Arduino.h>
#include "Config.h"

class SwitchControl {
public:
    void begin() {
        // External gate/base pull-down is needed during reset, before setup().
        digitalWrite(PIN_SWITCH, offLevel());
        pinMode(PIN_SWITCH, OUTPUT);
        enabled_ = false;
        initialized_ = true;
    }
    void set(bool enabled) {
        if (!initialized_) return;
        digitalWrite(PIN_SWITCH, enabled ? onLevel() : offLevel());
        enabled_ = enabled;
    }
    bool enabled() const { return enabled_; }
    int level() const { return enabled_ ? onLevel() : offLevel(); }
    static bool parse(const char *value, bool &enabled) {
        if (!value || (value[0] != '0' && value[0] != '1') || value[1] != '\0') return false;
        enabled = value[0] == '1';
        return true;
    }
private:
    static int onLevel() { return SWITCH_ACTIVE_HIGH ? HIGH : LOW; }
    static int offLevel() { return SWITCH_ACTIVE_HIGH ? LOW : HIGH; }
    bool enabled_ = false;
    bool initialized_ = false;
};
extern SwitchControl g_switch;
