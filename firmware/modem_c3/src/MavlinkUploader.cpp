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
#include <cmath>
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
    activation_ = Activation::Idle;
    activationError_ = "";
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
    activation_ = Activation::Idle; autoMode_ = -1; armed_ = false; customMode_ = 0;
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

                case MAVLINK_MSG_ID_PARAM_VALUE:
                    handleParamValue(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
                    break;
                case MAVLINK_MSG_ID_COMMAND_ACK:
                    handleCommandAck(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
                    break;
                case MAVLINK_MSG_ID_STATUSTEXT:
                    handleStatusText(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
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
    customMode_ = mavlink_msg_heartbeat_get_custom_mode(&msg);
    const uint8_t baseMode = mavlink_msg_heartbeat_get_base_mode(&msg);
    armed_ = (baseMode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
    autoMode_ = -1;
    if (mavlink_msg_heartbeat_get_autopilot(&msg) == MAV_AUTOPILOT_ARDUPILOTMEGA) {
        switch (mavlink_msg_heartbeat_get_type(&msg)) {
            case MAV_TYPE_QUADROTOR: case MAV_TYPE_COAXIAL: case MAV_TYPE_HELICOPTER:
            case MAV_TYPE_HEXAROTOR: case MAV_TYPE_OCTOROTOR: case MAV_TYPE_TRICOPTER:
            case MAV_TYPE_DODECAROTOR: case MAV_TYPE_DECAROTOR:
                autoMode_ = 3; break; // ArduCopter AUTO
            case MAV_TYPE_FIXED_WING: case MAV_TYPE_VTOL_TAILSITTER_DUOROTOR:
            case MAV_TYPE_VTOL_TAILSITTER_QUADROTOR: case MAV_TYPE_VTOL_TILTROTOR:
            case MAV_TYPE_VTOL_FIXEDROTOR: case MAV_TYPE_VTOL_TAILSITTER: case MAV_TYPE_VTOL_TILTWING:
            case MAV_TYPE_GROUND_ROVER: case MAV_TYPE_SURFACE_BOAT:
                autoMode_ = 10; break; // ArduPlane / Rover AUTO
            default: break;
        }
    }
    const bool inAuto = autoMode_ >= 0 && customMode_ == uint32_t(autoMode_) &&
                        (baseMode & MAV_MODE_FLAG_CUSTOM_MODE_ENABLED);
    // A fresh heartbeat AFTER COMMAND_ACK confirms the actual state.
    if (activation_ == Activation::SettingAuto && commandAccepted_ && inAuto) {
        activation_ = Activation::Arming;
        activationAt_ = millis(); commandAccepted_ = false; commandResult_ = -1;
        if (!sendActivationCommand(MAV_CMD_COMPONENT_ARM_DISARM, 1, 0))
            failActivation("ARM UART write failed");
    } else if (activation_ == Activation::Arming) {
        if (!inAuto) failActivation("Vehicle left AUTO during ARM");
        else if (commandAccepted_ && armed_) {
            activation_ = Activation::Complete;
            g_logger.appendStatus("AUTO + ARMED confirmed by heartbeat");
        }
    }


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
    if (msg.sysid != targetSysId_ || msg.compid != targetCompId_ ||
        (req.target_system != 0 && req.target_system != MAVLINK_SYS_ID) ||
        (req.target_component != 0 && req.target_component != MAVLINK_COMP_ID) ||
        req.mission_type != MAV_MISSION_TYPE_MISSION) return;


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
    if (msg.sysid != targetSysId_ || msg.compid != targetCompId_ ||
        (req.target_system != 0 && req.target_system != MAVLINK_SYS_ID) ||
        (req.target_component != 0 && req.target_component != MAVLINK_COMP_ID) ||
        req.mission_type != MAV_MISSION_TYPE_MISSION) return;


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
    if ((ack.target_system != 0 && ack.target_system != MAVLINK_SYS_ID) ||
        (ack.target_component != 0 && ack.target_component != MAVLINK_COMP_ID) ||
        ack.mission_type != MAV_MISSION_TYPE_MISSION) return;

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
        startActivation();
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
    if (activationBusy() || state_ == MissionUploadState::WAIT_REQUEST || state_ == MissionUploadState::SENDING) return false;
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

    activation_ = Activation::Idle; activationError_ = ""; commandResult_ = -1;
    vehicleText_[0] = '\0';
    totalCount_ = waypoints.size();
    sentItems_.assign(totalCount_, false);
    sentCount_ = 0;
    optionsBefore_ = optionsVerified_ = optionsDesired_ = -1;
    state_ = MissionUploadState::IDLE;
    if (autoMode_ == 3) { // Only ArduCopter uses this AUTO_OPTIONS bit for arming.
        activation_ = Activation::ReadingOptions;
        activationAt_ = millis(); optionsReadAttempts_ = 0;
        if (!requestAutoOptions()) { failActivation("AUTO_OPTIONS read UART write failed"); return false; }
        return true;
    }
    return beginMissionTransfer();
}

bool MavlinkUploader::beginMissionTransfer() {
    activation_ = Activation::Idle;
    state_ = MissionUploadState::WAIT_REQUEST;
    uploadStartTime_ = millis();

    if (!sendMissionCount(totalCount_)) {
        state_ = MissionUploadState::ERROR;
        return false;
    }

    return true;
}

void MavlinkUploader::stopUpload() {
    if (activationBusy()) activation_ = Activation::Cancelled;
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
        if (!sentItems_[seq]) { sentItems_[seq] = true; ++sentCount_; }

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
    if (optionsBusy()) {
        if (!isConnected()) failActivation("Heartbeat lost while configuring AUTO_OPTIONS");
        else if (uint32_t(millis()-activationAt_) >= 10000)
            failActivation("AUTO_OPTIONS response/verification timeout");
        else if (activation_ != Activation::WritingOptions && optionsReadAttempts_ < 3 && uint32_t(millis()-optionsReadAt_) >= 2000) {
            if (!requestAutoOptions()) failActivation("AUTO_OPTIONS read UART write failed");
        }
        return;
    }
    if (activationBusy()) {
        if (!isConnected()) failActivation("Heartbeat lost during AUTO/ARM");
        else if (uint32_t(millis()-activationAt_) >= 30000)
            failActivation(activation_ == Activation::SettingAuto ? "AUTO confirmation timeout" : "ARM confirmation timeout");
    }
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


const char* MavlinkUploader::activationName() const {
    switch (activation_) {
        case Activation::ReadingOptions: return "reading_options";
        case Activation::WritingOptions: return "writing_options";
        case Activation::VerifyingOptions: return "verifying_options";
        case Activation::Idle: return "idle";
        case Activation::SettingAuto: return "setting_auto";
        case Activation::Arming: return "arming";
        case Activation::Complete: return "complete";
        case Activation::Error: return "error";
        default: return "cancelled";
    }
}
void MavlinkUploader::failActivation(const char* reason) {
    const bool preparing = optionsBusy();
    activation_ = Activation::Error; activationError_ = reason;
    g_logger.appendStatus(reason);
    if (preparing) { state_ = MissionUploadState::ERROR; if (onComplete_) onComplete_(false); }
}
void MavlinkUploader::startActivation() {
    if (!isConnected()) { failActivation("No fresh heartbeat for AUTO"); return; }
    if (autoMode_ < 0) { failActivation("Unsupported autopilot/vehicle type for AUTO"); return; }
    activation_ = Activation::SettingAuto;
    activationAt_ = millis(); commandAccepted_ = false; commandResult_ = -1;
    activationError_ = ""; vehicleText_[0] = '\0';
    if (!sendActivationCommand(MAV_CMD_DO_SET_MODE, MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, autoMode_))
        failActivation("AUTO UART write failed");
}
bool MavlinkUploader::sendActivationCommand(uint16_t command, float param1, float param2) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_command_long_pack(MAVLINK_SYS_ID, MAVLINK_COMP_ID, &msg,
        targetSysId_, targetCompId_, command, 0, param1, param2, 0, 0, 0, 0, 0);
    const auto len = mavlink_msg_to_send_buffer(buf, &msg);
    g_logger.appendStatusF("Sending command %u to %u/%u", command, targetSysId_, targetCompId_);
    return mavSerial.write(buf, len) == len;
}
void MavlinkUploader::handleCommandAck(const uint8_t* data, size_t) {
    const auto& msg = *reinterpret_cast<const mavlink_message_t*>(data);
    if ((activation_ != Activation::SettingAuto && activation_ != Activation::Arming) || msg.sysid != targetSysId_ || msg.compid != targetCompId_) return;
    mavlink_command_ack_t ack;
    mavlink_msg_command_ack_decode(&msg, &ack);
    const auto expected = activation_ == Activation::SettingAuto ? MAV_CMD_DO_SET_MODE : MAV_CMD_COMPONENT_ARM_DISARM;
    if (ack.command != expected ||
        (ack.target_system != 0 && ack.target_system != MAVLINK_SYS_ID) ||
        (ack.target_component != 0 && ack.target_component != MAVLINK_COMP_ID)) return;
    commandResult_ = ack.result;
    if (ack.result == MAV_RESULT_ACCEPTED) commandAccepted_ = true;
    else if (ack.result != MAV_RESULT_IN_PROGRESS)
        failActivation(activation_ == Activation::SettingAuto ? "AUTO rejected: see COMMAND_ACK result / vehicle text" : "ARM rejected: see COMMAND_ACK result / vehicle text");
}
void MavlinkUploader::handleStatusText(const uint8_t* data, size_t) {
    const auto& msg = *reinterpret_cast<const mavlink_message_t*>(data);
    if (msg.sysid != targetSysId_ || msg.compid != targetCompId_) return;
    mavlink_statustext_t status;
    mavlink_msg_statustext_decode(&msg, &status);
    memcpy(vehicleText_, status.text, 50); vehicleText_[50] = '\0';
}


bool MavlinkUploader::requestAutoOptions() {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    static const char id[16] = "AUTO_OPTIONS";
    mavlink_msg_param_request_read_pack(MAVLINK_SYS_ID, MAVLINK_COMP_ID, &msg,
        targetSysId_, targetCompId_, id, -1);
    const auto len = mavlink_msg_to_send_buffer(buf, &msg);
    optionsReadAt_ = millis(); ++optionsReadAttempts_;
    return mavSerial.write(buf, len) == len;
}
void MavlinkUploader::handleParamValue(const uint8_t* data, size_t) {
    const auto& msg = *reinterpret_cast<const mavlink_message_t*>(data);
    if (!optionsBusy() || msg.sysid != targetSysId_ || msg.compid != targetCompId_) return;
    mavlink_param_value_t param;
    mavlink_msg_param_value_decode(&msg, &param);
    static const char id[16] = "AUTO_OPTIONS";
    if (strncmp(param.param_id, id, sizeof(id)) != 0) return;
    // ArduPilot uses numeric float conversion (not byte-wise encoding).
    // Reject values that cannot preserve every bit when setting bit 0 in float32.
    if ((param.param_type != MAV_PARAM_TYPE_INT8 && param.param_type != MAV_PARAM_TYPE_INT16 && param.param_type != MAV_PARAM_TYPE_INT32) ||
        !std::isfinite(param.param_value) || param.param_value < 0 || param.param_value > 16777215.0f ||
        std::floor(param.param_value) != param.param_value) {
        failActivation("Invalid AUTO_OPTIONS type/value"); return;
    }
    const int32_t value = static_cast<int32_t>(param.param_value);
    if (activation_ == Activation::ReadingOptions) {
        optionsBefore_ = value; optionsType_ = param.param_type;
        if ((value & 1) != 0) {
            optionsVerified_ = value;
            if (!beginMissionTransfer()) { failActivation("MISSION_COUNT UART write failed"); if (onComplete_) onComplete_(false); }
            return;
        }
        if (armed_) { failActivation("Cannot change AUTO_OPTIONS while armed"); return; }
        optionsDesired_ = value | 1; // Preserve takeoff, yaw and all other option bits.
        activation_ = Activation::WritingOptions; activationAt_ = millis();
        mavlink_message_t set;
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        mavlink_msg_param_set_pack(MAVLINK_SYS_ID, MAVLINK_COMP_ID, &set,
            targetSysId_, targetCompId_, id, static_cast<float>(optionsDesired_), optionsType_);
        const auto len = mavlink_msg_to_send_buffer(buf, &set);
        g_logger.appendStatusF("AUTO_OPTIONS: %ld -> %ld", long(value), long(optionsDesired_));
        if (mavSerial.write(buf, len) != len) failActivation("AUTO_OPTIONS set UART write failed");
        return;
    }
    if (value != optionsDesired_ || param.param_type != optionsType_) {
        failActivation("AUTO_OPTIONS write/readback mismatch"); return;
    }
    if (activation_ == Activation::WritingOptions) {
        // PARAM_SET acknowledgement is not enough: explicitly request readback.
        activation_ = Activation::VerifyingOptions; activationAt_ = millis(); optionsReadAttempts_ = 0;
        if (!requestAutoOptions()) failActivation("AUTO_OPTIONS verification UART write failed");
    } else {
        optionsVerified_ = value;
        if (!beginMissionTransfer()) { failActivation("MISSION_COUNT UART write failed"); if (onComplete_) onComplete_(false); }
    }
}
