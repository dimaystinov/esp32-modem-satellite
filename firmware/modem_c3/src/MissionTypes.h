#pragma once

/**
 * @file MissionTypes.h
 * @brief Типы данных для миссии и MAVLink
 */

#include <cstdint>
#include <cstddef>

// ============================================================================
// Структура точки маршрута
// ============================================================================

/**
 * @struct Waypoint
 * @brief Точка маршрута QGC WPL 110 формата
 *
 * Структура соответствует формату Mission Planner:
 * seq, current, frame, command, param1-4, lat, lon, alt, autocontinue
 */
struct Waypoint {
    uint16_t seq;           // Порядковый номер (0-based)
    uint8_t current;        // Текущая точка (1 = home, 0 = обычная)
    uint8_t frame;          // Координатная система (0=ABS, 3=REL)
    uint16_t command;       // Команда MAV_CMD (16=WAYPOINT)
    float param1;           // Параметр 1 (задержка в сек)
    float param2;           // Параметр 2 (радиус принятия)
    float param3;           // Параметр 3 (пропуск)
    float param4;          // Параметр 4 (Yaw)
    double lat;            // Широта (градусы)
    double lon;            // Долгота (градусы)
    float alt;             // Высота (метры)
    uint8_t autocontinue;  // Автопереход (0 или 1)
};

// ============================================================================
// Состояния модуля миссии
// ============================================================================

enum class MissionUploadState {
    IDLE,           // Ожидание
    WAIT_REQUEST,   // Ожидание запроса MISSION_REQUEST
    SENDING,        // Отправка точек
    DONE,           // Завершено успешно
    ERROR           // Ошибка
};

enum class FrameRxState {
    WAIT_START,     // Ожидание стартового байта
    WAIT_CMD,       // Ожидание кода команды
    WAIT_SIZE_LO,   // Ожидание младшего байта размера
    WAIT_SIZE_HI,   // Ожидание старшего байта размера
    WAIT_PAYLOAD,   // Прием payload
    WAIT_CRC_LO,    // Ожидание младшего байта CRC
    WAIT_CRC_HI,    // Ожидание старшего байта CRC
    WAIT_STOP       // Ожидание стопового байта
};

// ============================================================================
// Состояния дисплея
// ============================================================================

enum class DisplayPage {
    WIFI_STATUS,        // WiFi статус и IP
    MAVLINK_STATUS,     // MAVLink sysid/compid/heartbeat
    PROTOCOL_STATUS,    // Протокол: RX, CRC
    MISSION_STATUS,     // Миссия: количество точек, статус
    COUNT               // Количество страниц
};

// ============================================================================
// Константы валидации
// ============================================================================

namespace MissionLimits {
    constexpr double MIN_LAT = -90.0;   // Минимальная широта
    constexpr double MAX_LAT = 90.0;    // Максимальная широта
    constexpr double MIN_LON = -180.0;  // Минимальная долгота
    constexpr double MAX_LON = 180.0;   // Максимальная долгота
    constexpr float MIN_ALT = -500.0f;  // Минимальная высота
    constexpr float MAX_ALT = 10000.0f; // Максимальная высота
    constexpr uint16_t MAX_WAYPOINTS = 500; // Максимум точек
}

// ============================================================================
// Структура для передачи статуса
// ============================================================================

struct MissionStatus {
    uint16_t count;                     // Количество точек
    uint16_t uploaded;                  // Загружено точек
    MissionUploadState state;           // Состояние загрузки
    bool ready;                         // Миссия готова
    uint32_t lastError;                 // Последняя ошибка
};

struct ProtocolStatus {
    FrameRxState rxState;               // Текущее состояние приема
    uint32_t totalRxBytes;              // Всего принято байт
    uint32_t totalRxFrames;             // Всего принято кадров
    uint32_t droppedFrames;             // Отброшено кадров
    uint32_t lastFrameLen;              // Длина последнего кадра
    uint16_t lastCrcCalc;               // Посчитанный CRC
    uint16_t lastCrcRecv;               // Принятый CRC
    bool lastCrcOk;                     // CRC сошелся
};
