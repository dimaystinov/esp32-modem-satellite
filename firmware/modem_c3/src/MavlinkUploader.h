#pragma once

/**
 * @file MavlinkUploader.h
 * @brief MAVLink загрузчик миссии
 *
 * Реализует протокол загрузки миссии на полётник:
 * MISSION_COUNT -> MISSION_REQUEST -> MISSION_ITEM_INT -> MISSION_ACK
 */

#include "Config.h"
#include "MissionTypes.h"
#include <cstdint>
#include <cstddef>
#include <vector>

class MissionStore;
class Logger;

class MavlinkUploader {
public:
    /**
     * @brief Callback при завершении загрузки
     * @param success true если успешно
     */
    using OnUploadComplete = void (*)(bool success);

    /**
     * @brief Инициализация MAVLink
     * @param rxPin Пин RX
     * @param txPin Пин TX
     * @param baud Скорость
     * @return true при успехе
     */
    bool init(uint8_t rxPin = PIN_MAV_RX, uint8_t txPin = PIN_MAV_TX, uint32_t baud = 115200);

    /**
     * @brief Деинициализация
     */
    void deinit();

    /**
     * @brief Цикл обработки MAVLink
     */
    void poll();

    /**
     * @brief Начать загрузку миссии
     * @return true если загрузка начата
     */
    bool startUpload();

    /**
     * @brief Остановить загрузку
     */
    void stopUpload();

    /**
     * @brief Установить callback
     */
    void setOnComplete(OnUploadComplete cb) { onComplete_ = cb; }

    /**
     * @brief Получить состояние загрузки
     */
    MissionUploadState getState() const { return state_; }

    /**
     * @brief Получить прогресс (0-100)
     */
    uint8_t getProgress() const;

    /**
     * @brief Получить количество отправленных точек
     */
    uint16_t getSentCount() const { return sentCount_; }

    /**
     * @brief Получить общее количество точек
     */
    uint16_t getTotalCount() const { return totalCount_; }

    /**
     * @brief Проверить подключение к полётнику
     * @return true если получали heartbeat
     */
    bool isConnected() const;
    void clearLink();
    uint32_t rxBytes() const { return rxBytes_; }
    uint32_t lastByteTime() const { return lastByteTime_; }

    /**
     * @brief Получить sysid полётника
     */
    uint8_t getTargetSysId() const { return targetSysId_; }

    /**
     * @brief Получить compid полётника
     */
    uint8_t getTargetCompId() const { return targetCompId_; }

    /**
     * @brief Получить количество heartbeat
     */
    uint32_t getHeartbeatCount() const { return heartbeatCount_; }

    /**
     * @brief Получить время последнего heartbeat
     */
    uint32_t getLastHeartbeat() const { return lastHeartbeat_; }

private:
    uint32_t rxBytes_ = 0, lastByteTime_ = 0;
    // UART
    uint8_t rxPin_ = 17;
    uint8_t txPin_ = 16;
    uint32_t baud_ = 115200;

    // MAVLink state
    uint8_t targetSysId_ = 0;
    uint8_t targetCompId_ = 0;
    uint32_t lastHeartbeat_ = 0;
    uint32_t heartbeatCount_ = 0;

    // Upload state
    MissionUploadState state_ = MissionUploadState::IDLE;
    uint16_t sentCount_ = 0;
    uint16_t totalCount_ = 0;
    uint32_t uploadStartTime_ = 0;

    // Callback
    OnUploadComplete onComplete_ = nullptr;

    // Buffer
    uint8_t rxBuffer_[MAVLINK_BUF_SIZE];
    size_t rxLen_ = 0;

    // Приватные методы
    void processMavlink();
    void handleHeartbeat(const uint8_t* data, size_t len);
    void handleMissionRequest(const uint8_t* data, size_t len);
    void handleMissionRequestInt(const uint8_t* data, size_t len);
    void handleMissionAck(const uint8_t* data, size_t len);
    bool sendMissionCount(uint16_t count);
    bool sendMissionItem(uint16_t seq);
    void resetState();
    void checkTimeout();
};

// Глобальный экземпляр
extern MavlinkUploader g_mavlinkUploader;
