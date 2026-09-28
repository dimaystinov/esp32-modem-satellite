#pragma once

/**
 * @file SettingsStore.h
 * @brief Хранение настроек в Preferences (NVS)
 *
 * Использует ESP32 Preferences для хранения мелких настроек:
 * - echo режим
 * - интервал обновления
 * - активный WiFi профиль
 * - MAVLink таймаут
 * - авто-загрузка миссии
 */

#include <cstdint>
#include <cstring>
#include <Preferences.h>

class SettingsStore {
public:
    /**
     * @brief Инициализация Preferences
     * @return true при успехе
     */
    bool init();

    /**
     * @brief Деинициализация
     */
    void deinit();

    // Echo режим
    bool getEchoEnabled() const;
    void setEchoEnabled(bool enabled);

    // Интервал автообновления страницы (сек)
    uint32_t getRefreshSec() const;
    void setRefreshSec(uint32_t sec);

    // Активный профиль WiFi
    int8_t getActiveWifiProfile() const;
    void setActiveWifiProfile(int8_t profile);

    // MAVLink таймаут (мс)
    uint32_t getMavlinkTimeout() const;
    void setMavlinkTimeout(uint32_t timeout);

    // Авто-загрузка миссии после приема
    bool getAutoUpload() const;
    void setAutoUpload(bool enabled);

    // Интервал обновления дисплея (сек)
    uint32_t getDisplayInterval() const;
    void setDisplayInterval(uint32_t interval);

    // Размер последней миссии
    uint32_t getLastMissionSize() const;
    void setLastMissionSize(uint32_t size);

    // Флаг первого запуска
    bool isFirstBoot() const;
    void setFirstBoot(bool first);

    // Сброс к значениям по умолчанию
    void resetToDefaults();

    // Очистка всех настроек
    void clearAll();

private:
    bool initialized_ = false;

    // Вспомогательный метод для открытия Preferences
    Preferences openPrefs(bool readOnly) const;

    // Ключи Preferences
    static constexpr const char* NAMESPACE = "mavlink";
    static constexpr const char* KEY_ECHO = "echo";
    static constexpr const char* KEY_REFRESH = "refresh";
    static constexpr const char* KEY_WIFI_PROF = "wifi_prof";
    static constexpr const char* KEY_MAV_TO = "mav_to";
    static constexpr const char* KEY_AUTO_UP = "auto_up";
    static constexpr const char* KEY_DISP_INT = "disp_int";
    static constexpr const char* KEY_LAST_SIZE = "last_sz";
    static constexpr const char* KEY_FIRST_BOOT = "first_boot";

    // Значения по умолчанию
    static constexpr bool DEFAULT_ECHO = false;
    static constexpr uint32_t DEFAULT_REFRESH = 20;
    static constexpr int8_t DEFAULT_WIFI_PROF = -1;
    static constexpr uint32_t DEFAULT_MAV_TO = 5000;
    static constexpr bool DEFAULT_AUTO_UP = false;
    static constexpr uint32_t DEFAULT_DISP_INT = 10;
    static constexpr uint32_t DEFAULT_LAST_SIZE = 0;
    static constexpr bool DEFAULT_FIRST_BOOT = true;
};

// Глобальный экземпляр
extern SettingsStore g_settings;
