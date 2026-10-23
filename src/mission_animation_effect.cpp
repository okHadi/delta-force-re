#include "dfre/mission_animation_effect.hpp"
#include <cstring>
#include <stdexcept>
namespace dfre {namespace {using U=std::uint32_t;std::int32_t si(U value){std::int32_t result;std::memcpy(&result,&value,4);return result;}}
void mission_animation_effect(const MissionAnimationEffectBindings& b,const std::array<U,3>& args){
    const auto read=[&](U at,U width=4){if(!b.read)throw std::runtime_error("Animation effect current memory is not bound");return b.read(at,width);};
    const auto call=[&](U site,const std::array<U,4>& values){if(!b.call)throw std::runtime_error("Animation effect required body is not bound");b.call(site,values);};
    const U actor=args[2];const U initial_water=U(read(0xbd5248));
    const U x=U(read(actor+0x24));const U y=U(read(actor+0x28));const U z=U(read(actor+0x2c));
    if(initial_water){const auto bits=read(0x4ccb2c,8);double bias;std::memcpy(&bias,&bits,8);const long double level=static_cast<long double>(si(z))+static_cast<long double>(bias);const U current_water=U(read(0xbd5248));
        if(static_cast<long double>(si(current_water))>level){call(0x45dc70,{2,x,y,initial_water});const bool step=args[1]!=0;const U sound_water=U(read(0xbd5248));call(0x43e7d0,{step?0x88u:0x89u,x,y,sound_water});return;}}
    const U offset=((x&0x3ff0000u)>>16)+((y&0x3ff0000u)>>6);const bool step=args[1]!=0;
    const U grid=U(read(0xbd52ac));const U cell=U(read(grid+offset,1))&255u;const U type=U(read(0xbd5334+cell,1))&255u;
    U sound=0;if(type==1||type==4||type==5||type==6)sound=step?0x82:0x83;else if(type==2)sound=step?0x8b:0x8c;else if(type==3)sound=step?0x86:0x87;else return;
    call(0x43e7d0,{sound,x,y,z});
}
}
