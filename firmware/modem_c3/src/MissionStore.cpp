/**
 * @file MissionStore.cpp
 * @brief ?????????? ???????? ??????
 */

#include "MissionStore.h"
#include "MissionParser.h"
#include "Logger.h"
#include <LittleFS.h>
#include <cstring>

// ?????????? ?????????
MissionStore g_missionStore;

// ============================================================================
// ?????????????
// ============================================================================

bool MissionStore::init() {
    if (fsMounted_) return true;

    if (!LittleFS.begin(false)) {
        if (!LittleFS.begin(true)) {
            return false;
        }
    }

    fsMounted_ = true;

    if (!LittleFS.exists("/logs")) {
        LittleFS.mkdir("/logs");
    }

    return true;
}

void MissionStore::deinit() {
    waypoints_.clear();
    if (fsMounted_) {
        LittleFS.end();
        fsMounted_ = false;
    }
}

// ============================================================================
// ?????? ? ???????
// ============================================================================

bool MissionStore::saveMission(const char* rawText, const std::vector<Waypoint>& waypoints) {
    if (!fsMounted_ || !rawText) return false;

    if (!writeFile(FilePaths::MISSION_TEMP, rawText)) {
        g_logger.appendStatus("Failed to write temp mission file");
        return false;
    }

    // Храним только одну последнюю миссию, без накопления старых копий.
    if (exists(FilePaths::MISSION_BACKUP)) {
        remove(FilePaths::MISSION_BACKUP);
    }

    if (exists(FilePaths::MISSION_CURRENT)) {
        remove(FilePaths::MISSION_CURRENT);
    }

    File tempFile = LittleFS.open(FilePaths::MISSION_TEMP, "r");
    File missionFile = LittleFS.open(FilePaths::MISSION_CURRENT, "w");

    if (!tempFile || !missionFile) {
        if (tempFile) tempFile.close();
        if (missionFile) missionFile.close();
        return false;
    }

    while (tempFile.available()) {
        uint8_t buffer[256];
        size_t len = tempFile.read(buffer, sizeof(buffer));
        if (missionFile.write(buffer, len) != len) {
            tempFile.close(); missionFile.close();
            g_logger.appendStatus("Mission save failed: incomplete write");
            return false;
        }
    }

    tempFile.close();
    missionFile.close();

    remove(FilePaths::MISSION_TEMP);

    waypoints_ = waypoints;

    g_logger.appendStatusF("Mission saved: %d waypoints", waypoints.size());

    return true;
}

bool MissionStore::loadLastMission(std::vector<Waypoint>& waypoints) {
    if (!fsMounted_) return false;

    size_t size = getMissionSize();
    if (size == 0 || size > 65535) return false;

    char* buffer = new char[size + 1];
    if (!buffer) return false;

    int readLen = loadRawText(buffer, size + 1);
    if (readLen <= 0) {
        delete[] buffer;
        return false;
    }

    MissionParser parser;
    MissionParser::ParseResult result = parser.parse(buffer, waypoints);

    delete[] buffer;

    if (result.result == MissionParser::Result::OK) {
        waypoints_ = waypoints;
        g_logger.appendStatusF("Mission loaded: %d waypoints", waypoints.size());
        return true;
    }

    g_logger.appendStatusF("Failed to parse mission: %s",
        parser.getErrorString(result.result));

    return false;
}

int MissionStore::loadRawText(char* buffer, size_t maxLen) {
    if (!fsMounted_ || !buffer || maxLen == 0) return -1;

    return readFile(FilePaths::MISSION_CURRENT, buffer, maxLen);
}

bool MissionStore::hasMission() const {
    if (!fsMounted_) return false;
    return exists(FilePaths::MISSION_CURRENT);
}

size_t MissionStore::getMissionSize() const {
    if (!fsMounted_) return 0;

    File file = LittleFS.open(FilePaths::MISSION_CURRENT, "r");
    if (!file) return 0;

    size_t size = file.size();
    file.close();
    return size;
}

bool MissionStore::deleteMission() {
    if (!fsMounted_) return false;

    bool result = true;
    if (exists(FilePaths::MISSION_CURRENT)) {
        result = remove(FilePaths::MISSION_CURRENT);
    }
    if (exists(FilePaths::MISSION_TEMP)) {
        result = remove(FilePaths::MISSION_TEMP) && result;
    }
    if (exists(FilePaths::MISSION_BACKUP)) {
        result = remove(FilePaths::MISSION_BACKUP) && result;
    }

    if (result) {
        waypoints_.clear();
    }

    return result;
}

bool MissionStore::backupMission() {
    if (!fsMounted_) return false;

    if (!exists(FilePaths::MISSION_CURRENT)) return false;

    return copy(FilePaths::MISSION_CURRENT, FilePaths::MISSION_BACKUP);
}

bool MissionStore::restoreBackup() {
    if (!fsMounted_) return false;

    if (!exists(FilePaths::MISSION_BACKUP)) return false;

    if (exists(FilePaths::MISSION_CURRENT)) {
        remove(FilePaths::MISSION_CURRENT);
    }

    return copy(FilePaths::MISSION_BACKUP, FilePaths::MISSION_CURRENT);
}

// ============================================================================
// ??????????????? ??????
// ============================================================================

bool MissionStore::writeFile(const char* path, const uint8_t* data, size_t len) {
    if (!fsMounted_ || !path || !data) return false;

    File file = LittleFS.open(path, "w");
    if (!file) return false;

    size_t written = file.write(data, len);
    file.close();

    return written == len;
}

bool MissionStore::writeFile(const char* path, const char* text) {
    if (!text) return false;
    return writeFile(path, reinterpret_cast<const uint8_t*>(text), strlen(text));
}

int MissionStore::readFile(const char* path, char* buffer, size_t maxLen) {
    if (!fsMounted_ || !path || !buffer || maxLen == 0) return -1;

    File file = LittleFS.open(path, "r");
    if (!file) return -1;

    size_t toRead = min(file.size(), maxLen - 1);
    size_t read = file.read(reinterpret_cast<uint8_t*>(buffer), toRead);
    buffer[read] = '\0';

    file.close();
    return static_cast<int>(read);
}

bool MissionStore::exists(const char* path) const {
    if (!fsMounted_ || !path) return false;
    return LittleFS.exists(path);
}

bool MissionStore::remove(const char* path) {
    if (!fsMounted_ || !path) return false;
    return LittleFS.remove(path);
}

bool MissionStore::copy(const char* src, const char* dst) {
    if (!fsMounted_ || !src || !dst) return false;

    File srcFile = LittleFS.open(src, "r");
    File dstFile = LittleFS.open(dst, "w");

    if (!srcFile || !dstFile) {
        if (srcFile) srcFile.close();
        if (dstFile) dstFile.close();
        return false;
    }

    uint8_t buffer[256];
    while (srcFile.available()) {
        size_t len = srcFile.read(buffer, sizeof(buffer));
        if (len > 0) {
            dstFile.write(buffer, len);
        }
    }

    srcFile.close();
    dstFile.close();
    return true;
}
