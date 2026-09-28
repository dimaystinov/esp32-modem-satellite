#pragma once

/**
 * @file WebUi.h
 * @brief HTTP веб-сервер и API
 *
 * Реализует:
 * - GET / - HTML dashboard
 * - GET /api/status - JSON статус
 * - POST /api/echo?set=1/0 - включить/выключить echo
 * - POST /api/logs/clear - очистить логи
 * - POST /api/upload/start - начать загрузку
 * - POST /api/upload/stop - остановить загрузку
 * - GET /api/mission - метаданные миссии
 * - GET /api/mission/raw - сырой текст миссии
 */

#include <cstdint>
#include <cstddef>

class WebUi {
public:
    /**
     * @brief Callback для отправки логов
     */
    using OnLogRequest = void (*)();

    /**
     * @brief Инициализация веб-сервера
     * @param port Порт (default 80)
     * @return true при успехе
     */
    bool init(uint16_t port = 80);

    /**
     * @brief Деинициализация
     */
    void deinit();

    /**
     * @brief Цикл обработки (вызывать в loop)
     */
    void loop();

    /**
     * @brief Установить callback для логов
     */
    void setOnLogRequest(OnLogRequest cb) { onLogRequest_ = cb; }

private:
    void* server_ = nullptr;  // WebServer*
    uint16_t port_ = 80;
    OnLogRequest onLogRequest_;

    // Route handlers
    void handleRoot();
    void handleStatus();
    void handleEcho();
    void handleSwitch();
    void handleLogs();
    void handleLogsClear();
    void handleUploadStart();
    void handleUploadStop();
    void handleMission();
    void handleMissionClear();
    void handleMissionRaw();
    void handleWifiProfiles();
    void handleConfig();
    void handleNotFound();
    void handleStaticFile(const char* path);

    // Helpers
    void sendJson(const char* json);
    void sendError(const char* msg, int code = 400);
    void sendOk();
    const char* getContentType(const char* filename);
};

// Глобальный экземпляр
extern WebUi g_webUi;
