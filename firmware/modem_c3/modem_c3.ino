/**
 * @file main.ino
 * @brief Главный файл проекта ESP32-C3 MAVLink Controller
 *
 * Модульная архитектура:
 * - main.ino: только orchestration (инициализация и loop)
 * - src/: модули (.h/.cpp)
 * - data/: файлы для LittleFS
 */

#include "src/Config.h"
#include "src/FilePaths.h"
#include "src/MissionTypes.h"
#include "src/AppState.h"
#include "src/Logger.h"
#include "src/ProtocolCrc.h"
#include "src/FrameReceiver.h"
#include "src/MissionParser.h"
#include "src/MissionStore.h"
#include "src/SettingsStore.h"
#include "src/WifiManagerEx.h"
#include "src/MavlinkUploader.h"
#include "src/DisplayUi.h"
#include "src/WebUi.h"
#include "src/SwitchControl.h"
#include "src/PowerWorkflow.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <HardwareSerial.h>

// ============================================================================
// Forward declarations
// ============================================================================

bool onFrameReceived(const uint8_t* payload, size_t len);
void onCrcError(uint16_t expected, uint16_t received);
void onWiFiStateChanged(bool connected, uint32_t ip);
void onUploadComplete(bool success);

static HardwareSerial g_protoSerial(1);
static bool g_debugMode = true;
static bool g_serialLog = false;
static uint8_t g_mavRxPin = PIN_MAV_RX_DEBUG;
static uint8_t g_mavTxPin = PIN_MAV_TX_DEBUG;

// ============================================================================
// Setup
// ============================================================================

