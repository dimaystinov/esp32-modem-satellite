/**
 * @file MavlinkUploader.cpp
 * @brief Реализация MAVLink загрузчика
 */

#include "MavlinkUploader.h"
#include "Config.h"
#include "MissionStore.h"
#include "Logger.h"
#include "AppState.h"
#include <Arduino.h>
#include <MAVLink.h>
#include <HardwareSerial.h>
static HardwareSerial mavSerial(0);

// Глобальный экземпляр
MavlinkUploader g_mavlinkUploader;

// ============================================================================
// Инициализация
// ============================================================================

bool MavlinkUploader::init(uint8_t rxPin, uint8_t txPin, uint32_t baud) {
    rxPin_ = rxPin;
    txPin_ = txPin;
    baud_ = baud;

    // Инициализируем mavSerial для MAVLink
    mavSerial.setRxBufferSize(4096);
    mavSerial.begin(baud_, SERIAL_8N1, rxPin_, txPin_);

    resetState();

    if (g_logger.getStatus()) {
        g_logger.appendStatus("MAVLink initialized");
    }

    return true;
}

void MavlinkUploader::deinit() {
    mavSerial.end();
    resetState();
}

void MavlinkUploader::resetState() {
    state_ = MissionUploadState::IDLE;
    sentCount_ = 0;
    totalCount_ = 0;
    uploadStartTime_ = 0;
    rxLen_ = 0;
}

// ============================================================================
// Основной цикл
// ============================================================================

bool MavlinkUploader::isConnected() const {
    return targetSysId_ != 0 && uint32_t(millis()-lastHeartbeat_) < MAVLINK_HEARTBEAT_TIMEOUT;
}
void MavlinkUploader::clearLink() {
    while (mavSerial.available()) mavSerial.read();
    targetSysId_ = 0; targetCompId_ = 0; lastHeartbeat_ = 0; rxLen_ = 0;
    mavlink_reset_channel_status(MAVLINK_COMM_0);
}
void MavlinkUploader::poll() {
    // Читаем входящие данные
    while (mavSerial.available()) {
        uint8_t c = mavSerial.read();
        ++rxBytes_; lastByteTime_ = millis();

        if (rxLen_ == sizeof(rxBuffer_)) processMavlink();
        if (rxLen_ < sizeof(rxBuffer_)) {
            rxBuffer_[rxLen_++] = c;

            // Проверяем наличие полного сообщения
            if (c == 0xFD || c == 0xFE) { // MAVLink v2 или v1 magic
                // Даем время на прием остального пакета
                delay(1);
            }
        }
    }

    // Обрабатываем пакеты
    if (rxLen_ > 0) {
        processMavlink();
    }

    // Проверяем таймауты
    checkTimeout();

    // Обновляем состояние в AppState
    g_appState.targetSysid = targetSysId_;
    g_appState.targetCompid = targetCompId_;
    g_appState.lastHeartbeat = lastHeartbeat_;
    g_appState.mavlinkConnected = isConnected();
    g_appState.uploadState = state_;
    g_appState.missionItemsSent = sentCount_;
    g_appState.missionItemsTotal = totalCount_;
}

void MavlinkUploader::processMavlink() {
    mavlink_message_t msg;
    mavlink_status_t status;

    for (size_t i = 0; i < rxLen_; i++) {
        uint8_t c = rxBuffer_[i];

        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
            // Сообщение распарсено
            switch (msg.msgid) {
                case MAVLINK_MSG_ID_HEARTBEAT:
                    handleHeartbeat(reinterpret_cast<const uint8_t*>(&msg), MAVLINK_MAX_PACKET_LEN);
                    break;

                case MAVLINK_MSG_ID_MISSION_REQUEST:
                    handleMissionRequest(reinterpret_cast<const uint8_t*>(&msg), MAVLINK_MAX_PACKET_LEN);
                    break;

                case MAVLINK_MSG_ID_MISSION_REQUEST_INT:
                    handleMissionRequestInt(reinterpret_cast<const uint8_t*>(&msg), MAVLINK_MAX_PACKET_LEN);
                    break;

                case MAVLINK_MSG_ID_MISSION_ACK:
                    handleMissionAck(reinterpret_cast<const uint8_t*>(&msg), MAVLINK_MAX_PACKET_LEN);
                    break;

                default:
                    break;
            }
        }
    }

    rxLen_ = 0;
}

