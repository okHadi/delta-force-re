#include "dfre/mission_behavior_zones.hpp"
namespace dfre {
namespace {
using U=std::uint32_t;
U read(const MissionBehaviorZoneBindings& b,U at){if(!b.read)throw std::runtime_error("Behavior zone reader is not bound");return b.read(at,4);}
void write(const MissionBehaviorZoneBindings& b,U at,U value){if(!b.write)throw std::runtime_error("Behavior zone writer is not bound");b.write(at,4,value);}
std::int32_t signed32(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
}
void mission_behavior_zones_reset(const MissionBehaviorZoneBindings& b){write(b,0x4f8d44,0);}
bool mission_behavior_zone_add(const MissionBehaviorZoneBindings& b,U x,U y,U z,U radius,U type,U flags){
    if(!read(b,0x67f528))return false;
    const auto count=read(b,0x4f8d44);if(signed32(count)>=200)return false;
    const auto row=0x8e48b0u+(count<<5);
    write(b,row,flags);write(b,row+4,type);
    write(b,row+8,x-radius);write(b,row+12,x+radius);
    write(b,row+16,y-radius);write(b,row+20,y+radius);
    write(b,row+24,z-radius);write(b,0x4f8d44,count+1);write(b,row+28,z+radius);
    return true;
}
MissionBehaviorZoneBindings MissionBehaviorZones::bindings(U& host_gate){
    auto cell=[this,&host_gate](U at,U width)->U&{
        if(width!=4)throw std::runtime_error("Behavior zone access must be four bytes");
        if(at==0x67f528)return host_gate;
        if(at==0x4f8d44)return count;
        if(at<0x8e48b0||at>=0x8e48b0+200*32||(at&3))
            throw std::runtime_error("Behavior zone address is outside the current owner");
        const U offset=at-0x8e48b0;return records[offset/32][(offset%32)/4];
    };
    return {[cell](U at,U width){return cell(at,width);},
            [cell](U at,U width,U value){cell(at,width)=value;}};
}
std::vector<ActorZoneRect> MissionBehaviorZones::active_records()const{
    if(count>records.size())throw std::runtime_error("Behavior zone count exceeds the current owner");
    std::vector<ActorZoneRect> result;result.reserve(count);
    for(U i=0;i<count;++i){const auto& r=records[i];
        result.push_back({r[0],r[1],signed32(r[2]),signed32(r[3]),signed32(r[4]),signed32(r[5]),signed32(r[6]),signed32(r[7])});
    }
    return result;
}
}
