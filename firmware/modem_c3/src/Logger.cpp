/**
 * @file Logger.cpp
 * @brief Реализация модуля логирования
 */

#include "Logger.h"
#include <cstdarg>
#include <cstdio>

// Глобальный экземпляр
Logger g_logger;

// ============================================================================
// Инициализация
// ============================================================================

bool Logger::init(size_t rxHexSize, size_t rxAsciiSize,
                  size_t txHexSize, size_t txAsciiSize,
                  size_t statusSize) {
    // Освобождаем старую память если есть
    deinit();

    // Сохраняем размеры
    rxHexSize_ = rxHexSize;
    rxAsciiSize_ = rxAsciiSize;
    txHexSize_ = txHexSize;
    txAsciiSize_ = txAsciiSize;
    statusSize_ = statusSize;

    // Выделяем память (+1 для null terminator)
    rxHex_ = new char[rxHexSize_ + 1];
    rxAscii_ = new char[rxAsciiSize_ + 1];
    txHex_ = new char[txHexSize_ + 1];
    txAscii_ = new char[txAsciiSize_ + 1];
    status_ = new char[statusSize_ + 1];

    // Проверяем выделение
    if (!rxHex_ || !rxAscii_ || !txHex_ || !txAscii_ || !status_) {
        deinit();
        return false;
    }

    // Инициализируем пустыми строками
    rxHex_[0] = '\0';
    rxAscii_[0] = '\0';
    txHex_[0] = '\0';
    txAscii_[0] = '\0';
    status_[0] = '\0';

    return true;
}

void Logger::deinit() {
    delete[] rxHex_;
    delete[] rxAscii_;
    delete[] txHex_;
    delete[] txAscii_;
    delete[] status_;

    rxHex_ = nullptr;
    rxAscii_ = nullptr;
    txHex_ = nullptr;
    txAscii_ = nullptr;
    status_ = nullptr;

    rxHexLen_ = rxAsciiLen_ = txHexLen_ = txAsciiLen_ = statusLen_ = 0;
    rxHexSize_ = rxAsciiSize_ = txHexSize_ = txAsciiSize_ = statusSize_ = 0;
}

// ============================================================================
// Вспомогательные функции
// ============================================================================

char Logger::hexNibble(uint8_t v) {
    return (v < 10) ? ('0' + v) : ('A' + (v - 10));
}

void Logger::appendChar(char* buf, size_t& len, size_t maxLen, char c) {
    if (len < maxLen) {
        buf[len++] = c;
        buf[len] = '\0';
    } else {
        // Кольцевой буфер - сдвигаем и добавляем в конец
        if (maxLen > 0) {
            memmove(buf, buf + 1, maxLen - 1);
            buf[maxLen - 1] = c;
            len = maxLen;
            buf[len] = '\0';
        }
    }
}

void Logger::appendText(char* buf, size_t& len, size_t maxLen, const char* s) {
    while (s && *s) {
        appendChar(buf, len, maxLen, *s++);
    }
}

// ============================================================================
// Добавление данных
// ============================================================================

void Logger::appendRxByte(uint8_t b) {
    if (!rxHex_ || !rxAscii_) return;

    // HEX: два символа на байт
    appendChar(rxHex_, rxHexLen_, rxHexSize_, hexNibble((b >> 4) & 0x0F));
    appendChar(rxHex_, rxHexLen_, rxHexSize_, hexNibble(b & 0x0F));

    // ASCII: печатаемый символ или точка
    char c = (b >= 32 && b < 127) ? static_cast<char>(b) : '.';
    appendChar(rxAscii_, rxAsciiLen_, rxAsciiSize_, c);

    totalRxBytes_++;
}

void Logger::appendTxByte(uint8_t b) {
    if (!txHex_ || !txAscii_) return;

    // HEX: два символа на байт
    appendChar(txHex_, txHexLen_, txHexSize_, hexNibble((b >> 4) & 0x0F));
    appendChar(txHex_, txHexLen_, txHexSize_, hexNibble(b & 0x0F));

    // ASCII: печатаемый символ или точка
    char c = (b >= 32 && b < 127) ? static_cast<char>(b) : '.';
    appendChar(txAscii_, txAsciiLen_, txAsciiSize_, c);

    totalTxBytes_++;
}

void Logger::appendStatus(const char* msg) {
    if (!status_ || !msg) return;

    appendText(status_, statusLen_, statusSize_, msg);
    appendChar(status_, statusLen_, statusSize_, '\n');
}

void Logger::appendStatusF(const char* fmt, ...) {
    if (!status_ || !fmt) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    appendStatus(buf);
}

// ============================================================================
// Очистка
// ============================================================================

void Logger::clearAll() {
    clearRx();
    clearTx();
    clearStatus();
}

void Logger::clearRx() {
    if (rxHex_) rxHex_[0] = '\0';
    if (rxAscii_) rxAscii_[0] = '\0';
    rxHexLen_ = 0;
    rxAsciiLen_ = 0;
    totalRxBytes_ = 0;
    totalRxFrames_ = 0;
}

void Logger::clearTx() {
    if (txHex_) txHex_[0] = '\0';
    if (txAscii_) txAscii_[0] = '\0';
    txHexLen_ = 0;
    txAsciiLen_ = 0;
    totalTxBytes_ = 0;
    totalTxFrames_ = 0;
}

void Logger::clearStatus() {
    if (status_) status_[0] = '\0';
    statusLen_ = 0;
}
