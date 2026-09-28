/**
 * @file SettingsStore.cpp
 * @brief Реализация хранения настроек в Preferences
 */

#include "SettingsStore.h"
#include <Preferences.h>

// Глобальный экземпляр
SettingsStore g_settings;

// ============================================================================
// Инициализация
// ============================================================================

bool SettingsStore::init() {
    if (initialized_) return true;

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) {
        return false;
    }

    initialized_ = true;
    prefs.end();
    return true;
}

void SettingsStore::deinit() {
    initialized_ = false;
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

Preferences SettingsStore::openPrefs(bool readOnly) const {
    Preferences prefs;
    prefs.begin(NAMESPACE, readOnly);
    return prefs;
}

// ============================================================================
// Echo режим
// ============================================================================

bool SettingsStore::getEchoEnabled() const {
    if (!initialized_) return DEFAULT_ECHO;
    Preferences prefs = openPrefs(true);
    return prefs.getBool(KEY_ECHO, DEFAULT_ECHO);
}

void SettingsStore::setEchoEnabled(bool enabled) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putBool(KEY_ECHO, enabled);
    prefs.end();
}

// ============================================================================
// Интервал автообновления
// ============================================================================

uint32_t SettingsStore::getRefreshSec() const {
    if (!initialized_) return DEFAULT_REFRESH;
    Preferences prefs = openPrefs(true);
    return prefs.getUInt(KEY_REFRESH, DEFAULT_REFRESH);
}

void SettingsStore::setRefreshSec(uint32_t sec) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putUInt(KEY_REFRESH, sec);
    prefs.end();
}

// ============================================================================
// Активный профиль WiFi
// ============================================================================

int8_t SettingsStore::getActiveWifiProfile() const {
    if (!initialized_) return DEFAULT_WIFI_PROF;
    Preferences prefs = openPrefs(true);
    return prefs.getChar(KEY_WIFI_PROF, DEFAULT_WIFI_PROF);
}

void SettingsStore::setActiveWifiProfile(int8_t profile) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putChar(KEY_WIFI_PROF, profile);
    prefs.end();
}

// ============================================================================
// MAVLink таймаут
// ============================================================================

uint32_t SettingsStore::getMavlinkTimeout() const {
    if (!initialized_) return DEFAULT_MAV_TO;
    Preferences prefs = openPrefs(true);
    return prefs.getUInt(KEY_MAV_TO, DEFAULT_MAV_TO);
}

void SettingsStore::setMavlinkTimeout(uint32_t timeout) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putUInt(KEY_MAV_TO, timeout);
    prefs.end();
}

// ============================================================================
// Авто-загрузка
// ============================================================================

bool SettingsStore::getAutoUpload() const {
    if (!initialized_) return DEFAULT_AUTO_UP;
    Preferences prefs = openPrefs(true);
    return prefs.getBool(KEY_AUTO_UP, DEFAULT_AUTO_UP);
}

void SettingsStore::setAutoUpload(bool enabled) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putBool(KEY_AUTO_UP, enabled);
    prefs.end();
}

// ============================================================================
// Интервал дисплея
// ============================================================================

uint32_t SettingsStore::getDisplayInterval() const {
    if (!initialized_) return DEFAULT_DISP_INT;
    Preferences prefs = openPrefs(true);
    return prefs.getUInt(KEY_DISP_INT, DEFAULT_DISP_INT);
}

void SettingsStore::setDisplayInterval(uint32_t interval) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putUInt(KEY_DISP_INT, interval);
    prefs.end();
}

// ============================================================================
// Размер последней миссии
// ============================================================================

uint32_t SettingsStore::getLastMissionSize() const {
    if (!initialized_) return DEFAULT_LAST_SIZE;
    Preferences prefs = openPrefs(true);
    return prefs.getUInt(KEY_LAST_SIZE, DEFAULT_LAST_SIZE);
}

void SettingsStore::setLastMissionSize(uint32_t size) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putUInt(KEY_LAST_SIZE, size);
    prefs.end();
}

// ============================================================================
// Флаг первого запуска
// ============================================================================

bool SettingsStore::isFirstBoot() const {
    if (!initialized_) return DEFAULT_FIRST_BOOT;
    Preferences prefs = openPrefs(true);
    return prefs.getBool(KEY_FIRST_BOOT, DEFAULT_FIRST_BOOT);
}

void SettingsStore::setFirstBoot(bool first) {
    if (!initialized_) return;
    Preferences prefs = openPrefs(false);
    prefs.putBool(KEY_FIRST_BOOT, first);
    prefs.end();
}

// ============================================================================
// Сброс и очистка
// ============================================================================

void SettingsStore::resetToDefaults() {
    if (!initialized_) return;

    Preferences prefs = openPrefs(false);
    prefs.putBool(KEY_ECHO, DEFAULT_ECHO);
    prefs.putUInt(KEY_REFRESH, DEFAULT_REFRESH);
    prefs.putChar(KEY_WIFI_PROF, DEFAULT_WIFI_PROF);
    prefs.putUInt(KEY_MAV_TO, DEFAULT_MAV_TO);
    prefs.putBool(KEY_AUTO_UP, DEFAULT_AUTO_UP);
    prefs.putUInt(KEY_DISP_INT, DEFAULT_DISP_INT);
    prefs.putUInt(KEY_LAST_SIZE, DEFAULT_LAST_SIZE);
    prefs.putBool(KEY_FIRST_BOOT, false);
    prefs.end();
}

void SettingsStore::clearAll() {
    if (!initialized_) return;

    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.clear();
    prefs.end();
}