// ============================================================================
// Обработчики сообщений
// ============================================================================

void MavlinkUploader::handleHeartbeat(const uint8_t* data, size_t len) {
    mavlink_message_t msg;
    memcpy(&msg, data, sizeof(msg));

    if (mavlink_msg_heartbeat_get_autopilot(&msg) == MAV_AUTOPILOT_INVALID ||
        mavlink_msg_heartbeat_get_type(&msg) == MAV_TYPE_GCS) return;
    if (targetSysId_ != 0 && (msg.sysid != targetSysId_ || msg.compid != targetCompId_)) return;
    lastHeartbeat_ = millis();
    heartbeatCount_++;

    // Запоминаем sysid/compid при первом heartbeat
    if (targetSysId_ == 0) {
        targetSysId_ = msg.sysid;
        targetCompId_ = msg.compid;

        if (g_logger.getStatus()) {
            g_logger.appendStatusF("Heartbeat from sys=%u comp=%u type=%u",
                                  targetSysId_, targetCompId_,
                                  mavlink_msg_heartbeat_get_type(&msg));
        }

        // Обновляем AppState
        g_appState.updateMavlink(targetSysId_, targetCompId_, lastHeartbeat_);
    }
}

void MavlinkUploader::handleMissionRequest(const uint8_t* data, size_t len) {
    mavlink_message_t msg;
    memcpy(&msg, data, sizeof(msg));

    mavlink_mission_request_t req;
    mavlink_msg_mission_request_decode(&msg, &req);

    if (g_logger.getStatus()) {
        g_logger.appendStatusF("Mission request: seq=%u", req.seq);
    }

    if (state_ == MissionUploadState::WAIT_REQUEST ||
        state_ == MissionUploadState::SENDING) {
        if (sendMissionItem(req.seq)) {
            state_ = MissionUploadState::SENDING;
            uploadStartTime_ = millis();
        } else {
            if (g_logger.getStatus()) {
                g_logger.appendStatusF("Failed to send mission item: %u", req.seq);
            }
            state_ = MissionUploadState::ERROR;
            if (onComplete_) {
                onComplete_(false);
            }
        }
    }
}

void MavlinkUploader::handleMissionRequestInt(const uint8_t* data, size_t len) {
    mavlink_message_t msg;
    memcpy(&msg, data, sizeof(msg));

    mavlink_mission_request_int_t req;
    mavlink_msg_mission_request_int_decode(&msg, &req);

    if (g_logger.getStatus()) {
        g_logger.appendStatusF("Mission request INT: seq=%u", req.seq);
    }

    if (state_ == MissionUploadState::WAIT_REQUEST ||
        state_ == MissionUploadState::SENDING) {
        if (sendMissionItem(req.seq)) {
            state_ = MissionUploadState::SENDING;
            uploadStartTime_ = millis();
        } else {
            if (g_logger.getStatus()) {
                g_logger.appendStatusF("Failed to send mission item INT: %u", req.seq);
            }
            state_ = MissionUploadState::ERROR;
            if (onComplete_) {
                onComplete_(false);
            }
        }
    }
}

