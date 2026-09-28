#include "WifiManagerEx.h"
#include "Config.h"
#include "AppState.h"
#include "Logger.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
WifiManagerEx g_wifiManager;
static DNSServer dns;
bool WifiManagerEx::init() { WiFi.persistent(false); return connect(); }
bool WifiManagerEx::connect() {
    if (expired_ || !remainingMs()) return false;
    if (active_) return true;
    lastAttempt_ = millis();
    const IPAddress ip(192,168,0,4);
    WiFi.mode(WIFI_AP);
    active_ = WiFi.softAPConfig(ip,ip,IPAddress(255,255,255,0),IPAddress(192,168,0,10),ip) && WiFi.softAP(AP_SSID,AP_PASSWORD);
    if (active_) {
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        dns.setTTL(60); dns.start(53,LOCAL_DNS_NAME,ip);
        MDNS.begin("modem-bridge"); MDNS.addService("http","tcp",HTTP_PORT);
        g_appState.setWiFiState(true, uint32_t(ip));
        if(callback_) callback_(true,uint32_t(ip));
        g_logger.appendStatus("AP modem_bridge ready: http://192.168.0.4");
    }
    return active_;
}
void WifiManagerEx::loop() {
    if (expired_) return;
    if (!remainingMs()) {
        expired_=true; dns.stop(); MDNS.end(); WiFi.mode(WIFI_OFF); active_=false;
        g_appState.setWiFiState(false);
        if(callback_) callback_(false,0);
        g_logger.appendStatus("Wi-Fi OFF after 300s; power and mission processing continue");
        return;
    }
    if (!active_ && uint32_t(millis()-lastAttempt_)>=5000) connect();
    if (active_ && !dns.isUp() && uint32_t(millis()-lastDnsAttempt_)>=5000) {
        lastDnsAttempt_=millis(); dns.start(53,LOCAL_DNS_NAME,WiFi.softAPIP());
    }
}
uint32_t WifiManagerEx::getIP() const { return active_ ? uint32_t(WiFi.softAPIP()) : 0; }
const char* WifiManagerEx::getCurrentSSID() const { return AP_SSID; }
unsigned WifiManagerEx::clients() const { return active_ ? WiFi.softAPgetStationNum() : 0; }
bool WifiManagerEx::dnsEnabled() const { return active_ && dns.isUp(); }
uint32_t WifiManagerEx::remainingMs() const {
    const uint32_t now=millis();
    return expired_ || now>=WIFI_AUTO_OFF_MS ? 0 : WIFI_AUTO_OFF_MS-now;
}
