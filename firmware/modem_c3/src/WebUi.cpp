/**
 * @file WebUi.cpp
 * @brief Реализация HTTP сервера
 */

#include "WebUi.h"
#include "Config.h"
#include "FilePaths.h"
#include "AppState.h"
#include "Logger.h"
#include "WifiManagerEx.h"
#include "MavlinkUploader.h"
#include "MissionStore.h"
#include "SettingsStore.h"
#include "FrameReceiver.h"
#include "WebAssets.h"
#include "SwitchControl.h"
#include "PowerWorkflow.h"
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstring>

// Глобальный экземпляр
WebUi g_webUi;

namespace {
bool writeTextFile(const char* path, const char* content) {
    if (!path || !content) return false;
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    const size_t len = strlen(content);
    const size_t written = f.write(reinterpret_cast<const uint8_t*>(content), len);
    f.close();
    return written == len;
}

bool needsUpdate(const char* path, const char* content) {
    if (!LittleFS.exists(path)) return true;
    File f = LittleFS.open(path, "r");
    if (!f) return true;
    if (f.size() != strlen(content)) { f.close(); return true; }
    size_t pos = 0;
    while (f.available()) {
        if (f.read() != static_cast<uint8_t>(content[pos++])) { f.close(); return true; }
    }
    f.close();
    return false;
}

void ensureWebUiFiles() {
    if (needsUpdate(FilePaths::WEB_INDEX, WebAssets::INDEX_HTML)) {
        writeTextFile(FilePaths::WEB_INDEX, WebAssets::INDEX_HTML);
    }
    if (needsUpdate(FilePaths::WEB_STYLE, WebAssets::STYLE_CSS)) {
        writeTextFile(FilePaths::WEB_STYLE, WebAssets::STYLE_CSS);
    }
    if (needsUpdate(FilePaths::WEB_APP, WebAssets::APP_JS)) {
        writeTextFile(FilePaths::WEB_APP, WebAssets::APP_JS);
    }
}
}  // namespace

static const char* missionUploadStateToText(MissionUploadState state) {
    switch (state) {
        case MissionUploadState::IDLE: return "Idle";
        case MissionUploadState::WAIT_REQUEST: return "WaitRequest";
        case MissionUploadState::SENDING: return "Sending";
        case MissionUploadState::DONE: return "Done";
        case MissionUploadState::ERROR: return "Error";
        default: return "Unknown";
    }
}

static const char* rxStateToText(FrameRxState state) {
    switch (state) {
        case FrameRxState::WAIT_START: return "WAIT_START";
        case FrameRxState::WAIT_CMD: return "WAIT_CMD";
        case FrameRxState::WAIT_SIZE_LO: return "WAIT_SIZE_LO";
        case FrameRxState::WAIT_SIZE_HI: return "WAIT_SIZE_HI";
        case FrameRxState::WAIT_PAYLOAD: return "WAIT_PAYLOAD";
        case FrameRxState::WAIT_CRC_LO: return "WAIT_CRC_LO";
        case FrameRxState::WAIT_CRC_HI: return "WAIT_CRC_HI";
        case FrameRxState::WAIT_STOP: return "WAIT_STOP";
        default: return "UNKNOWN";
    }
}

bool WebUi::init(uint16_t port) {
    port_ = port;

    // Автоматически разворачиваем встроенный UI в LittleFS при первом запуске.
    if (LittleFS.begin(false) || LittleFS.begin(true)) {
        ensureWebUiFiles();
    }

    server_ = new WebServer(port_);
    if (!server_) {
        return false;
    }

    WebServer* srv = static_cast<WebServer*>(server_);

    srv->on("/", HTTP_GET, [this]() { handleRoot(); });
    srv->on("/index.html", HTTP_GET, [this]() { handleRoot(); });
    srv->on("/style.css", HTTP_GET, [this]() { handleStaticFile(FilePaths::WEB_STYLE); });
    srv->on("/app.js", HTTP_GET, [this]() { handleStaticFile(FilePaths::WEB_APP); });

    srv->on("/api/switch", HTTP_POST, [this]() { handleSwitch(); });
    srv->on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    srv->on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    srv->on("/api/echo", HTTP_POST, [this]() { handleEcho(); });
    srv->on("/api/logs/clear", HTTP_POST, [this]() { handleLogsClear(); });
    srv->on("/api/upload/start", HTTP_POST, [this]() { handleUploadStart(); });
    srv->on("/api/upload/stop", HTTP_POST, [this]() { handleUploadStop(); });
    srv->on("/api/mission", HTTP_GET, [this]() { handleMission(); });
    srv->on("/api/mission/clear", HTTP_POST, [this]() { handleMissionClear(); });
    srv->on("/api/mission/raw", HTTP_GET, [this]() { handleMissionRaw(); });
    srv->on("/api/wifi", HTTP_GET, [this]() { handleWifiProfiles(); });
    srv->on("/api/config", HTTP_GET, [this]() { handleConfig(); });
    srv->onNotFound([this]() { handleNotFound(); });

    srv->begin();

    if (g_logger.getStatus()) {
        g_logger.appendStatusF("HTTP server started on port %d", port_);
    }

    return true;
}

