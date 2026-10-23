#include "dfre/mission_wet_effect.hpp"
#include <stdexcept>

namespace dfre {
void mission_wet_effect(const MissionWetEffectBindings& b,std::uint32_t frame){
    using U=std::uint32_t;
    const auto read=[&](U at){if(!b.read)throw std::runtime_error("Wet effect current memory read is not bound");return b.read(at,4);};
    const auto write=[&](U offset,U value){if(!b.write)throw std::runtime_error("Wet effect scratch store is not bound");b.write(frame+offset,4,value);};
    const auto call=[&](U site,const std::array<U,3>& args){if(!b.call)throw std::runtime_error("Wet effect required body is not bound");return b.call(site,args);};
    const U index=read(0x8e44b0u+read(frame+0x120u)*4u);
    const U definition=read(0xc23810u)+index*0x13cu;
    call(0x49a620,{frame,0,0x114});
    write(0,index);write(4,definition);
    write(0x10,read(definition+0x6c));
    write(0x24,read(frame+0x124));write(0x28,read(frame+0x128));write(0x2c,read(frame+0x12c));
    write(0x30,call(0x477860,{0,0,0})<<16);
    write(0x7c,call(0x477860,{0,0,0})+0x8000u);
    write(0x6c,0);
    write(0x110,read(definition+0x84));
    write(0x10c,read(definition+0x90));
    call(0x45d970,{frame,1,0});
}
} // namespace dfre
