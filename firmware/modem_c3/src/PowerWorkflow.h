#pragma once
#include <stdint.h>
class PowerWorkflow {
public:
    enum class Stage { WaitingMission, WaitingHeartbeat, Uploading, Complete, Error, Cancelled };
    Stage stage = Stage::WaitingMission;
    bool accepted = false;
    uint32_t acceptedAt = 0, powerAt = 0;
    uint32_t baselineHeartbeats = 0;
    void missionSaved(uint32_t now, uint32_t heartbeats) {
        if (!accepted) powerAt = now;
        accepted = true; acceptedAt = now; baselineHeartbeats = heartbeats;
        stage = Stage::WaitingHeartbeat;
    }
    bool ready(bool connected, uint32_t heartbeats) const {
        return accepted && stage == Stage::WaitingHeartbeat && connected && heartbeats != baselineHeartbeats;
    }
    void started(bool ok) { stage = ok ? Stage::Uploading : Stage::Error; }
    void complete(bool ok) { if(stage == Stage::Uploading) stage = ok ? Stage::Complete : Stage::Error; }
    void cancel() { if(accepted) stage = Stage::Cancelled; }
    const char* name() const {
        switch(stage) {
            case Stage::WaitingMission: return "waiting_mission";
            case Stage::WaitingHeartbeat: return "waiting_heartbeat";
            case Stage::Uploading: return "uploading";
            case Stage::Complete: return "complete";
            case Stage::Error: return "error";
            default: return "cancelled";
        }
    }
};
extern PowerWorkflow g_power;
