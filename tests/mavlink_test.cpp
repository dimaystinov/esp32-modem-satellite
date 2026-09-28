#include "../firmware/modem_c3/src/MavlinkUploader.h"
#include "../firmware/modem_c3/src/MissionStore.h"
#include <HardwareSerial.h>
#include <MAVLink.h>
#include <cassert>
MissionStore g_missionStore;
using A = MavlinkUploader::Activation;
void input(const mavlink_message_t& msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN]; auto n=mavlink_msg_to_send_buffer(buf,&msg);
    testMavPort->rx.insert(testMavPort->rx.end(),buf,buf+n); g_mavlinkUploader.poll();
}
std::vector<mavlink_command_long_t> commands() {
    std::vector<mavlink_command_long_t> out; mavlink_message_t m; mavlink_status_t s;
    for (auto b:testMavPort->tx) if(mavlink_parse_char(MAVLINK_COMM_1,b,&m,&s) && m.msgid==MAVLINK_MSG_ID_COMMAND_LONG) {
        mavlink_command_long_t c; mavlink_msg_command_long_decode(&m,&c); out.push_back(c);
    }
    testMavPort->tx.clear(); return out;
}
void hb(int mode=0,bool armed=false,int sys=1,int type=MAV_TYPE_QUADROTOR,int autopilot=MAV_AUTOPILOT_ARDUPILOTMEGA) {
    mavlink_message_t m; mavlink_msg_heartbeat_pack(sys,1,&m,type,autopilot,MAV_MODE_FLAG_CUSTOM_MODE_ENABLED|(armed?MAV_MODE_FLAG_SAFETY_ARMED:0),mode,MAV_STATE_ACTIVE); input(m);
}
void request(int seq,int sys=1) {
    mavlink_message_t m; mavlink_msg_mission_request_int_pack(sys,1,&m,MAVLINK_SYS_ID,MAVLINK_COMP_ID,seq,MAV_MISSION_TYPE_MISSION); input(m);
}
void missionAck(int result=MAV_MISSION_ACCEPTED,int sys=1,int target=MAVLINK_SYS_ID,int type=MAV_MISSION_TYPE_MISSION) {
    mavlink_message_t m; mavlink_msg_mission_ack_pack(sys,1,&m,target,MAVLINK_COMP_ID,result,type,0); input(m);
}
void ack(int command,int result=MAV_RESULT_ACCEPTED,int sys=1,int target=MAVLINK_SYS_ID) {
    mavlink_message_t m; mavlink_msg_command_ack_pack(sys,1,&m,command,result,0,0,target,MAVLINK_COMP_ID); input(m);
}
void reset(int type=MAV_TYPE_QUADROTOR,int autopilot=MAV_AUTOPILOT_ARDUPILOTMEGA) {
    g_mavlinkUploader.stopUpload(); g_mavlinkUploader.clearLink(); testMavPort->tx.clear(); testMillis=100;
    auto& points=const_cast<std::vector<Waypoint>&>(g_missionStore.getWaypoints()); points.assign(2,Waypoint{});points[0].seq=0;points[1].seq=1;
    hb(0,false,1,type,autopilot); assert(g_mavlinkUploader.startUpload());
}
void uploaded(int type=MAV_TYPE_QUADROTOR,int autopilot=MAV_AUTOPILOT_ARDUPILOTMEGA) {reset(type,autopilot);request(0);request(1);missionAck();}
int main() {
    g_mavlinkUploader.init();
    reset();request(1);request(1);missionAck();assert(commands().empty()); // duplicate/out-of-order last item isn't all items
    request(0,2);assert(g_mavlinkUploader.getSentCount()==1);
    request(0);missionAck(MAV_MISSION_ACCEPTED,2);missionAck(MAV_MISSION_ACCEPTED,1,254);missionAck(MAV_MISSION_ACCEPTED,1,MAVLINK_SYS_ID,MAV_MISSION_TYPE_FENCE);
    assert(commands().empty());missionAck();auto c=commands();
    assert(c.size()==1 && c[0].command==MAV_CMD_DO_SET_MODE && c[0].param1==1 && c[0].param2==3 && c[0].target_system==1 && c[0].target_component==1);
    missionAck();assert(commands().empty()); // duplicate MISSION_ACK never retriggers AUTO
    hb(3);assert(commands().empty()); // no COMMAND_ACK yet
    ack(MAV_CMD_COMPONENT_ARM_DISARM);ack(MAV_CMD_DO_SET_MODE,MAV_RESULT_ACCEPTED,2);ack(MAV_CMD_DO_SET_MODE,MAV_RESULT_ACCEPTED,1,254);hb(3);assert(commands().empty());
    ack(MAV_CMD_DO_SET_MODE);assert(commands().empty());hb(3);c=commands();
    assert(c.size()==1 && c[0].command==MAV_CMD_COMPONENT_ARM_DISARM && c[0].param1==1 && c[0].param2==0);
    hb(3,true);assert(g_mavlinkUploader.activation()==A::Arming);
    ack(MAV_CMD_COMPONENT_ARM_DISARM);hb(3,true);assert(g_mavlinkUploader.activation()==A::Complete);
    hb(3,false);assert(commands().empty() && g_mavlinkUploader.activation()==A::Complete); // no automatic rearm
    uploaded();commands();ack(MAV_CMD_DO_SET_MODE,MAV_RESULT_DENIED);hb(3);assert(g_mavlinkUploader.activation()==A::Error && commands().empty());
    uploaded();commands();ack(MAV_CMD_DO_SET_MODE);hb(3);commands();ack(MAV_CMD_COMPONENT_ARM_DISARM,MAV_RESULT_TEMPORARILY_REJECTED);hb(3);assert(g_mavlinkUploader.activation()==A::Error && commands().empty());
    uploaded();commands();g_mavlinkUploader.stopUpload();ack(MAV_CMD_DO_SET_MODE);hb(3);assert(g_mavlinkUploader.activation()==A::Cancelled && commands().empty());
    uploaded();commands();testMillis+=6000;g_mavlinkUploader.poll();assert(g_mavlinkUploader.activation()==A::Error);
    uploaded();commands();for(int i=0;i<31;++i){testMillis+=1000;hb(0);}assert(g_mavlinkUploader.activation()==A::Error && commands().empty());
    uploaded();commands();ack(MAV_CMD_DO_SET_MODE);hb(3);commands();hb(0);assert(g_mavlinkUploader.activation()==A::Error);
    reset();request(0);request(1);missionAck(MAV_MISSION_ERROR);assert(commands().empty() && g_mavlinkUploader.activation()==A::Idle);
    uploaded(MAV_TYPE_FIXED_WING);c=commands();assert(c.size()==1 && c[0].param2==10);
    uploaded(MAV_TYPE_GROUND_ROVER);c=commands();assert(c.size()==1 && c[0].param2==10);
    uploaded(MAV_TYPE_QUADROTOR,MAV_AUTOPILOT_PX4);assert(g_mavlinkUploader.activation()==A::Error && commands().empty());
    puts("PASS: actual MAVLink UART upload -> AUTO -> ARM; identity, ACK/heartbeat order, rejections, cancellation, timeouts, no rearm");
}
