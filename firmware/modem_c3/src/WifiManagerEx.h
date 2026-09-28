#pragma once
#include <stdint.h>
#include <vector>
struct WifiProfile { char ssid[33]; char password[65]; int8_t priority; };
class WifiManagerEx {
public:
    using OnStateChanged = void (*)(bool,uint32_t);
    bool init();
    bool connect();
    void loop();
    bool isConnected() const { return active_; } // AP radio running, not FC connectivity
    uint32_t getIP() const;
    const char* getCurrentSSID() const;
    int32_t getRSSI() const { return 0; }
    unsigned clients() const;
    bool dnsEnabled() const;
    uint32_t remainingMs() const;
    bool expired() const { return expired_; }
    const std::vector<WifiProfile>& getProfiles() const { return profiles_; }
    size_t getProfileCount() const { return 0; }
    int8_t getActiveProfile() const { return -1; }
    void setOnStateChanged(OnStateChanged cb) { callback_ = cb; }
private:
    std::vector<WifiProfile> profiles_;
    bool active_ = false, expired_ = false;
    uint32_t lastAttempt_ = 0, lastDnsAttempt_ = 0;
    OnStateChanged callback_ = nullptr;
};
extern WifiManagerEx g_wifiManager;
