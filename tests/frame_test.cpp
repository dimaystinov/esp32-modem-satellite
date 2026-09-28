#include "../firmware/modem_c3/src/FrameReceiver.h"
#include <cassert>
static bool accept=false; static int calls=0;
static bool received(const uint8_t*,size_t){++calls;assert(Serial.tx.empty());return accept;}
static void feed(bool crcOk){
 Serial.tx.clear();
 std::vector<uint8_t> frame={0x79,1,3,0,'a','b','c'};
 uint16_t crc=ProtocolCrc::calculate(frame.data(),frame.size());
 if(!crcOk)crc^=1;
 frame.push_back(crc&255);frame.push_back(crc>>8);frame.push_back(0x47);
 for(auto b:frame)Serial.rx.push_back(b);
 g_frameReceiver.pollSerial();
}
int main(){
 assert(g_frameReceiver.init());g_frameReceiver.setOnFrameReceived(received);
 feed(true);assert(calls==1 && Serial.tx==std::vector<uint8_t>({0x79,1,0x14,0x47}));
 accept=true;feed(true);assert(calls==2 && Serial.tx==std::vector<uint8_t>({0x79,1,0x55,0x47}));
 feed(false);assert(calls==2 && Serial.tx[2]==0x14);
 g_frameReceiver.deinit();
 puts("PASS: ACK only after application acceptance; rejected mission and bad CRC return NACK");
}
