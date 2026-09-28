#pragma once

/**
 * @file DisplayUi.h
 * @brief OLED интерфейс на U8g2
 *
 * Показывает 4 экрана с циклом каждые 10 секунд:
 * 1. WiFi статус и IP
 * 2. MAVLink sysid/compid/heartbeat
 * 3. Протокол RX/CRC/frame
 * 4. Миссия count/upload
 */

#include <cstdint>
#include <cstddef>
#include <U8g2lib.h>
#include "Config.h"

class DisplayUi {
public:
    /**
     * @brief Инициализация дисплея
     * @param sda SDA пин
     * @param scl SCL пин
     * @return true при успехе
     */
    bool init(uint8_t sda = PIN_OLED_SDA, uint8_t scl = PIN_OLED_SCL);

    /**
     * @brief Деинициализация
     */
    void deinit();

    /**
     * @brief Обновление дисплея (вызывать в loop)
     * @return true если экран обновлен
     */
    bool update();

    /**
     * @brief Принудительное обновление
     */
    void forceUpdate();

    /**
     * @brief Установить страницу
     * @param page Номер страницы
     */
    void setPage(uint8_t page);

    /**
     * @brief Следующая страница
     */
    void nextPage();

    /**
     * @brief Показать сообщение временно
     * @param msg Сообщение
     * @param durationMs Длительность в мс
     */
    void showMessage(const char* msg, uint32_t durationMs = 3000);

    /**
     * @brief Проверить инициализирован ли
     */
    bool isInitialized() const { return initialized_; }

private:
    U8G2_SSD1306_72X40_ER_F_HW_I2C* display_ = nullptr;
    bool initialized_ = false;

    uint8_t currentPage_ = 0;
    uint32_t lastPageSwitch_ = 0;
    uint32_t pageInterval_ = 10000; // 10 секунд

    // Для временных сообщений
    char message_[64];
    uint32_t messageUntil_ = 0;
    bool showingMessage_ = false;

    // Отрисовка страниц
    void drawWiFiPage();
    void drawMavlinkPage();
    void drawProtocolPage();
    void drawMissionPage();
    void drawMessage();

    // Вспомогательные
    void drawHeader(const char* title);
    void drawText(int x, int y, const char* text);
};

// Глобальный экземпляр
extern DisplayUi g_display;
