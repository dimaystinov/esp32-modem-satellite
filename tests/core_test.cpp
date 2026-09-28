#include "../firmware/modem_c3/src/SwitchControl.h"
#include "../firmware/modem_c3/src/MissionParser.h"
#include "../firmware/modem_c3/src/ProtocolCrc.h"
#include <cassert>
#include "../firmware/modem_c3/src/PowerWorkflow.h"
#include <string>
int main() {
    static_assert(PIN_MAV_RX==3 && PIN_MAV_TX==10 && PIN_PROTO_RX_WORK==6 && PIN_PROTO_TX_WORK==7);
    static_assert(PIN_SWITCH==4 && !PROTOCOL_OVER_USB);
    SwitchControl sw;
    sw.set(true); assert(writes.empty());
    sw.begin(); assert(outputConfigured && !sw.enabled() && sw.level()==LOW);
    sw.set(true); assert(sw.enabled() && writes.back()==std::make_pair(4,HIGH));
    sw.set(false); assert(!sw.enabled() && writes.back()==std::make_pair(4,LOW));
    sw.set(true); sw.begin(); assert(!sw.enabled());
    bool enabled=false;
    assert(SwitchControl::parse("1",enabled) && enabled);
    assert(SwitchControl::parse("0",enabled) && !enabled);
    for(const char *bad : {"", "on", "true", "01", "1x", "2", "-1"}) assert(!SwitchControl::parse(bad,enabled));
    assert(!SwitchControl::parse(nullptr,enabled));
    assert(ProtocolCrc::calculate(reinterpret_cast<const uint8_t*>("123456789"),9)==0x29B1);
    MissionParser parser; std::vector<Waypoint> points;
    const std::string mission="QGC WPL 110\n0\t1\t0\t16\t0\t0\t0\t0\t55\t37\t100\t1\n1\t0\t3\t16\t0\t0\t0\t0\t55.1\t37.1\t50\t1\n";
    assert(parser.parse(reinterpret_cast<const uint8_t*>(mission.data()),mission.size(),points).result==MissionParser::Result::OK && points.size()==2);
    assert(parser.parse(mission.c_str(),points).result==MissionParser::Result::OK && points.size()==2);
    assert(parser.parse("broken",points).result==MissionParser::Result::INVALID_HEADER);
    const std::string maxFrame=mission+std::string(65535-mission.size(),'\n');
    assert(parser.parse(reinterpret_cast<const uint8_t*>(maxFrame.data()),maxFrame.size(),points).result==MissionParser::Result::OK && points.size()==2);
    PowerWorkflow flow;
    assert(!flow.accepted && !flow.ready(true,10));
    flow.missionSaved(100,10);
    assert(flow.accepted && flow.powerAt==100 && !flow.ready(true,10));
    assert(!flow.ready(false,11) && flow.ready(true,11));
    flow.started(true); assert(!flow.ready(true,12));
    flow.complete(true); assert(flow.stage==PowerWorkflow::Stage::Complete);
    flow.missionSaved(200,12); assert(flow.powerAt==100 && flow.ready(true,13));
    flow.cancel(); assert(!flow.ready(true,14) && flow.accepted);
    flow.missionSaved(300,14); flow.started(false); assert(flow.stage==PowerWorkflow::Stage::Error);
    PowerWorkflow reboot; assert(!reboot.accepted && !reboot.ready(true,15));
    puts("PASS: C3 pin map, GPIO4 boot/off/on/input validation, CRC, mission parsing incl. 65535-byte payload");
}
