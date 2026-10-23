#include "dfre/mission_sound_registration.hpp"
namespace dfre {
namespace {
using U=std::uint32_t;
U read(const MissionSoundBankBindings& b,U at,U width=4){if(!b.read)throw MissionSoundBankError("Sound registration reader is not bound");return b.read(at,width);}
void put(const MissionSoundBankBindings& b,U at,U value,U width=4){if(!b.write)throw MissionSoundBankError("Sound registration writer is not bound");b.write(at,width,value);}
}
void mission_sound_attach(const MissionSoundBankBindings& b,U actor,U id,U scale,U ceiling){
    const U definition=0x4f2ea8u+id*40;
    if(!(read(b,definition+0x20,1)&2))return;
    const auto ambient=read(b,definition);if(!ambient)return;
    if(!read(b,ambient+0x18)){
        if(!b.call)throw MissionSoundBankError("Ambient sample loader is not bound");
        if(!b.call(0x43ee80,{id}))return;
    }
    for(U index=0;index<1024;++index){const U row=0x6e0700u+index*16;
        if(read(b,row+4))continue;
        put(b,row,read(b,definition));put(b,row+4,actor);
        put(b,row+8,read(b,0x4f7ea4));put(b,row+12,scale&255,1);put(b,row+13,ceiling&255,1);
        put(b,actor+0x1c,row);put(b,0x4f7ea4,read(b,0x4f7ea4)+1);return;
    }
}
U mission_sound_loudness(const MissionSoundBankBindings& b,U id){
    const auto ambient=read(b,0x4f2ea8u+id*40);return ambient?read(b,ambient+0x48)<<16:0;
}
}