void MavlinkUploader::handleMissionAck(const uint8_t* data, size_t len) {
    mavlink_message_t msg;
    memcpy(&msg, data, sizeof(msg));

    if ((state_ != MissionUploadState::WAIT_REQUEST && state_ != MissionUploadState::SENDING) ||
        msg.sysid != targetSysId_ || msg.compid != targetCompId_) return;
    mavlink_mission_ack_t ack;
    mavlink_msg_mission_ack_decode(&msg, &ack);

    if (g_logger.getStatus()) {
        g_logger.appendStatusF("Mission ACK: type=%u", ack.type);
    }

    if (ack.type == MAV_MISSION_ACCEPTED && sentCount_ != totalCount_) return;
    if (ack.type == MAV_MISSION_ACCEPTED) {
        state_ = MissionUploadState::DONE;
        uploadStartTime_ = 0;
        if (onComplete_) {
            onComplete_(true);
        }
    } else {
        state_ = MissionUploadState::ERROR;
        uploadStartTime_ = 0;
        if (onComplete_) {
            onComplete_(false);
        }
    }
}

// ============================================================================
// Отправка сообщений
// ============================================================================

bool MavlinkUploader::startUpload() {
    if (!isConnected()) {
        if (g_logger.getStatus()) {
            g_logger.appendStatus("Cannot upload: no vehicle detected");
        }
        return false;
    }

    const auto& waypoints = g_missionStore.getWaypoints();
    if (waypoints.empty()) {
        if (g_logger.getStatus()) {
            g_logger.appendStatus("Cannot upload: no mission loaded");
        }
        return false;
    }

    totalCount_ = waypoints.size();
    sentCount_ = 0;
    state_ = MissionUploadState::WAIT_REQUEST;
    uploadStartTime_ = millis();

    if (!sendMissionCount(totalCount_)) {
        state_ = MissionUploadState::ERROR;
        return false;
    }

    return true;
}

void MavlinkUploader::stopUpload() {
    state_ = MissionUploadState::IDLE;
    sentCount_ = 0;
}

bool MavlinkUploader::sendMissionCount(uint16_t count) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

  mavlink_msg_mission_count_pack(
    MAVLINK_SYS_ID, MAVLINK_COMP_ID,
    &msg,
    targetSysId_, targetCompId_,
    count,
    MAV_MISSION_TYPE_MISSION,
    0
  );

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    size_t sent = mavSerial.write(buf, len);

    if (g_logger.getStatus()) {
        g_logger.appendStatusF("Sent MISSION_COUNT: %u", count);
    }

    return sent == len;
}

bool MavlinkUploader::sendMissionItem(uint16_t seq) {
    const auto& waypoints = g_missionStore.getWaypoints();
    if (seq >= waypoints.size()) {
        return false;
    }

    const Waypoint& wp = waypoints[seq];

    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_mission_item_int_pack(
        MAVLINK_SYS_ID, MAVLINK_COMP_ID,
        &msg,
        targetSysId_, targetCompId_,
        wp.seq,
        wp.frame,
        wp.command,
        wp.current,
        wp.autocontinue,
        wp.param1, wp.param2, wp.param3, wp.param4,
        (int32_t)(wp.lat * 1e7),
        (int32_t)(wp.lon * 1e7),
        wp.alt,
        MAV_MISSION_TYPE_MISSION
    );

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    size_t sent = mavSerial.write(buf, len);

    if (sent == len) {
        sentCount_ = seq + 1;

        if (g_logger.getStatus()) {
            g_logger.appendStatusF("Sent item %u/%u", sentCount_, totalCount_);
        }

        return true;
    }

    return false;
}

// ============================================================================
// Таймауты и прогресс
// ============================================================================

void MavlinkUploader::checkTimeout() {
    if (state_ == MissionUploadState::IDLE ||
        state_ == MissionUploadState::DONE ||
        state_ == MissionUploadState::ERROR) {
        return;
    }

    if (millis() - uploadStartTime_ > MISSION_UPLOAD_TIMEOUT) {
        if (g_logger.getStatus()) {
            g_logger.appendStatus("Mission upload timeout");
        }

        state_ = MissionUploadState::ERROR;
        uploadStartTime_ = 0;

        if (onComplete_) {
            onComplete_(false);
        }
    }
}

uint8_t MavlinkUploader::getProgress() const {
    if (totalCount_ == 0) return 0;
    if (state_ == MissionUploadState::DONE) return 100;
    return (sentCount_ * 100) / totalCount_;
}
