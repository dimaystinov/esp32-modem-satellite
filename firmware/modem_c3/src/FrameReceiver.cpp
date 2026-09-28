/**
 * @file FrameReceiver.cpp
 * @brief Реализация приемника кадров
 */

#include "FrameReceiver.h"
#include "Config.h"
#include "Logger.h"
#include "AppState.h"
#include <Arduino.h>

// Глобальный экземпляр
FrameReceiver g_frameReceiver;

// ============================================================================
// Инициализация
// ============================================================================

bool FrameReceiver::init(size_t maxPayloadSize) {
    // Освобождаем старый буфер
    deinit();

    bufferSize_ = maxPayloadSize + 10; // + margin
    payloadBuffer_ = new uint8_t[bufferSize_];

    if (!payloadBuffer_) {
        return false;
    }

    if (!io_) {
        io_ = &Serial;
    }

    reset();
    return true;
}

void FrameReceiver::deinit() {
    delete[] payloadBuffer_;
    payloadBuffer_ = nullptr;
    bufferSize_ = 0;
}

void FrameReceiver::reset() {
    state_ = FrameRxState::WAIT_START;
    payloadSize_ = 0;
    payloadReceived_ = 0;
    crcCalculated_ = 0xFFFF;
    crcReceived_ = 0;

    if (g_appState.logger) {
        g_appState.protocolRxState = state_;
    }
}

// ============================================================================
// Основной цикл
// ============================================================================

bool FrameReceiver::pollSerial() {
    if (!payloadBuffer_ || !io_) return false;

    bool frameReceived = false;

    while (io_->available() > 0) {
        int c = io_->read();
        if (c < 0) break;

        uint8_t b = static_cast<uint8_t>(c);

        // Логируем входящий байт если нужно
        if (g_logger.getRxHex()) {
            g_logger.appendRxByte(b);
        }

        // Echo режим
        if (g_appState.echoEnabled) {
            io_->write(&b, 1);
            if (g_logger.getTxHex()) {
                g_logger.appendTxByte(b);
            }
        }

        processByte(b);
        lastByteTime_ = millis();

        // Если кадр полностью принят
        if (state_ == FrameRxState::WAIT_START && payloadReceived_ > 0) {
            frameReceived = true;
        }
    }

    // Проверка таймаута
    if (isReceiving() && (millis() - lastByteTime_) > FRAME_RX_TIMEOUT) {
        if (g_logger.getStatus()) {
            g_logger.appendStatus("Frame RX timeout");
        }
        reset();
    }

    return frameReceived;
}

void FrameReceiver::processByte(uint8_t b) {
    switch (state_) {
        case FrameRxState::WAIT_START:
            handleWaitStart(b);
            break;
        case FrameRxState::WAIT_CMD:
            handleWaitCmd(b);
            break;
        case FrameRxState::WAIT_SIZE_LO:
            handleWaitSizeLo(b);
            break;
        case FrameRxState::WAIT_SIZE_HI:
            handleWaitSizeHi(b);
            break;
        case FrameRxState::WAIT_PAYLOAD:
            handleWaitPayload(b);
            break;
        case FrameRxState::WAIT_CRC_LO:
            handleWaitCrcLo(b);
            break;
        case FrameRxState::WAIT_CRC_HI:
            handleWaitCrcHi(b);
            break;
        case FrameRxState::WAIT_STOP:
            handleWaitStop(b);
            break;
    }

    // Обновляем глобальное состояние
    g_appState.protocolRxState = state_;
    g_appState.frameInProgress = isReceiving();
}

// ============================================================================
// State handlers
// ============================================================================

void FrameReceiver::handleWaitStart(uint8_t b) {
    if (b == PROTO_START_BYTE) {
        state_ = FrameRxState::WAIT_CMD;
        crcCalculated_ = 0xFFFF;
        crcCalculated_ = ProtocolCrc::update(crcCalculated_, b);
    }
}

void FrameReceiver::handleWaitCmd(uint8_t b) {
    if (b == PROTO_CMD_MISSION) {
        state_ = FrameRxState::WAIT_SIZE_LO;
        crcCalculated_ = ProtocolCrc::update(crcCalculated_, b);
    } else {
        // Неверная команда, сброс
        reset();
    }
}

void FrameReceiver::handleWaitSizeLo(uint8_t b) {
    payloadSize_ = b;
    state_ = FrameRxState::WAIT_SIZE_HI;
    crcCalculated_ = ProtocolCrc::update(crcCalculated_, b);
}

