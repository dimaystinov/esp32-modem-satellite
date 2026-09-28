/**
 * @file MissionParser.cpp
 * @brief Реализация парсера миссии
 */

#include "MissionParser.h"
#include "Config.h"
#include <cstdlib>
#include <cctype>
#include <cstring>

// ============================================================================
// Публичные методы
// ============================================================================

MissionParser::ParseResult MissionParser::parse(const uint8_t* payload, size_t len,
                                                std::vector<Waypoint>& waypoints) {
    if (!payload || len == 0) {
        return {Result::EMPTY_PAYLOAD, 0, 0, "Empty payload"};
    }

    // Создаем копию с null-terminator
    char* buffer = new char[len + 1];
    if (!buffer) {
        return {Result::NO_MEMORY, 0, 0, "No memory for buffer"};
    }

    memcpy(buffer, payload, len);
    buffer[len] = '\0';

    return parseOwned(buffer, waypoints);
}

MissionParser::ParseResult MissionParser::parse(const char* text,
                                                std::vector<Waypoint>& waypoints) {
    if (!text || strlen(text) == 0) {
        return {Result::EMPTY_PAYLOAD, 0, 0, "Empty text"};
    }

    waypoints.clear();

    // Создаем изменяемую копию для токенизации
    size_t len = strlen(text);
    char* buffer = new char[len + 1];
    if (!buffer) {
        return {Result::NO_MEMORY, 0, 0, "No memory for tokenization"};
    }
    strcpy(buffer, text);

    return parseOwned(buffer, waypoints);
}

MissionParser::ParseResult MissionParser::parseOwned(char* buffer, std::vector<Waypoint>& waypoints) {
    waypoints.clear();
    // Разбиваем на строки
    char* savePtr = nullptr;
    char* line = strtok_r(buffer, "\r\n", &savePtr);
    uint16_t lineNum = 0;

    // Первая строка - заголовок
    if (!line) {
        delete[] buffer;
        return {Result::EMPTY_PAYLOAD, 0, 0, "No lines found"};
    }

    if (!checkHeader(line)) {
        delete[] buffer;
        return {Result::INVALID_HEADER, 0, 1, "Invalid header, expected 'QGC WPL 110'"};
    }

    // Парсим точки
    line = strtok_r(nullptr, "\r\n", &savePtr);
    lineNum = 1;

    while (line) {
        // Пропускаем пустые строки
        char* trimmed = trim(line);
        if (strlen(trimmed) == 0) {
            line = strtok_r(nullptr, "\r\n", &savePtr);
            lineNum++;
            continue;
        }

        // Проверяем лимит точек
        if (waypoints.size() >= MissionLimits::MAX_WAYPOINTS) {
            delete[] buffer;
            return {Result::TOO_MANY_POINTS, static_cast<uint16_t>(waypoints.size()),
                    lineNum, "Too many waypoints"};
        }

        // Парсим строку
        Waypoint wp;
        if (!parseWaypointLine(trimmed, wp)) {
            delete[] buffer;
            return {Result::PARSE_ERROR, static_cast<uint16_t>(waypoints.size()),
                    lineNum, "Failed to parse waypoint line"};
        }

        waypoints.push_back(wp);

        line = strtok_r(nullptr, "\r\n", &savePtr);
        lineNum++;
    }

    delete[] buffer;

    if (waypoints.empty()) {
        return {Result::PARSE_ERROR, 0, 0, "No waypoints found"};
    }

    // Валидируем всю миссию
    if (!validateMission(waypoints)) {
        return {Result::INVALID_SEQ, static_cast<uint16_t>(waypoints.size()),
                0, "Mission validation failed"};
    }

    return {Result::OK, static_cast<uint16_t>(waypoints.size()), 0, nullptr};
}

const char* MissionParser::getErrorString(Result result) {
    switch (result) {
        case Result::OK: return "OK";
        case Result::INVALID_HEADER: return "Invalid header";
        case Result::EMPTY_PAYLOAD: return "Empty payload";
        case Result::PARSE_ERROR: return "Parse error";
        case Result::INVALID_FIELD_COUNT: return "Invalid field count";
        case Result::INVALID_COORDS: return "Invalid coordinates";
        case Result::TOO_MANY_POINTS: return "Too many points";
        case Result::INVALID_SEQ: return "Invalid sequence";
        case Result::NO_MEMORY: return "No memory";
        default: return "Unknown error";
    }
}

