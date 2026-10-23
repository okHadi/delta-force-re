#include "dfre/mission_sound_stop.hpp"
namespace dfre {
void mission_sound_stop(const MissionSoundBankBindings& b,std::uint32_t actor,std::uint32_t scale){
    if(!b.read)throw MissionSoundBankError("Sound stop reader is not bound");
    for(std::uint32_t i=0;i<1024;++i){const auto row=0x6e0700u+i*16;
        if(b.read(row+4,4)!=actor)continue;
        if(b.read(row+12,1)!=scale)continue;
        if(!b.call)throw MissionSoundBankError("Sound stop clear body is not bound");
        b.call(0x49a620,{row,0,16});
        if(b.read(actor+0x1c,4)==row){
            if(!b.write)throw MissionSoundBankError("Sound stop writer is not bound");
            b.write(actor+0x1c,4,0);
        }
    }
}
}
