#pragma once

/**
 * @file ProtocolCrc.h
 * @brief CRC-16 IBM-SDLC для протокола
 *
 * Алгоритм: CRC-16 IBM-SDLC
 * Poly: 0x1021
 * Init: 0xFFFF
 * RefIn: false
 * RefOut: false
 * XorOut: 0x0000
 */

#include <cstdint>
#include <cstddef>

class ProtocolCrc {
public:
    /**
     * @brief Вычислить CRC-16 для массива байт
     * @param data Указатель на данные
     * @param len Количество байт
     * @return CRC-16
     */
    static uint16_t calculate(const uint8_t* data, size_t len);

    /**
     * @brief Вычислить CRC-16 для байта (инкрементально)
     * @param crc Текущее значение CRC
     * @param data Байт данных
     * @return Новое значение CRC
     */
    static uint16_t update(uint16_t crc, uint8_t data);

    /**
     * @brief Проверить CRC
     * @param data Данные (без CRC)
     * @param len Длина данных
     * @param expectedCrc Ожидаемый CRC
     * @return true если CRC совпадает
     */
    static bool verify(const uint8_t* data, size_t len, uint16_t expectedCrc);

private:
    // CRC-16 таблица для IBM-SDLC (poly=0x1021)
    static const uint16_t crcTable[256];

    // Инициализация таблицы (вызывается автоматически)
    static void initTable();
    static bool tableInitialized;
};