bool MissionParser::validateMission(const std::vector<Waypoint>& waypoints) {
    if (waypoints.empty()) return false;

    // Проверяем последовательность
    for (size_t i = 0; i < waypoints.size(); i++) {
        if (waypoints[i].seq != i) {
            return false;
        }
    }

    // Проверяем что seq 0 - это home (current=1)
    if (waypoints[0].current != 1) {
        return false;
    }

    return true;
}

// ============================================================================
// Приватные методы
// ============================================================================

bool MissionParser::checkHeader(const char* line) {
    if (!line) return false;

    if (static_cast<uint8_t>(line[0]) == 0xEF &&
        static_cast<uint8_t>(line[1]) == 0xBB &&
        static_cast<uint8_t>(line[2]) == 0xBF) {
        line += 3;
    }

    while (*line && isspace(static_cast<unsigned char>(*line))) {
        line++;
    }

    return (strncmp(line, "QGC WPL 110", 11) == 0);
}

bool MissionParser::parseWaypointLine(const char* line, Waypoint& wp) {
    // Формат: seq\tcurrent\tframe\tcommand\tp1\tp2\tp3\tp4\tlat\tlon\talt\tautocontinue
    // Или через пробелы

    char buffer[512];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Заменяем табуляции на пробелы
    for (char* p = buffer; *p; p++) {
        if (*p == '\t') *p = ' ';
    }

    // Парсим поля
    char* fields[12];
    int fieldCount = 0;
    char* p = buffer;

    while (*p && fieldCount < 12) {
        while (*p == ' ') p++;
        if (!*p) break;

        fields[fieldCount++] = p;

        while (*p && *p != ' ') p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }

    if (fieldCount != 12) {
        return false;
    }

    // Парсим каждое поле
    if (!parseUint16(fields[0], wp.seq)) return false;
    if (!parseUint8(fields[1], wp.current)) return false;
    if (!parseUint8(fields[2], wp.frame)) return false;
    if (!parseUint16(fields[3], wp.command)) return false;
    if (!parseFloat(fields[4], wp.param1)) return false;
    if (!parseFloat(fields[5], wp.param2)) return false;
    if (!parseFloat(fields[6], wp.param3)) return false;
    if (!parseFloat(fields[7], wp.param4)) return false;
    if (!parseDouble(fields[8], wp.lat)) return false;
    if (!parseDouble(fields[9], wp.lon)) return false;
    if (!parseFloat(fields[10], wp.alt)) return false;
    if (!parseUint8(fields[11], wp.autocontinue)) return false;

    // Валидируем координаты
    return validateCoords(wp.lat, wp.lon, wp.alt);
}

bool MissionParser::validateCoords(double lat, double lon, float alt) {
    if (lat < MissionLimits::MIN_LAT || lat > MissionLimits::MAX_LAT) return false;
    if (lon < MissionLimits::MIN_LON || lon > MissionLimits::MAX_LON) return false;
    if (alt < MissionLimits::MIN_ALT || alt > MissionLimits::MAX_ALT) return false;
    return true;
}

// ============================================================================
// Вспомогательные функции
// ============================================================================

bool MissionParser::parseDouble(const char* str, double& value) {
    if (!str || *str == '\0') return false;
    char* end;
    value = strtod(str, &end);
    return (*end == '\0');
}

bool MissionParser::parseFloat(const char* str, float& value) {
    if (!str || *str == '\0') return false;
    char* end;
    value = strtof(str, &end);
    return (*end == '\0');
}

bool MissionParser::parseUint16(const char* str, uint16_t& value) {
    if (!str || *str == '\0') return false;
    char* end;
    long v = strtol(str, &end, 10);
    if (*end != '\0' || v < 0 || v > 65535) return false;
    value = static_cast<uint16_t>(v);
    return true;
}

bool MissionParser::parseUint8(const char* str, uint8_t& value) {
    if (!str || *str == '\0') return false;
    char* end;
    long v = strtol(str, &end, 10);
    if (*end != '\0' || v < 0 || v > 255) return false;
    value = static_cast<uint8_t>(v);
    return true;
}

char* MissionParser::trim(char* str) {
    if (!str) return nullptr;

    // Убираем пробелы с начала
    while (isspace(static_cast<unsigned char>(*str))) str++;
    if (*str == '\0') return str;

    // Убираем пробелы с конца
    char* end = str + strlen(str) - 1;
    while (end > str && isspace(static_cast<unsigned char>(*end))) end--;
    end[1] = '\0';

    return str;
}