void WebUi::deinit() {
    if (server_) {
        WebServer* srv = static_cast<WebServer*>(server_);
        srv->close();
        delete srv;
        server_ = nullptr;
    }
}

void WebUi::loop() {
    if (server_) {
        WebServer* srv = static_cast<WebServer*>(server_);
        srv->handleClient();
    }
}

void WebUi::handleRoot() {
    handleStaticFile(FilePaths::WEB_INDEX);
}

void WebUi::handleStaticFile(const char* path) {
    WebServer* srv = static_cast<WebServer*>(server_);

    File file = LittleFS.open(path, "r");
    if (!file) {
        if (strcmp(path, FilePaths::WEB_INDEX) == 0) {
            srv->send(200, "text/html; charset=utf-8",
                      "<!doctype html><html><head><meta charset='utf-8'>"
                      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                      "<title>MAVLink Console</title>"
                      "<style>body{font-family:Segoe UI,Tahoma,sans-serif;background:#dfe6ef;"
                      "margin:0;padding:16px;color:#1f2933}.w{max-width:900px;margin:0 auto;"
                      "background:#f7f9fc;border:1px solid #b8c4d2;padding:16px}"
                      "pre{background:#fff;border:1px solid #c0cbd9;padding:10px;white-space:pre-wrap}"
                      "button{padding:8px 12px}</style></head><body><div class='w'>"
                      "<h2>MAVLink Mission Console</h2>"
                      "<p>LittleFS static files not found. UI fallback is active.</p>"
                      "<p>Upload <code>main/data</code> to LittleFS for full interface.</p>"
                      "<button onclick='load()'>Refresh Status</button>"
                      "<pre id='out'>Loading...</pre>"
                      "<script>async function load(){try{const r=await fetch('/api/status');"
                      "const j=await r.json();document.getElementById('out').textContent="
                      "JSON.stringify(j,null,2);}catch(e){document.getElementById('out').textContent=e.message;}}"
                      "load();setInterval(load,2000);</script></div></body></html>");
            return;
        }

        if (strcmp(path, FilePaths::WEB_STYLE) == 0) {
            srv->send(200, "text/css", "");
            return;
        }

        if (strcmp(path, FilePaths::WEB_APP) == 0) {
            srv->send(200, "application/javascript", "");
            return;
        }

        sendError("Static file not found", 404);
        return;
    }

    srv->streamFile(file, getContentType(path));
    file.close();
}

void WebUi::handleSwitch() {
    sendError("GPIO4 is automatic: send a valid mission through the modem", 409);
}

