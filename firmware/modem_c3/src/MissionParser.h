#pragma once

/**
 * @file MissionParser.h
 * @brief Парсинг миссии QGC WPL 110
 *
 * Парсит текстовый формат waypoints:
 * QGC WPL 110
 * 0\t1\t0\t16\t0\t0\t0\t0\tlat\tlon\talt\t1
 */

#include "MissionTypes.h"
#include <cstdint>
#include <cstddef>
#include <vector>

class MissionParser {
public:
    /**
     * @brief Результат парсинга
     */
    enum class Result {
        OK,                 // Успех
        INVALID_HEADER,     // Неверный заголовок
        EMPTY_PAYLOAD,      // Пустой payload
        PARSE_ERROR,        // Ошибка парсинга строки
        INVALID_FIELD_COUNT,// Неверное количество полей
        INVALID_COORDS,     // Неверные координаты
        TOO_MANY_POINTS,    // Слишком много точек
        INVALID_SEQ,        // Неверный sequence
        NO_MEMORY           // Нет памяти
    };

    /**
     * @brief Структура результата парсинга
     */
    struct ParseResult {
        Result result;
        uint16_t pointsParsed;
        uint16_t lineNumber;
        const char* errorMsg;
    };

    /**
     * @brief Парсить payload как текстовую миссию
     * @param payload Указатель на данные
     * @param len Размер данных
     * @param waypoints Вектор для результата
     * @return Результат парсинга
     */
    ParseResult parse(const uint8_t* payload, size_t len,
                      std::vector<Waypoint>& waypoints);

    /**
     * @brief Парсить из null-terminated строки
     * @param text Текст миссии
     * @param waypoints Вектор для результата
     * @return Результат парсинга
     */
    ParseResult parse(const char* text, std::vector<Waypoint>& waypoints);

    /**
     * @brief Получить текстовое описание ошибки
     * @param result Код результата
     * @return Строка с описанием
     */
    static const char* getErrorString(Result result);

    /**
     * @brief Проверить валидность миссии
     * @param waypoints Вектор точек
     * @return true если миссия валидна
     */
    static bool validateMission(const std::vector<Waypoint>& waypoints);

private:
    ParseResult parseOwned(char* buffer, std::vector<Waypoint>& waypoints);
    /**
     * @brief Проверить заголовок
     * @param line Первая строка
     * @return true если заголовок корректный
     */
    bool checkHeader(const char* line);

    /**
     * @brief Разобрать одну строку waypoint
     * @param line Строка с данными
     * @param wp Структура для результата
     * @return true при успехе
     */
    bool parseWaypointLine(const char* line, Waypoint& wp);

    /**
     * @brief Валидировать координаты
     * @param lat Широта
     * @param lon Долгота
     * @param alt Высота
     * @return true если координаты валидны
     */
    bool validateCoords(double lat, double lon, float alt);

    // Вспомогательные функции
    static bool parseDouble(const char* str, double& value);
    static bool parseFloat(const char* str, float& value);
    static bool parseUint16(const char* str, uint16_t& value);
    static bool parseUint8(const char* str, uint8_t& value);
    static char* trim(char* str);
};
