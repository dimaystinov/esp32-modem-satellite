#pragma once

/**
 * @file Logger.h
 * @brief Модуль логирования RX/TX данных и статуса
 *
 * Потокобезопасный (для single-thread) логгер с кольцевыми буферами.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>

// ============================================================================
// Интерфейс Logger
// ============================================================================

class Logger {
public:
    /**
     * @brief Инициализация логгера с заданными размерами буферов
     * @param rxHexSize Размер RX hex буфера
     * @param rxAsciiSize Размер RX ASCII буфера
     * @param txHexSize Размер TX hex буфера
     * @param txAsciiSize Размер TX ASCII буфера
     * @param statusSize Размер статус буфера
     * @return true при успехе
     */
    bool init(size_t rxHexSize = 32768, size_t rxAsciiSize = 16384,
              size_t txHexSize = 32768, size_t txAsciiSize = 16384,
              size_t statusSize = 8192);

    /**
     * @brief Деинициализация - освобождение памяти
     */
    void deinit();

    /**
     * @brief Добавить RX байт
     * @param b Байт для логирования
     */
    void appendRxByte(uint8_t b);

    /**
     * @brief Добавить TX байт
     * @param b Байт для логирования
     */
    void appendTxByte(uint8_t b);

    /**
     * @brief Добавить строку в статус лог
     * @param msg Сообщение (null-terminated)
     */
    void appendStatus(const char* msg);

    /**
     * @brief Отформатированная запись в статус лог
     * @param fmt Формат строки printf-style
     * @param ... Аргументы
     */
    void appendStatusF(const char* fmt, ...);

    /**
     * @brief Очистить все логи
     */
    void clearAll();

    /**
     * @brief Очистить RX лог
     */
    void clearRx();

    /**
     * @brief Очистить TX лог
     */
    void clearTx();

    /**
     * @brief Очистить статус лог
     */
    void clearStatus();

    // Getters
    const char* getRxHex() const { return rxHex_; }
    const char* getRxAscii() const { return rxAscii_; }
    const char* getTxHex() const { return txHex_; }
    const char* getTxAscii() const { return txAscii_; }
    const char* getStatus() const { return status_; }

    size_t getRxHexLen() const { return rxHexLen_; }
    size_t getRxAsciiLen() const { return rxAsciiLen_; }
    size_t getTxHexLen() const { return txHexLen_; }
    size_t getTxAsciiLen() const { return txAsciiLen_; }
    size_t getStatusLen() const { return statusLen_; }

    uint32_t getTotalRxBytes() const { return totalRxBytes_; }
    uint32_t getTotalTxBytes() const { return totalTxBytes_; }
    uint32_t getTotalRxFrames() const { return totalRxFrames_; }
    uint32_t getTotalTxFrames() const { return totalTxFrames_; }
    uint32_t getDroppedFrames() const { return droppedFrames_; }

    void incrementRxFrames() { totalRxFrames_++; }
    void incrementTxFrames() { totalTxFrames_++; }
    void incrementDroppedFrames() { droppedFrames_++; }

    void setLastRxLen(uint32_t len) { lastRxLen_ = len; }
    void setLastTxLen(uint32_t len) { lastTxLen_ = len; }
    void setExpectedLen(uint32_t len) { expectedLen_ = len; }
    void setFrameLen(uint32_t len) { frameLen_ = len; }

    uint32_t getLastRxLen() const { return lastRxLen_; }
    uint32_t getLastTxLen() const { return lastTxLen_; }
    uint32_t getExpectedLen() const { return expectedLen_; }
    uint32_t getFrameLen() const { return frameLen_; }

private:
    // Буферы
    char* rxHex_ = nullptr;
    char* rxAscii_ = nullptr;
    char* txHex_ = nullptr;
    char* txAscii_ = nullptr;
    char* status_ = nullptr;

    // Размеры
    size_t rxHexSize_ = 0;
    size_t rxAsciiSize_ = 0;
    size_t txHexSize_ = 0;
    size_t txAsciiSize_ = 0;
    size_t statusSize_ = 0;

    // Текущие длины
    size_t rxHexLen_ = 0;
    size_t rxAsciiLen_ = 0;
    size_t txHexLen_ = 0;
    size_t txAsciiLen_ = 0;
    size_t statusLen_ = 0;

    // Счетчики
    uint32_t totalRxBytes_ = 0;
    uint32_t totalTxBytes_ = 0;
    uint32_t totalRxFrames_ = 0;
    uint32_t totalTxFrames_ = 0;
    uint32_t droppedFrames_ = 0;

    // Метаданные последнего кадра
    uint32_t lastRxLen_ = 0;
    uint32_t lastTxLen_ = 0;
    uint32_t expectedLen_ = 0;
    uint32_t frameLen_ = 0;

    // Вспомогательные функции
    static char hexNibble(uint8_t v);
    void appendChar(char* buf, size_t& len, size_t maxLen, char c);
    void appendText(char* buf, size_t& len, size_t maxLen, const char* s);
};

// Глобальный экземпляр
extern Logger g_logger;