void FrameReceiver::handleWaitSizeHi(uint8_t b) {
    payloadSize_ |= (static_cast<uint16_t>(b) << 8);

    // Проверяем размер
    if (payloadSize_ > bufferSize_) {
        if (g_logger.getStatus()) {
            g_logger.appendStatusF("Payload too large: %u", payloadSize_);
        }
        reset();
        return;
    }

    payloadReceived_ = 0;
    state_ = FrameRxState::WAIT_PAYLOAD;
    crcCalculated_ = ProtocolCrc::update(crcCalculated_, b);

    if (g_logger.getStatus()) {
        g_logger.appendStatusF("Frame start, size: %u", payloadSize_);
    }
}

void FrameReceiver::handleWaitPayload(uint8_t b) {
    if (payloadReceived_ < bufferSize_) {
        payloadBuffer_[payloadReceived_++] = b;
        crcCalculated_ = ProtocolCrc::update(crcCalculated_, b);

        if (payloadReceived_ >= payloadSize_) {
            state_ = FrameRxState::WAIT_CRC_LO;
        }
    }
}

void FrameReceiver::handleWaitCrcLo(uint8_t b) {
    crcReceived_ = b;
    state_ = FrameRxState::WAIT_CRC_HI;
}

void FrameReceiver::handleWaitCrcHi(uint8_t b) {
    crcReceived_ |= (static_cast<uint16_t>(b) << 8);
    state_ = FrameRxState::WAIT_STOP;
}

void FrameReceiver::handleWaitStop(uint8_t b) {
    if (b == PROTO_STOP_BYTE) {
        // Проверяем CRC
        if (validateCrc()) {
            // Успех
            g_appState.lastCrcOk = true;
            g_appState.lastCrcCalc = crcCalculated_;
            g_appState.lastCrcRecv = crcReceived_;
            g_appState.lastFrameLen = payloadReceived_;
            g_appState.totalRxFrames++;

            // Отправляем ACK
            // Application ACK follows parsing and persistent storage.

            if (g_logger.getStatus()) {
                g_logger.appendStatusF("Frame OK, CRC=%04X, len=%u",
                                      crcCalculated_, payloadReceived_);
            }

            // Вызываем callback
            const bool accepted = onFrameReceived_ && onFrameReceived_(payloadBuffer_, payloadReceived_);
            sendAck(accepted ? PROTO_ACK_OK : PROTO_ACK_ERROR);
        } else {
            // Ошибка CRC
            g_appState.lastCrcOk = false;
            g_appState.lastCrcCalc = crcCalculated_;
            g_appState.lastCrcRecv = crcReceived_;
            g_appState.droppedFrames++;

            // Отправляем NACK
            sendAck(PROTO_ACK_ERROR);

            if (g_logger.getStatus()) {
                g_logger.appendStatusF("CRC error: calc=%04X recv=%04X",
                                      crcCalculated_, crcReceived_);
            }

            if (onCrcError_) {
                onCrcError_(crcCalculated_, crcReceived_);
            }
        }
    } else {
        // Неверный стоповый байт
        if (g_logger.getStatus()) {
            g_logger.appendStatus("Invalid stop byte");
        }
        g_appState.droppedFrames++;
    }

    reset();
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

bool FrameReceiver::validateCrc() {
    return (crcCalculated_ == crcReceived_);
}

void FrameReceiver::sendAck(uint8_t result) {
    uint8_t ack[4] = {
        PROTO_START_BYTE,
        PROTO_CMD_MISSION,
        result,
        PROTO_STOP_BYTE
    };

    if (io_) {
        io_->write(ack, 4);
    }

    // Логируем отправленное
    for (int i = 0; i < 4; i++) {
        if (g_logger.getTxHex()) {
            g_logger.appendTxByte(ack[i]);
        }
    }

    g_appState.totalTxFrames++;
    g_appState.lastTxLen = 4;
}

uint8_t FrameReceiver::getProgress() const {
    if (state_ == FrameRxState::WAIT_START) return 0;
    if (state_ == FrameRxState::WAIT_PAYLOAD && payloadSize_ > 0) {
        return (payloadReceived_ * 100) / payloadSize_;
    }
    if (state_ >= FrameRxState::WAIT_CRC_LO) return 100;
    return 50; // Прием заголовка
}