void setup() {
    g_switch.begin(); // OFF before filesystem, networking or serial initialization.
    // Инициализация Serial для USB
    Serial.begin(115200);
    delay(100);

    if (g_serialLog) Serial.println("\n\n========================================");
    if (g_serialLog) Serial.println("ESP32-C3 MAVLink Controller");
    if (g_serialLog) Serial.println("========================================\n");

    const bool usbSerialConnected = bool(Serial);
    g_debugMode = PROTOCOL_OVER_USB;
    g_serialLog = usbSerialConnected && !g_debugMode;
    g_mavRxPin = g_debugMode ? PIN_MAV_RX_DEBUG : PIN_MAV_RX_WORK;
    g_mavTxPin = g_debugMode ? PIN_MAV_TX_DEBUG : PIN_MAV_TX_WORK;

    g_appState.usbSerialConnected = usbSerialConnected;
    g_appState.debugMode = g_debugMode;
    g_appState.mavRxPin = g_mavRxPin;
    g_appState.mavTxPin = g_mavTxPin;
    g_appState.protocolRxPin = g_debugMode ? -1 : PIN_PROTO_RX_WORK;
    g_appState.protocolTxPin = g_debugMode ? -1 : PIN_PROTO_TX_WORK;

    // 1. Инициализация Logger
    if (g_serialLog) Serial.print("Initializing Logger... ");
    if (!g_logger.init(RX_HEX_MAX, RX_ASCII_MAX, TX_HEX_MAX, TX_ASCII_MAX, STATUS_MAX)) {
        if (g_serialLog) Serial.println("FAILED");
        while(1) { delay(1000); }
    }
    if (g_serialLog) Serial.println("OK");
    g_logger.appendStatus("Boot");
    g_logger.appendStatus(g_debugMode ? "Runtime mode: DEBUG" : "Runtime mode: WORK");

    // 2. Инициализация LittleFS
    if (g_serialLog) Serial.print("Initializing LittleFS... ");
    if (!LittleFS.begin(true)) {
        if (g_serialLog) Serial.println("FAILED");
        g_logger.appendStatus("LittleFS init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_logger.appendStatus("LittleFS initialized");
    }

    // 3. Инициализация Settings (Preferences)
    if (g_serialLog) Serial.print("Initializing Settings... ");
    if (!g_settings.init()) {
        if (g_serialLog) Serial.println("FAILED");
        g_logger.appendStatus("Settings init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_logger.appendStatus("Settings initialized");

        // Загружаем настройки
        g_appState.echoEnabled = g_settings.getEchoEnabled();

        // Проверяем первый запуск
        if (g_settings.isFirstBoot()) {
            g_logger.appendStatus("First boot detected");
            g_settings.setFirstBoot(false);
        }
    }

    // 4. Инициализация MissionStore
    if (g_serialLog) Serial.print("Initializing MissionStore... ");
    if (!g_missionStore.init()) {
        if (g_serialLog) Serial.println("FAILED");
        g_logger.appendStatus("MissionStore init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_logger.appendStatus("MissionStore initialized");

        // Загружаем последнюю миссию если есть
        std::vector<Waypoint> waypoints;
        if (g_missionStore.loadLastMission(waypoints)) {
            g_appState.missionReady = true;
            g_appState.missionCount = g_missionStore.getCount();
            g_logger.appendStatusF("Loaded mission: %d waypoints", g_appState.missionCount);
        }
    }

    // 5. Инициализация FrameReceiver
    if (g_serialLog) Serial.print("Initializing FrameReceiver... ");
    if (g_debugMode) {
        g_frameReceiver.setIo(&Serial);
    } else {
        g_protoSerial.setRxBufferSize(4096);
        g_protoSerial.begin(PROTO_BAUD_RATE, SERIAL_8N1, PIN_PROTO_RX_WORK, PIN_PROTO_TX_WORK);
        g_frameReceiver.setIo(&g_protoSerial);
    }
    if (!g_frameReceiver.init(PROTO_MAX_PAYLOAD)) {
        if (g_serialLog) Serial.println("FAILED");
        g_logger.appendStatus("FrameReceiver init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_frameReceiver.setOnFrameReceived(onFrameReceived);
        g_frameReceiver.setOnCrcError(onCrcError);
        g_logger.appendStatus("FrameReceiver initialized");
    }

    // 6. Инициализация WiFi
    if (g_serialLog) Serial.print("Initializing WiFi... ");
    g_wifiManager.setOnStateChanged(onWiFiStateChanged);
    if (!g_wifiManager.init()) {
        if (g_serialLog) Serial.println("FAILED");
        g_logger.appendStatus("WiFi init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_wifiManager.connect();
        g_logger.appendStatus("WiFi initialized");
    }

    // 7. Инициализация MAVLink
    if (g_serialLog) Serial.print("Initializing MAVLink... ");
    g_mavlinkUploader.setOnComplete(onUploadComplete);
    if (!g_mavlinkUploader.init(g_mavRxPin, g_mavTxPin, MAV_BAUD_RATE)) {
        if (g_serialLog) Serial.println("FAILED");
        g_logger.appendStatus("MAVLink init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_logger.appendStatus("MAVLink initialized");
    }

    // 8. Инициализация Display
    if (g_serialLog) Serial.print("Initializing Display... ");
    if (!g_display.init(PIN_OLED_SDA, PIN_OLED_SCL)) {
        if (g_serialLog) Serial.println("FAILED (continuing without display)");
        g_logger.appendStatus("Display init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_logger.appendStatus("Display initialized");
    }

    // 9. Инициализация Web Server
    if (g_serialLog) Serial.print("Initializing Web Server... ");
    if (!g_webUi.init(HTTP_PORT)) {
        if (g_serialLog) Serial.println("FAILED");
        g_logger.appendStatus("WebServer init failed");
    } else {
        if (g_serialLog) Serial.println("OK");
        g_logger.appendStatus("WebServer initialized");
    }

    // Сохраняем время загрузки
    g_appState.bootTime = millis();

    // Связываем модули с AppState
    g_appState.wifiManager = &g_wifiManager;
    g_appState.frameReceiver = &g_frameReceiver;
    g_appState.mavlinkUploader = &g_mavlinkUploader;
    g_appState.missionStore = &g_missionStore;
    g_appState.logger = &g_logger;

    if (g_serialLog) Serial.println("\n========================================");
    if (g_serialLog) Serial.println("Setup complete!");
    if (g_serialLog) Serial.println("========================================\n");

    g_logger.appendStatus("Setup complete");
    g_logger.appendStatusF("Echo: %s", g_appState.echoEnabled ? "ON" : "OFF");

    // Показываем стартовое сообщение на дисплее
    if (g_display.isInitialized()) {
        g_display.showMessage("Ready!", 3000);
    }
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    g_appState.usbSerialConnected = bool(Serial);
    // Обновляем uptime
    g_appState.updateUptime(millis());

    // 1. Обработка веб-клиентов
    if (g_wifiManager.isConnected()) g_webUi.loop();

    // 2. Прием кадров протокола
    g_frameReceiver.pollSerial();

    // 3. Обработка MAVLink
    if (g_switch.enabled()) g_mavlinkUploader.poll();
    if (g_power.ready(g_mavlinkUploader.isConnected(), g_mavlinkUploader.getHeartbeatCount())) {
        bool ok = g_mavlinkUploader.startUpload();
        g_power.started(ok);
        g_logger.appendStatus(ok ? "FC heartbeat received; mission upload started" : "Mission upload could not start");
    }

    // 4. Обновление дисплея
    g_display.update();

    // 5. Проверка WiFi
    g_wifiManager.loop();

    // 6. Watchdog / yield
    yield();

    // Небольшая задержка для стабильности
    delay(1);
}

// ============================================================================
// Callbacks
// ============================================================================

bool onFrameReceived(const uint8_t* payload, size_t len) {
    if (g_power.stage == PowerWorkflow::Stage::Uploading || g_mavlinkUploader.activationBusy()) {
        g_logger.appendStatus("New mission rejected: upload in progress");
        return false;
    }
    bool accepted = false;
    g_logger.appendStatusF("Frame received: %u bytes", len);

    // Парсим payload как текст миссии
    MissionParser parser;
    std::vector<Waypoint> waypoints;

    MissionParser::ParseResult result = parser.parse(payload, len, waypoints);

    if (result.result == MissionParser::Result::OK) {
        // Создаем null-terminated копию для сохранения
        char* text = new char[len + 1];
        if (text) {
            memcpy(text, payload, len);
            text[len] = '\0';

            // Сохраняем миссию
            if (g_missionStore.saveMission(text, waypoints)) {
                g_appState.missionReady = true;
                g_appState.missionCount = waypoints.size();
                g_settings.setLastMissionSize(len);

                g_logger.appendStatusF("Mission saved: %d waypoints", waypoints.size());

                // Показываем сообщение на дисплее
                char msg[32];
                snprintf(msg, sizeof(msg), "Mission: %d pts", waypoints.size());
                g_display.showMessage(msg, 3000);

                g_mavlinkUploader.resetActivation();
                g_power.missionSaved(millis(), g_mavlinkUploader.getHeartbeatCount());
                if (!g_switch.enabled()) g_mavlinkUploader.clearLink();
                g_switch.set(true);
                accepted = true;
                g_logger.appendStatus("Mission accepted and stored; GPIO4 HIGH; waiting for FC heartbeat");
            } else {
                g_logger.appendStatus("Failed to save mission");
            }

            delete[] text;
        }
    } else {
        g_logger.appendStatusF("Parse error: %s", parser.getErrorString(result.result));
        g_display.showMessage("Parse error!", 3000);
    }
    return accepted;
}

void onCrcError(uint16_t expected, uint16_t received) {
    g_logger.appendStatusF("CRC error: expected=%04X received=%04X", expected, received);
    g_display.showMessage("CRC Error!", 2000);
}

void onWiFiStateChanged(bool connected, uint32_t ip) {
    if (connected) {
        char ipStr[16];
        g_appState.getIPString(ipStr, sizeof(ipStr));
        g_logger.appendStatusF("WiFi connected: %s", ipStr);
    } else {
        g_logger.appendStatus("WiFi disconnected");
    }
}

void onUploadComplete(bool success) {
    g_power.complete(success);
    if (success) {
        g_logger.appendStatus("Mission upload complete");
        g_display.showMessage("Upload OK!", 3000);
    } else {
        g_logger.appendStatus("Mission upload failed");
        g_display.showMessage("Upload Failed!", 3000);
    }
}
