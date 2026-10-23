#include "dfre/mission_audio_listener.hpp"
namespace dfre {
void mission_audio_listener(std::uint32_t x,std::uint32_t y,std::uint32_t z,
    std::uint32_t yaw,std::uint32_t pitch,const MissionAudioListenerBindings& b){
    if(!b.write)throw MissionAudioListenerError("Listener needs current shared storage");
    b.write(0x6e06a8,4,x);b.write(0x6e06ac,4,y);b.write(0x6e06b0,4,z);
    b.write(0x6e4708,4,x);b.write(0x6e4704,4,y);b.write(0x6e06b4,4,yaw);
    b.write(0x6e4700,4,z);b.write(0x6e06b8,4,pitch);
}
}