void WebUi::handleStatus() {
    WebServer* srv = static_cast<WebServer*>(server_);

    StaticJsonDocument<5120> doc;

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["connected"] = g_wifiManager.isConnected();
    wifi["mode"] = "AP";
    wifi["clients"] = g_wifiManager.clients();
    wifi["dns_name"] = LOCAL_DNS_NAME;
    wifi["dns_enabled"] = g_wifiManager.dnsEnabled();
    wifi["remaining_ms"] = g_wifiManager.remainingMs();
    wifi["auto_off_ms"] = WIFI_AUTO_OFF_MS;
    wifi["ssid"] = g_wifiManager.getCurrentSSID();

    char ipStr[16];
    g_appState.getIPString(ipStr, sizeof(ipStr));
    wifi["ip"] = ipStr;

    JsonObject mavlink = doc.createNestedObject("mavlink");
    mavlink["connected"] = g_mavlinkUploader.isConnected();
    mavlink["sysid"] = g_mavlinkUploader.getTargetSysId();
    mavlink["compid"] = g_mavlinkUploader.getTargetCompId();
    mavlink["rxBytes"] = g_mavlinkUploader.rxBytes();
    mavlink["lastByteAgeMs"] = g_mavlinkUploader.rxBytes() ? uint32_t(millis()-g_mavlinkUploader.lastByteTime()) : 0;
    mavlink["heartbeatCount"] = g_mavlinkUploader.getHeartbeatCount();

    mavlink["heartbeatAgeMs"] = g_mavlinkUploader.getHeartbeatCount() ? uint32_t(millis()-g_mavlinkUploader.getLastHeartbeat()) : 0;
    mavlink["customMode"] = g_mavlinkUploader.customMode();
    mavlink["autoMode"] = g_mavlinkUploader.autoMode();
    mavlink["armed"] = g_mavlinkUploader.armed();
    JsonObject activation = doc.createNestedObject("activation");
    activation["stage"] = g_mavlinkUploader.activationName();
    activation["busy"] = g_mavlinkUploader.activationBusy();
    activation["error"] = g_mavlinkUploader.activationError();
    activation["commandResult"] = g_mavlinkUploader.commandResult();
    activation["vehicleText"] = g_mavlinkUploader.vehicleText();
    JsonObject sequence = doc.createNestedObject("sequence");
    sequence["stage"] = g_power.name();
    sequence["acceptedThisBoot"] = g_power.accepted;
    sequence["acceptedAtMs"] = g_power.acceptedAt;
    sequence["powerOnAtMs"] = g_power.powerAt;
    sequence["poweredForMs"] = g_switch.enabled() ? uint32_t(millis()-g_power.powerAt) : 0;
    sequence["waitingForHeartbeat"] = g_power.stage == PowerWorkflow::Stage::WaitingHeartbeat;
    sequence["waitingLong"] = g_power.stage == PowerWorkflow::Stage::WaitingHeartbeat && uint32_t(millis()-g_power.acceptedAt)>30000;
    sequence["powerFeedbackAvailable"] = false;
    doc["uptime_ms"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    JsonObject mission = doc.createNestedObject("mission");
    mission["count"] = g_missionStore.getCount();
    mission["ready"] = g_missionStore.getCount() > 0;
    mission["loaded"] = g_missionStore.hasMission();
    mission["size"] = g_missionStore.getMissionSize();
    mission["state"] = missionUploadStateToText(g_mavlinkUploader.getState());

    JsonObject upload = doc.createNestedObject("upload");
    upload["state"] = static_cast<int>(g_mavlinkUploader.getState());
    upload["stateText"] = missionUploadStateToText(g_mavlinkUploader.getState());
    upload["progress"] = g_mavlinkUploader.getProgress();
    upload["sent"] = g_mavlinkUploader.getSentCount();
    upload["total"] = g_mavlinkUploader.getTotalCount();

    JsonObject protocol = doc.createNestedObject("protocol");
    protocol["rxState"] = static_cast<int>(g_frameReceiver.getState());
    protocol["rxStateText"] = rxStateToText(g_frameReceiver.getState());
    protocol["lastCrcOk"] = g_appState.lastCrcOk;
    protocol["lastFrameLen"] = g_appState.lastFrameLen;
    protocol["frameInProgress"] = g_appState.frameInProgress;

    JsonObject logs = doc.createNestedObject("logs");
    logs["totalRx"] = g_logger.getTotalRxBytes();
    logs["totalTx"] = g_logger.getTotalTxBytes();
    logs["rxHexLen"] = g_logger.getRxHexLen();
    logs["rxAsciiLen"] = g_logger.getRxAsciiLen();
    logs["txHexLen"] = g_logger.getTxHexLen();
    logs["txAsciiLen"] = g_logger.getTxAsciiLen();

    JsonObject sw = doc.createNestedObject("switch");
    sw["gpio"] = PIN_SWITCH;
    sw["enabled"] = g_switch.enabled();
    sw["level"] = g_switch.level();
    sw["activeHigh"] = bool(SWITCH_ACTIVE_HIGH);
    JsonObject settings = doc.createNestedObject("settings");
    settings["echo"] = g_appState.echoEnabled;

    JsonObject runtime = doc.createNestedObject("runtime");
    runtime["mode"] = g_appState.debugMode ? "debug" : "work";
    runtime["usbSerialConnected"] = g_appState.usbSerialConnected;
    runtime["protocolTransport"] = g_appState.debugMode ? "usb_serial" : "uart1";
    runtime["protocolRxPin"] = g_appState.protocolRxPin;
    runtime["protocolTxPin"] = g_appState.protocolTxPin;
    runtime["mavlinkTransport"] = "uart0";
    runtime["mavRxPin"] = g_appState.mavRxPin;
    runtime["mavTxPin"] = g_appState.mavTxPin;

    String response;
    serializeJson(doc, response);
    srv->send(200, "application/json", response);
}

void WebUi::handleLogs() {
    WebServer* srv = static_cast<WebServer*>(server_);

    srv->setContentLength(CONTENT_LENGTH_UNKNOWN);
    srv->send(200, "text/plain; charset=utf-8", "");

    char line[192];
    snprintf(line, sizeof(line),
             "RX bytes=%lu, TX bytes=%lu\n"
             "RX hex len=%lu, RX ascii len=%lu\n"
             "TX hex len=%lu, TX ascii len=%lu\n\n",
             static_cast<unsigned long>(g_logger.getTotalRxBytes()),
             static_cast<unsigned long>(g_logger.getTotalTxBytes()),
             static_cast<unsigned long>(g_logger.getRxHexLen()),
             static_cast<unsigned long>(g_logger.getRxAsciiLen()),
             static_cast<unsigned long>(g_logger.getTxHexLen()),
             static_cast<unsigned long>(g_logger.getTxAsciiLen()));

    srv->sendContent("===== STATUS LOG =====\n");
    srv->sendContent(g_logger.getStatus() ? g_logger.getStatus() : "");
    srv->sendContent("\n");

    srv->sendContent("===== RX ASCII =====\n");
    srv->sendContent(g_logger.getRxAscii() ? g_logger.getRxAscii() : "");
    srv->sendContent("\n\n");

    srv->sendContent("===== RX HEX =====\n");
    srv->sendContent(g_logger.getRxHex() ? g_logger.getRxHex() : "");
    srv->sendContent("\n\n");

    srv->sendContent("===== TX ASCII =====\n");
    srv->sendContent(g_logger.getTxAscii() ? g_logger.getTxAscii() : "");
    srv->sendContent("\n\n");

    srv->sendContent("===== TX HEX =====\n");
    srv->sendContent(g_logger.getTxHex() ? g_logger.getTxHex() : "");
    srv->sendContent("\n\n");

    srv->sendContent("===== COUNTERS =====\n");
    srv->sendContent(line);
    srv->sendContent("");
}

void WebUi::handleEcho() {
    WebServer* srv = static_cast<WebServer*>(server_);

    if (srv->hasArg("set")) {
        int val = srv->arg("set").toInt();
        g_appState.echoEnabled = (val != 0);
        g_settings.setEchoEnabled(g_appState.echoEnabled);

        if (g_logger.getStatus()) {
            g_logger.appendStatusF("Echo %s", g_appState.echoEnabled ? "ON" : "OFF");
        }

        sendOk();
    } else {
        sendError("Missing 'set' parameter");
    }
}

void WebUi::handleLogsClear() {
    g_logger.clearAll();
    sendOk();
}

void WebUi::handleUploadStart() {
    if (!g_power.accepted || !g_switch.enabled()) { sendError("Waiting for a new valid modem mission",409); return; }
    if (g_power.stage == PowerWorkflow::Stage::Uploading || g_mavlinkUploader.activationBusy()) { sendError("Upload already running",409); return; }
    if (g_missionStore.getCount() == 0) {
        sendError("No mission loaded");
        return;
    }

    if (!g_mavlinkUploader.isConnected()) {
        sendError("No MAVLink connection");
        return;
    }

    if (g_mavlinkUploader.startUpload()) {
        g_power.started(true);
        sendOk();
    } else {
        sendError("Failed to start upload");
    }
}

void WebUi::handleUploadStop() {
    g_power.cancel();
    g_mavlinkUploader.stopUpload();
    sendOk();
}

void WebUi::handleMission() {
    WebServer* srv = static_cast<WebServer*>(server_);

    const auto& waypoints = g_missionStore.getWaypoints();
    const size_t totalCount = waypoints.size();
    const size_t shownCount = totalCount > 64 ? 64 : totalCount;

    const size_t capacity =
        JSON_OBJECT_SIZE(8) +
        JSON_ARRAY_SIZE(shownCount) +
        shownCount * JSON_OBJECT_SIZE(12) + 1024;
    DynamicJsonDocument doc(capacity);

    doc["hasMission"] = g_missionStore.hasMission();
    doc["size"] = g_missionStore.getMissionSize();
    doc["totalCount"] = totalCount;
    doc["shownCount"] = shownCount;
    doc["truncated"] = (shownCount < totalCount);
    doc["uploadState"] = missionUploadStateToText(g_mavlinkUploader.getState());
    doc["uploadSent"] = g_mavlinkUploader.getSentCount();
    doc["uploadTotal"] = g_mavlinkUploader.getTotalCount();

    JsonArray arr = doc.createNestedArray("waypoints");
    for (size_t i = 0; i < shownCount; i++) {
        const Waypoint& wp = waypoints[i];
        JsonObject o = arr.createNestedObject();
        o["seq"] = wp.seq;
        o["current"] = wp.current;
        o["frame"] = wp.frame;
        o["command"] = wp.command;
        o["param1"] = wp.param1;
        o["param2"] = wp.param2;
        o["param3"] = wp.param3;
        o["param4"] = wp.param4;
        o["lat"] = wp.lat;
        o["lon"] = wp.lon;
        o["alt"] = wp.alt;
        o["autocontinue"] = wp.autocontinue;
    }

    String response;
    serializeJson(doc, response);
    srv->send(200, "application/json", response);
}

void WebUi::handleMissionClear() {
    g_power.cancel();
    g_mavlinkUploader.stopUpload();

    if (g_missionStore.deleteMission()) {
        g_appState.missionReady = false;
        g_appState.missionCount = 0;
        g_logger.appendStatus("Mission cleared");
        sendOk();
    } else {
        sendError("Failed to clear mission");
    }
}

void WebUi::handleMissionRaw() {
    if (!g_missionStore.hasMission()) {
        sendError("No mission file", 404);
        return;
    }

    size_t size = g_missionStore.getMissionSize();
    if (size == 0 || size > 65535) {
        sendError("Invalid mission size");
        return;
    }

    char* buffer = new char[size + 1];
    if (!buffer) {
        sendError("No memory");
        return;
    }

    int len = g_missionStore.loadRawText(buffer, size + 1);
    if (len <= 0) {
        delete[] buffer;
        sendError("Failed to read mission");
        return;
    }

    String hex;
    hex.reserve(static_cast<unsigned int>(len * 2));
    static const char* lut = "0123456789ABCDEF";
    for (int i = 0; i < len; i++) {
        const uint8_t b = static_cast<uint8_t>(buffer[i]);
        hex += lut[(b >> 4) & 0x0F];
        hex += lut[b & 0x0F];
    }

    DynamicJsonDocument doc(4096 + static_cast<size_t>(len) * 3);
    doc["ascii"] = buffer;
    doc["bytesHex"] = hex;
    doc["bytesLen"] = len;
    doc["uploadState"] = missionUploadStateToText(g_mavlinkUploader.getState());
    doc["uploadSent"] = g_mavlinkUploader.getSentCount();
    doc["uploadTotal"] = g_mavlinkUploader.getTotalCount();

    WebServer* srv = static_cast<WebServer*>(server_);
    String response;
    serializeJson(doc, response);
    srv->send(200, "application/json", response);
    delete[] buffer;
}

void WebUi::handleWifiProfiles() {
    WebServer* srv = static_cast<WebServer*>(server_);

    StaticJsonDocument<2048> doc;
    JsonArray profiles = doc.createNestedArray("profiles");

    for (size_t i = 0; i < g_wifiManager.getProfileCount(); i++) {
        const auto& profile = g_wifiManager.getProfiles()[i];
        JsonObject obj = profiles.createNestedObject();
        obj["ssid"] = profile.ssid;
        obj["priority"] = profile.priority;
    }

    doc["active"] = g_wifiManager.getActiveProfile();

    String response;
    serializeJson(doc, response);
    srv->send(200, "application/json", response);
}

void WebUi::handleConfig() {
    WebServer* srv = static_cast<WebServer*>(server_);

    StaticJsonDocument<1024> doc;
    doc["echo"] = g_settings.getEchoEnabled();
    doc["refreshSec"] = g_settings.getRefreshSec();
    doc["mavTimeout"] = g_settings.getMavlinkTimeout();
    doc["autoUpload"] = g_settings.getAutoUpload();
    doc["displayInterval"] = g_settings.getDisplayInterval();

    String response;
    serializeJson(doc, response);
    srv->send(200, "application/json", response);
}

void WebUi::handleNotFound() {
    WebServer* srv = static_cast<WebServer*>(server_);
    srv->send(404, "text/plain", "Not Found");
}

void WebUi::sendJson(const char* json) {
    WebServer* srv = static_cast<WebServer*>(server_);
    srv->send(200, "application/json", json);
}

void WebUi::sendError(const char* msg, int code) {
    WebServer* srv = static_cast<WebServer*>(server_);
    StaticJsonDocument<256> doc;
    doc["error"] = msg;
    doc["code"] = code;
    String response;
    serializeJson(doc, response);
    srv->send(code, "application/json", response);
}

void WebUi::sendOk() {
    WebServer* srv = static_cast<WebServer*>(server_);
    srv->send(200, "application/json", R"({"status":"ok"})");
}

const char* WebUi::getContentType(const char* filename) {
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    if (strstr(filename, ".json")) return "application/json";
    if (strstr(filename, ".txt")) return "text/plain";
    return "application/octet-stream";
}
