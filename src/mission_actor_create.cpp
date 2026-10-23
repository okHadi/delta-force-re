#include "dfre/mission_actor_create.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
U read(const MissionActorCreateBindings& b,U at,U width=4){if(!b.read)throw MissionActorCreateError("Actor creation needs current memory reads");return b.read(at,width);}
void write(const MissionActorCreateBindings& b,U at,U value,U width=4){if(!b.write)throw MissionActorCreateError("Actor creation needs current memory writes");b.write(at,width,value);}
U call(const MissionActorCreateBindings& b,U site,std::vector<U> args){if(!b.call)throw MissionActorCreateError("Actor creation needs its nested body");return b.call(site,args);}
std::int32_t signed32(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
}
U mission_actor_create(const MissionActorCreateBindings& b,U face,U side){
    const auto mode=read(b,0x8cf97c),night=read(b,0x8cf982,1)&0x10;
    const U class_id=mode==2?(night?(side?0x13b7u:0x13b6u):(side?0x1393u:0x1392u)):(night?(side?0x13b5u:0x13b4u):(side?0x138cu:0x138bu));
    const auto index=call(b,0x4828c0,{class_id});const auto definition=read(b,0xc23810)+index*0x13c;
    U slot=0,offset=0,occupied=read(b,0x7cca80);
    while(occupied){offset+=0x114;++slot;if(offset>=0x11400)break;occupied=read(b,0x7cca80+offset);}
    if(slot>=256)return 0;
    if(signed32(slot)>=signed32(read(b,0x8cfa18)))write(b,0x8cfa18,slot+1);
    const U actor=0x7cca80+slot*0x114;call(b,0x49a620,{actor,0,0x114});
    write(b,actor,index);write(b,actor+4,definition);
    const auto graphic=read(b,definition+0x6c);write(b,actor+0x14,0);write(b,actor+0x10,graphic);
    const auto character=call(b,0x42cce0,{read(b,definition+0x74)});write(b,actor+0x18,character);
    const auto person=call(b,0x44a0c0,{actor});write(b,actor+0x6c,3);write(b,actor+0x72,1,2);write(b,actor+0x0c,person);
    write(b,actor+0x98,face);write(b,actor+0x110,read(b,definition+0x84));write(b,actor+0x10c,read(b,definition+0x90));
    const auto scale=read(b,0x67f549,1)&1;const auto health=read(b,definition+0x94,2);write(b,actor+0x74,scale?U(std::uint16_t(health<<2)):health,2);
    return actor;
}
} // namespace dfre
