#include "dfre/mission_effect_emit.hpp"
#include <cstring>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
struct Access {
    const MissionEffectEmitBindings& b;
    U r(U at)const{if(!b.read)throw MissionEffectEmitError("Effect emission reader is not bound");return b.read(at,4);}
    void w(U at,U value)const{if(!b.write)throw MissionEffectEmitError("Effect emission writer is not bound");b.write(at,4,value);}
    U call(U site,std::initializer_list<U> args)const{if(!b.call)throw MissionEffectEmitError("Effect emission body is not bound");return b.call(site,args);}
};
}
void mission_effect_emit(const MissionEffectEmitBindings& b,U arguments,U frame,std::size_t bytes){
    if((arguments|frame)&3u||bytes<0x114||std::uint64_t(frame)+0x114>0x100000000ull||std::uint64_t(arguments)+12>0x100000000ull)throw MissionEffectEmitError("Effect emission needs aligned current arguments and 114h frame bytes");
    const Access m{b};const U index=m.call(0x4828c0,{0x1b68}),definition=m.r(0xc23810)+index*0x13c;
    m.call(0x49a620,{frame,0,0x114});m.w(frame,index);m.w(frame+4,definition);m.w(frame+0x10,m.r(definition+0x6c));
    const U x=m.r(arguments);m.w(frame+0x3c,1);m.w(frame+0x24,x);
    const U y=m.r(arguments+4);m.w(frame+0x44,0);m.w(frame+0x28,y);
    const U z=m.r(arguments+8);m.w(frame+0x40,0);m.w(frame+0x2c,z);
    const U first=m.call(0x477860,{});m.w(frame+0x38,first<<16);
    const U second=m.call(0x477860,{}),growth=U(si(second+0x20000u)>>4);m.w(frame+0x6c,0);m.w(frame+0x7c,growth);
    m.w(frame+0x110,m.r(definition+0x84));m.w(frame+0x10c,m.r(definition+0x90));m.call(0x45d970,{frame,1});
}
void mission_effect_definitions_initialize(const MissionEffectEmitBindings& b){
    const Access m{b};m.w(0x8e44b0,0);m.call(0x45d6f0,{0});
    constexpr U first[]{0x1b68,0x1b67,0x1b65,0x1b66,0x1b59,0x1b5a,0x1b5b,0x1b5c,0x1b5d,0x1b5e,0x1b5f,0x1b60,0x1b61,0x1b62,0x1b63,0x1b64};
    for(U i=0;i<16;++i){const U index=m.call(0x4828c0,{first[i]});m.w(0x8e44b4+i*4,index);m.call(0x45d6f0,{index});}
    constexpr U rest[]{0xfce,0xfd3,0xfda,0xfdb,0xfdc,0xfdd,0xfde,0xfe0,0xfe1,0xfe2,0xfe3,0xfe4,0xfe5,0xfe6,0xfeb};
    for(U id:rest){const U index=m.call(0x4828c0,{id});m.call(0x45d6f0,{index});}
}
}
