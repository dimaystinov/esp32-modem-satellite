#pragma once

/**
 * @file MissionStore.h
 * @brief ???????? ?????? ? LittleFS
 *
 * ????????? ????? ????? ?????? ? ???????????? ?????.
 * ?????????? ????????? ?????????? ????? ????????? ????.
 */

#include "MissionTypes.h"
#include "FilePaths.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <FS.h>

class MissionStore {
public:
    bool init();
    void deinit();

    bool saveMission(const char* rawText, const std::vector<Waypoint>& waypoints);
    bool loadLastMission(std::vector<Waypoint>& waypoints);
    int loadRawText(char* buffer, size_t maxLen);
    bool hasMission() const;
    size_t getMissionSize() const;
    bool deleteMission();
    bool backupMission();
    bool restoreBackup();

    const std::vector<Waypoint>& getWaypoints() const { return waypoints_; }
    size_t getCount() const { return waypoints_.size(); }
    void clearCache() { waypoints_.clear(); }

private:
    std::vector<Waypoint> waypoints_;
    bool fsMounted_ = false;

    bool writeFile(const char* path, const uint8_t* data, size_t len);
    bool writeFile(const char* path, const char* text);
    int readFile(const char* path, char* buffer, size_t maxLen);
    bool exists(const char* path) const;
    bool remove(const char* path);
    bool copy(const char* src, const char* dst);
};

extern MissionStore g_missionStore;
