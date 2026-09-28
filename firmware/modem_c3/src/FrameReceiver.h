#pragma once

/**
 * @file FrameReceiver.h
 * @brief Приемник кадров протокола 0x79 0x01 Size Payload CRC 0x47
 *
 * State machine для приема кадра:
 * WAIT_START -> WAIT_CMD -> WAIT_SIZE_LO -> WAIT_SIZE_HI ->
 * WAIT_PAYLOAD -> WAIT_CRC_LO -> WAIT_CRC_HI -> WAIT_STOP
 */

#include "MissionTypes.h"
#include "ProtocolCrc.h"
#include "AppState.h"
#include <cstdint>
#include <cstddef>

class MissionParser;
class MissionStore;
class Logger;
class Stream;

class FrameReceiver {
public:
    /**
     * @brief Callback при успешном приеме кадра
     * @param payload Данные
     * @param len Размер
     */
    using OnFrameReceived = bool (*)(const uint8_t* payload, size_t len);

    /**
     * @brief Callback при ошибке CRC
     */
    using OnCrcError = void (*)(uint16_t expected, uint16_t received);

    /**
     * @brief Инициализация приемника
     * @param maxPayloadSize Максимальный размер payload
     * @return true при успехе
     */
    bool init(size_t maxPayloadSize = 65535);

    /**
     * @brief Деинициализация
     */
    void deinit();

    /**
     * @brief Установить callback при успехе
     */
    void setOnFrameReceived(OnFrameReceived cb) { onFrameReceived_ = cb; }

    /**
     * @brief Установить callback при ошибке CRC
     */
    void setOnCrcError(OnCrcError cb) { onCrcError_ = cb; }

    /**
     * @brief Установить интерфейс ввода/вывода для протокола
     * @param io Stream (Serial/HardwareSerial)
     */
    void setIo(Stream* io) { io_ = io; }

    /**
     * @brief Обработка входящих данных из Serial
     * @return true если принят полный кадр
     */
    bool pollSerial();

    /**
     * @brief Сбросить состояние приемника
     */
    void reset();

    /**
     * @brief Получить текущее состояние
     */
    FrameRxState getState() const { return state_; }

    /**
     * @brief Получить размер ожидаемого payload
     */
    uint16_t getExpectedSize() const { return payloadSize_; }

    /**
     * @brief Получить текущее количество принятых байт payload
     */
    uint16_t getReceivedSize() const { return payloadReceived_; }

    /**
     * @brief Проверить идет ли прием
     */
    bool isReceiving() const { return state_ != FrameRxState::WAIT_START; }

    /**
     * @brief Получить прогресс приема (0-100)
     */
    uint8_t getProgress() const;

private:
    // Буферы
    uint8_t* payloadBuffer_ = nullptr;
    size_t bufferSize_ = 0;

    // Состояние
    FrameRxState state_ = FrameRxState::WAIT_START;
    uint16_t payloadSize_ = 0;
    uint16_t payloadReceived_ = 0;
    uint16_t crcCalculated_ = 0;
    uint16_t crcReceived_ = 0;
    uint32_t lastByteTime_ = 0;

    // Callbacks
    OnFrameReceived onFrameReceived_ = nullptr;
    OnCrcError onCrcError_ = nullptr;
    Stream* io_ = nullptr;

    // Обработка байта
    void processByte(uint8_t b);

    // Отправка квитанции
    void sendAck(uint8_t result);

    // State handlers
    void handleWaitStart(uint8_t b);
    void handleWaitCmd(uint8_t b);
    void handleWaitSizeLo(uint8_t b);
    void handleWaitSizeHi(uint8_t b);
    void handleWaitPayload(uint8_t b);
    void handleWaitCrcLo(uint8_t b);
    void handleWaitCrcHi(uint8_t b);
    void handleWaitStop(uint8_t b);

    // Валидация
    bool validateCrc();
};

// Глобальный экземпляр
extern FrameReceiver g_frameReceiver;
