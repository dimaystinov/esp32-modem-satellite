/**
 * @file DisplayUi.cpp
 * @brief Реализация OLED интерфейса
 */

#include "DisplayUi.h"
#include "Config.h"
#include "AppState.h"
#include "WifiManagerEx.h"
#include "MavlinkUploader.h"
#include "Logger.h"
#include "MissionStore.h"
#include <Wire.h>
#include <Arduino.h>

// Глобальный экземпляр
DisplayUi g_display;

// ============================================================================
// Инициализация
// ============================================================================

bool DisplayUi::init(uint8_t sda, uint8_t scl) {
    if (!ENABLE_OLED) return false;
    Wire.begin(sda, scl);
    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() != 0) { Wire.end(); return false; }
    // Инициализируем I2C
    Wire.setPins(sda, scl);

    // Создаем дисплей
    display_ = new U8G2_SSD1306_72X40_ER_F_HW_I2C(
        U8G2_R0, U8X8_PIN_NONE, scl, sda);

    if (!display_) {
        return false;
    }

    if (!display_->begin()) {
        delete display_;
        display_ = nullptr;
        return false;
    }

    display_->clearBuffer();
    display_->setFont(DISPLAY_FONT_SMALL);
    display_->drawStr(0, 12, "MAVLink");
    display_->drawStr(0, 24, "starting...");
    display_->sendBuffer();

    initialized_ = true;
    lastPageSwitch_ = millis();

    return true;
}

void DisplayUi::deinit() {
    if (display_) {
        display_->clearBuffer();
        display_->sendBuffer();
        delete display_;
        display_ = nullptr;
    }
    initialized_ = false;
}

// ============================================================================
// Обновление
// ============================================================================

bool DisplayUi::update() {
    if (!initialized_ || !display_) return false;

    uint32_t now = millis();

    // Проверяем временное сообщение
    if (showingMessage_) {
        if (now < messageUntil_) {
            drawMessage();
            return true;
        } else {
            showingMessage_ = false;
        }
    }

    // Проверяем нужно ли переключить страницу
    if (now - lastPageSwitch_ > pageInterval_) {
        lastPageSwitch_ = now;
        nextPage();
    }

    // Отрисовываем текущую страницу
    switch (currentPage_) {
        case 0:
            drawWiFiPage();
            break;
        case 1:
            drawMavlinkPage();
            break;
        case 2:
            drawProtocolPage();
            break;
        case 3:
            drawMissionPage();
            break;
        default:
            currentPage_ = 0;
            drawWiFiPage();
            break;
    }

    return true;
}

void DisplayUi::forceUpdate() {
    update();
}

void DisplayUi::setPage(uint8_t page) {
    currentPage_ = page % 4;
    lastPageSwitch_ = millis();
    forceUpdate();
}

void DisplayUi::nextPage() {
    currentPage_ = (currentPage_ + 1) % 4;
}

void DisplayUi::showMessage(const char* msg, uint32_t durationMs) {
    if (!msg) return;

    strncpy(message_, msg, sizeof(message_) - 1);
    message_[sizeof(message_) - 1] = '\0';

    messageUntil_ = millis() + durationMs;
    showingMessage_ = true;
}

// ============================================================================
// Отрисовка страниц
// ============================================================================

void DisplayUi::drawWiFiPage() {
    if (!display_) return;

    display_->clearBuffer();
    display_->setFont(DISPLAY_FONT_SMALL);

    // Заголовок
    display_->drawStr(0, 8, "WiFi");

    char buf[32];

    if (g_wifiManager.isConnected()) {
        // SSID
        strncpy(buf, g_wifiManager.getCurrentSSID(), sizeof(buf) - 1);
        buf[15] = '\0'; // Обрезаем
        display_->drawStr(0, 18, buf);

        // IP
        uint32_t ip = g_wifiManager.getIP();
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
                 (int)(ip & 0xFF), (int)((ip >> 8) & 0xFF),
                 (int)((ip >> 16) & 0xFF), (int)((ip >> 24) & 0xFF));
        display_->drawStr(0, 28, buf);

        // RSSI
        int32_t rssi = g_wifiManager.getRSSI();
        snprintf(buf, sizeof(buf), "RSSI:%d", (int)rssi);
        display_->drawStr(0, 38, buf);
    } else {
        display_->drawStr(0, 18, "Not");
        display_->drawStr(0, 28, "connected");

        // Показываем попытки
        if (g_appState.wifiReconnectAttempts > 0) {
            snprintf(buf, sizeof(buf), "Try:%lu", g_appState.wifiReconnectAttempts);
            display_->drawStr(0, 38, buf);
        }
    }

    display_->sendBuffer();
}

