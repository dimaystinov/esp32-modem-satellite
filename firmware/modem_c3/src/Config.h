#pragma once

/**
 * @file Config.h
 * @brief Глобальные константы и настройки проекта ESP32-C3 MAVLink
 *
 * Содержит все пины, таймауты, размеры буферов и пути к файлам.
 */

#include <Arduino.h>

// ============================================================================
// Пины ESP32-C3
// ============================================================================

// Fixed C3 mapping, independent of USB connection.
#define PIN_MAV_RX          3
#define PIN_MAV_TX          10
#define MAV_BAUD_RATE       115200
#define PIN_MAV_RX_DEBUG    PIN_MAV_RX
#define PIN_MAV_TX_DEBUG    PIN_MAV_TX
#define PIN_MAV_RX_WORK     PIN_MAV_RX
#define PIN_MAV_TX_WORK     PIN_MAV_TX
#define PIN_PROTO_RX_WORK   6
#define PIN_PROTO_TX_WORK   7
#define PROTO_BAUD_RATE     115200
// Explicit USB mission input for bench use; default is modem UART1.
#define PROTOCOL_OVER_USB   0
#define PIN_SWITCH          4
#define SWITCH_ACTIVE_HIGH  1
#define ENABLE_OLED         1
#define PIN_OLED_SDA        0
#define PIN_OLED_SCL        1
#define OLED_ADDR           0x3C

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "This project is for ESP32-C3"
#endif
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Enable USB CDC On Boot: UART0 is used by MAVLink"
#endif

// ============================================================================
// Размеры буферов
// ============================================================================

// Буферы для логирования
#define RX_HEX_MAX          4096   // Макс. размер RX hex лога
#define RX_ASCII_MAX        2048   // Макс. размер RX ASCII лога
#define TX_HEX_MAX          4096   // Макс. размер TX hex лога
#define TX_ASCII_MAX        2048   // Макс. размер TX ASCII лога
#define STATUS_MAX          4096    // Макс. размер статус лога

// Протокол
#define PROTO_MAX_PAYLOAD   65535   // Максимальный размер payload (uint16_t)
#define PROTO_FRAME_MAX     (6 + PROTO_MAX_PAYLOAD + 4) // START+CMD+SIZE+PAYLOAD+CRC+STOP

// MAVLink
#define MAVLINK_BUF_SIZE    256     // Размер буфера MAVLink
#define MISSION_MAX_POINTS  500     // Максимум точек миссии в RAM

// HTTP
#define HTTP_PORT           80      // Порт веб-сервера
#define WEB_MAX_RESPONSE    8192    // Макс. размер HTTP ответа

// ============================================================================
// Таймауты (мс)
// ============================================================================

#define MAVLINK_HEARTBEAT_TIMEOUT   5000    // Таймаут heartbeat
#define MISSION_UPLOAD_TIMEOUT        5000    // Таймаут загрузки миссии
#define WIFI_CONNECT_TIMEOUT          30000   // Таймаут подключения WiFi
#define DISPLAY_UPDATE_INTERVAL       10000   // Интервал обновления дисплея
#define FRAME_RX_TIMEOUT              1000    // Таймаут приема кадра

// ============================================================================
// Значения по умолчанию
// ============================================================================

#define DEFAULT_ECHO_ENABLED    false   // Эхо по умолчанию выключено
#define DEFAULT_REFRESH_SEC     20      // Автообновление страницы
#define DEFAULT_DISPLAY_INTERVAL 10     // Интервал смены экранов (сек)
#define DEFAULT_MAV_TIMEOUT     5000    // MAVLink таймаут (мс)

// ============================================================================
// Заголовки протокола
// ============================================================================

#define PROTO_START_BYTE        0x79    // Стартовый байт
#define PROTO_CMD_MISSION       0x01    // Команда загрузки миссии
#define PROTO_STOP_BYTE         0x47    // Стоповый байт

// Коды ответа
#define PROTO_ACK_OK            0x55    // Успешно
#define PROTO_ACK_ERROR         0x14    // Ошибка

// ============================================================================
// CRC-16 IBM-SDLC параметры
// ============================================================================

#define CRC_POLY                0x1021  // Полином
#define CRC_INIT                0xFFFF  // Начальное значение
#define CRC_XOR_OUT             0x0000  // XOR out

// ============================================================================
// Настройки дисплея
// ============================================================================

#define DISPLAY_WIDTH           72      // Ширина OLED
#define DISPLAY_HEIGHT          40      // Высота OLED
#define DISPLAY_FONT_SMALL      u8g2_font_5x8_tr
#define DISPLAY_FONT_NORMAL     u8g2_font_ncenB08_tr

// ============================================================================
// MAVLink параметры
// ============================================================================

#define MAVLINK_SYS_ID          1       // System ID компаньона
#define MAVLINK_COMP_ID         200     // Component ID компаньона

// ============================================================================
// Флаги компиляции
// ============================================================================

// Раскомментируй для отладки
// #define DEBUG_MODE
// #define VERBOSE_LOGGING

#define AP_SSID "modem_bridge"
#define AP_PASSWORD "00000000"
#define LOCAL_DNS_NAME "legion.modem"
#define WIFI_AUTO_OFF_MS 300000UL
