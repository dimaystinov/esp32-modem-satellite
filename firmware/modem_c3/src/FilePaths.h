#pragma once

/**
 * @file FilePaths.h
 * @brief Пути к файлам в LittleFS
 *
 * Все пути начинаются с / (корень LittleFS)
 */

namespace FilePaths {
    // Миссии
    constexpr const char* MISSION_CURRENT   = "/mission.waypoints";     // Текущая миссия
    constexpr const char* MISSION_TEMP      = "/mission.tmp";          // Временный файл миссии
    constexpr const char* MISSION_BACKUP    = "/mission.bak";          // Резервная копия

    // Конфигурации
    constexpr const char* WIFI_PROFILES     = "/wifi_profiles.json";  // WiFi профили
    constexpr const char* CONFIG_JSON       = "/config.json";          // Основной конфиг

    // Web UI
    constexpr const char* WEB_INDEX         = "/index.html";           // Главная страница
    constexpr const char* WEB_STYLE         = "/style.css";            // CSS стили
    constexpr const char* WEB_APP           = "/app.js";               // JavaScript

    // Логи
    constexpr const char* LOG_DIR           = "/logs";                 // Директория логов
    constexpr const char* LOG_SYSTEM        = "/logs/system.log";      // Системные логи
}

namespace PrefKeys {
    // Preferences ключи (NVS)
    constexpr const char* ECHO_ENABLED      = "echo_en";               // Эхо режим
    constexpr const char* REFRESH_SEC       = "refresh";               // Автообновление
    constexpr const char* ACTIVE_WIFI_PROF  = "wifi_prof";             // Активный WiFi профиль
    constexpr const char* MAV_TIMEOUT       = "mav_to";                // MAVLink таймаут
    constexpr const char* AUTO_UPLOAD       = "auto_up";               // Авто-загрузка миссии
    constexpr const char* DISPLAY_INTERVAL  = "disp_int";              // Интервал дисплея
    constexpr const char* LAST_MISSION_SIZE = "last_sz";               // Размер последней миссии
    constexpr const char* FIRST_BOOT        = "first_boot";            // Флаг первого запуска
}