void DisplayUi::drawMavlinkPage() {
    if (!display_) return;

    display_->clearBuffer();
    display_->setFont(DISPLAY_FONT_SMALL);

    // Заголовок
    display_->drawStr(0, 8, "MAVLink");

    char buf[32];

    if (g_mavlinkUploader.isConnected()) {
        // ID
        snprintf(buf, sizeof(buf), "ID:%u.%u",
                 g_mavlinkUploader.getTargetSysId(),
                 g_mavlinkUploader.getTargetCompId());
        display_->drawStr(0, 18, buf);

        // Heartbeat count
        snprintf(buf, sizeof(buf), "HB:%lu", g_mavlinkUploader.getHeartbeatCount());
        display_->drawStr(0, 28, buf);

        // Статус загрузки
        switch (g_mavlinkUploader.getState()) {
            case MissionUploadState::IDLE:
                display_->drawStr(0, 38, "Ready");
                break;
            case MissionUploadState::WAIT_REQUEST:
            case MissionUploadState::SENDING:
                snprintf(buf, sizeof(buf), "%u%%", g_mavlinkUploader.getProgress());
                display_->drawStr(0, 38, buf);
                break;
            case MissionUploadState::DONE:
                display_->drawStr(0, 38, "Done");
                break;
            case MissionUploadState::ERROR:
                display_->drawStr(0, 38, "Error");
                break;
        }
    } else {
        display_->drawStr(0, 18, "No");
        display_->drawStr(0, 28, "vehicle");
        display_->drawStr(0, 38, "Waiting...");
    }

    display_->sendBuffer();
}

void DisplayUi::drawProtocolPage() {
    if (!display_) return;

    display_->clearBuffer();
    display_->setFont(DISPLAY_FONT_SMALL);

    // Заголовок
    display_->drawStr(0, 8, "Protocol");

    char buf[32];

    // Состояние приема
    switch (g_appState.protocolRxState) {
        case FrameRxState::WAIT_START:
            display_->drawStr(0, 18, "Wait...");
            break;
        case FrameRxState::WAIT_CMD:
        case FrameRxState::WAIT_SIZE_LO:
        case FrameRxState::WAIT_SIZE_HI:
            display_->drawStr(0, 18, "Header");
            break;
        case FrameRxState::WAIT_PAYLOAD:
            display_->drawStr(0, 18, "RX...");
            break;
        default:
            display_->drawStr(0, 18, "Processing");
            break;
    }

    // CRC статус
    if (g_appState.lastCrcOk) {
        display_->drawStr(0, 28, "CRC:OK");
    } else if (g_appState.lastCrcCalc != 0) {
        display_->drawStr(0, 28, "CRC:ERR");
    } else {
        display_->drawStr(0, 28, "CRC:--");
    }

    // Размер кадра
    snprintf(buf, sizeof(buf), "Len:%lu", g_appState.lastFrameLen);
    display_->drawStr(0, 38, buf);

    display_->sendBuffer();
}

void DisplayUi::drawMissionPage() {
    if (!display_) return;

    display_->clearBuffer();
    display_->setFont(DISPLAY_FONT_SMALL);

    // Заголовок
    display_->drawStr(0, 8, "Mission");

    char buf[32];

    // Количество точек
    snprintf(buf, sizeof(buf), "Points:%u", g_missionStore.getCount());
    display_->drawStr(0, 18, buf);

    // Статус
    if (g_missionStore.getCount() > 0) {
        if (g_mavlinkUploader.getState() == MissionUploadState::SENDING ||
            g_mavlinkUploader.getState() == MissionUploadState::WAIT_REQUEST) {
            snprintf(buf, sizeof(buf), "Upload:%u%%", g_mavlinkUploader.getProgress());
            display_->drawStr(0, 28, buf);
        } else if (g_mavlinkUploader.getState() == MissionUploadState::DONE) {
            display_->drawStr(0, 28, "Uploaded");
        } else {
            display_->drawStr(0, 28, "Ready");
        }

        // Статус MAVLink
        if (g_mavlinkUploader.isConnected()) {
            display_->drawStr(0, 38, "Link:OK");
        } else {
            display_->drawStr(0, 38, "Link:--");
        }
    } else {
        display_->drawStr(0, 28, "No");
        display_->drawStr(0, 38, "mission");
    }

    display_->sendBuffer();
}

void DisplayUi::drawMessage() {
    if (!display_) return;

    display_->clearBuffer();
    display_->setFont(DISPLAY_FONT_SMALL);

    display_->drawStr(0, 8, "Message");

    // Разбиваем сообщение на строки
    char line1[32] = {0};
    char line2[32] = {0};
    char line3[32] = {0};

    size_t len = strlen(message_);
    if (len <= 12) {
        strncpy(line1, message_, 12);
        line1[12] = '\0';
    } else if (len <= 24) {
        strncpy(line1, message_, 12);
        line1[12] = '\0';
        strncpy(line2, message_ + 12, 12);
        line2[12] = '\0';
    } else {
        strncpy(line1, message_, 12);
        line1[12] = '\0';
        strncpy(line2, message_ + 12, 12);
        line2[12] = '\0';
        strncpy(line3, message_ + 24, 12);
        line3[12] = '\0';
    }

    display_->drawStr(0, 18, line1);
    if (line2[0]) display_->drawStr(0, 28, line2);
    if (line3[0]) display_->drawStr(0, 38, line3);

    display_->sendBuffer();
}
