#pragma once

/**
 * @file AppState.h
 * @brief Глобальное состояние приложения
 *
 * Централизованное хранилище состояния всех модулей системы.
 * Обеспечивает связь между компонентами без глобальных переменных.
 */

#include "MissionTypes.h"
#include "Config.h"
#include <cstdint>
#include <cstring>
#include <cstdio>

// Forward declarations
class WifiManagerEx;
class FrameReceiver;
class MavlinkUploader;
class MissionStore;
class Logger;

struct AppState {
    // ============ WiFi состояние ============
    bool wifiConnected = false;
    uint8_t wifiIP[4] = {0, 0, 0, 0};       // IP адрес
    int8_t activeWifiProfile = -1;          // Активный профиль (-1 = нет)
    uint32_t wifiReconnectAttempts = 0;     // Попытки переподключения

    // ============ Протокол состояние ============
    FrameRxState protocolRxState = FrameRxState::WAIT_START;
    bool lastCrcOk = false;
    uint16_t lastCrcCalc = 0;
    uint16_t lastCrcRecv = 0;
    uint32_t lastFrameLen = 0;
    uint32_t lastRxTime = 0;
    bool frameInProgress = false;
    uint32_t totalRxFrames = 0;      // Всего принято кадров
    uint32_t totalTxFrames = 0;      // Всего отправлено кадров
    uint32_t droppedFrames = 0;      // Отброшено кадров
    uint32_t lastTxLen = 0;          // Длина последнего отправленного кадра

    // ============ MAVLink состояние ============
    uint8_t targetSysid = 0;
    uint8_t targetCompid = 0;
    uint32_t lastHeartbeat = 0;
    uint32_t heartbeatCount = 0;
    bool mavlinkConnected = false;
    MissionUploadState uploadState = MissionUploadState::IDLE;
    uint16_t missionItemsSent = 0;
    uint16_t missionItemsTotal = 0;
    uint32_t uploadStartTime = 0;

    // ============ Миссия состояние ============
    bool missionReady = false;
    uint16_t missionCount = 0;
    uint32_t missionLoadedTime = 0;

    // ============ Дисплей состояние ============
    DisplayPage currentPage = DisplayPage::WIFI_STATUS;
    uint32_t lastDisplayUpdate = 0;
    uint32_t displayPageIndex = 0;

    // ============ Системное состояние ============
    uint32_t bootTime = 0;
    uint32_t uptime = 0;
    bool echoEnabled = false;
    bool firstBoot = true;
    bool usbSerialConnected = false;
    bool debugMode = true;
    int8_t protocolRxPin = -1; // -1 when USB serial is used
    int8_t protocolTxPin = -1; // -1 when USB serial is used
    int8_t mavRxPin = PIN_MAV_RX_DEBUG;
    int8_t mavTxPin = PIN_MAV_TX_DEBUG;

    // ============ Указатели на модули (для коллбэков) ============
    WifiManagerEx* wifiManager = nullptr;
    FrameReceiver* frameReceiver = nullptr;
    MavlinkUploader* mavlinkUploader = nullptr;
    MissionStore* missionStore = nullptr;
    Logger* logger = nullptr;

    // ============ Методы ============

    /**
     * @brief Получить IP как строку
     * @param buf Буфер для строки (минимум 16 байт)
     */
    void getIPString(char* buf, size_t len) const {
        if (buf && len >= 16) {
            snprintf(buf, len, "%d.%d.%d.%d",
                     wifiIP[0], wifiIP[1], wifiIP[2], wifiIP[3]);
        }
    }

    /**
     * @brief Установить IP из uint32_t
     */
    void setIP(uint32_t ip) {
        wifiIP[0] = (ip >> 0) & 0xFF;
        wifiIP[1] = (ip >> 8) & 0xFF;
        wifiIP[2] = (ip >> 16) & 0xFF;
        wifiIP[3] = (ip >> 24) & 0xFF;
    }

    /**
     * @brief Обновить uptime
     */
    void updateUptime(uint32_t currentMillis) {
        if (bootTime == 0) bootTime = currentMillis;
        uptime = currentMillis - bootTime;
    }

    /**
     * @brief Получить uptime в секундах
     */
    uint32_t getUptimeSec() const {
        return uptime / 1000;
    }

    /**
     * @brief Проверить таймаут heartbeat
     */
    bool isHeartbeatTimeout(uint32_t currentMillis, uint32_t timeout) const {
        if (lastHeartbeat == 0) return false;
        return (currentMillis - lastHeartbeat) > timeout;
    }

    /**
     * @brief Проверить таймаут загрузки миссии
     */
    bool isUploadTimeout(uint32_t currentMillis, uint32_t timeout) const {
        if (uploadStartTime == 0) return false;
        return (currentMillis - uploadStartTime) > timeout;
    }

    /**
     * @brief Получить прогресс загрузки (0-100)
     */
    uint8_t getUploadProgress() const {
        if (missionItemsTotal == 0) return 0;
        return (missionItemsSent * 100) / missionItemsTotal;
    }

    /**
     * @brief Сбросить состояние загрузки
     */
    void resetUpload() {
        uploadState = MissionUploadState::IDLE;
        missionItemsSent = 0;
        missionItemsTotal = 0;
        uploadStartTime = 0;
    }

    /**
     * @brief Начать загрузку
     * @param total Общее количество пунктов
     * @param currentMillis Текущее время в миллисекундах
     */
    void startUpload(uint16_t total, uint32_t currentMillis) {
        uploadState = MissionUploadState::WAIT_REQUEST;
        missionItemsTotal = total;
        missionItemsSent = 0;
        uploadStartTime = currentMillis;
    }

    /**
     * @brief Обновить состояние MAVLink
     */
    void updateMavlink(uint8_t sysid, uint8_t compid, uint32_t currentMillis) {
        targetSysid = sysid;
        targetCompid = compid;
        lastHeartbeat = currentMillis;
        mavlinkConnected = true;
        heartbeatCount++;
    }

    /**
     * @brief Установить состояние WiFi
     */
    void setWiFiState(bool connected, uint32_t ip = 0) {
        wifiConnected = connected;
        if (connected && ip != 0) {
            setIP(ip);
        }
    }

    /**
     * @brief Следующая страница дисплея
     */
    void nextDisplayPage() {
        displayPageIndex = (displayPageIndex + 1) % static_cast<uint32_t>(DisplayPage::COUNT);
        currentPage = static_cast<DisplayPage>(displayPageIndex);
    }

    /**
     * @brief Сбросить всё состояние
     */
    void reset() {
        wifiConnected = false;
        memset(wifiIP, 0, sizeof(wifiIP));
        activeWifiProfile = -1;
        wifiReconnectAttempts = 0;

        protocolRxState = FrameRxState::WAIT_START;
        lastCrcOk = false;
        lastCrcCalc = 0;
        lastCrcRecv = 0;
        lastFrameLen = 0;
        frameInProgress = false;

        targetSysid = 0;
        targetCompid = 0;
        lastHeartbeat = 0;
        mavlinkConnected = false;
        resetUpload();

        missionReady = false;
        missionCount = 0;

        currentPage = DisplayPage::WIFI_STATUS;
        displayPageIndex = 0;

        uptime = 0;
        usbSerialConnected = false;
        debugMode = true;
        protocolRxPin = -1;
        protocolTxPin = -1;
        mavRxPin = PIN_MAV_RX_DEBUG;
        mavTxPin = PIN_MAV_TX_DEBUG;
    }
};

// Глобальное состояние
extern AppState g_appState;
