#include "dfre/mission_object_effects.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
struct Memory {
 const MissionObjectEffectBindings& b;
 U read(U at,U width=4)const{if(!b.read)throw std::runtime_error("Object effect memory is not bound");return b.read(at,width);}
 void write(U at,U n,U width=4)const{if(!b.write)throw std::runtime_error("Object effect store is not bound");b.write(at,width,width==1?n&255u:width==2?n&65535u:n);}
 U call(U site,const std::vector<U>& args)const{if(!b.call)throw std::runtime_error("Object effect required call is not bound");return b.call(site,args);}
};
std::int32_t signed_word(U n){return n<0x80000000u?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
}
void mission_object_effect_request(const MissionObjectEffectBindings& b,U frame){
 const Memory m{b};const auto pose=m.read(frame+0x20);
 if(!m.read(0x67f528))return;
 auto n=m.read(pose);m.write(frame+2,n);n=m.read(pose+4);m.write(frame+6,n);n=m.read(pose+8);m.write(frame+10,n);n=m.read(pose+12);m.write(frame+14,n>>16,2);
 const auto owner=m.read(frame+0x1c);m.write(frame,(owner-0x7cca80u)/0x114u,2);m.write(frame+16,0,1);n=m.read(frame+0x24,1);m.write(frame+17,n,1);
 m.call(0x442630,{0x27,frame,18});const auto count=m.read(frame+0x24);const auto current_owner=m.read(frame+0x1c);m.call(0x4606b0,{current_owner,pose,count});
}
void mission_object_effect_publish(const MissionObjectEffectBindings& b,U frame){
 const Memory m{b};const auto z=m.read(frame+0x44);const auto y=m.read(frame+0x40);const auto x=m.read(frame+0x3c);const auto owner=m.read(frame+0x38);const auto actor=m.call(0x460d20,{owner,x,y,z});if(!actor)return;
 m.call(0x49a620,{frame,0,0x2b});m.write(frame,(actor-0x7dde80u)/0x114u,2);const auto definition=m.read(actor+4);auto n=m.read(definition+0x20,2);m.write(frame+2,n,2);
 n=m.read(actor+0x6c);m.write(frame+0x14,n);n=m.read(actor+0x24);m.write(frame+0x18,n);n=m.read(actor+0x28);m.write(frame+0x1c,n);n=m.read(actor+0x2c);m.write(frame+0x20,n);
 n=m.read(actor+0x30);m.write(frame+0x24,n>>16,2);n=m.read(actor+0x34);m.write(frame+0x26,n>>16,2);n=m.read(actor+0x70,1);m.write(frame+0x2a,n,1);
 m.write(frame+0x28,owner?(actor-0x7cca80u)/0x114u:0xffffu,2);m.call(0x442630,{0x2b,frame,0x2b});
}
void mission_object_effect_apply(const MissionObjectEffectBindings& b,U frame){
 const Memory m{b};const auto pose=m.read(frame+0x30);const auto count=m.read(frame+0x34);m.call(0x477850,{0x5fc});
 auto z=m.read(pose+8);auto y=m.read(pose+4);auto x=m.read(pose);m.call(0x45dc70,{3,x,y,z});
 auto owner=m.read(frame+0x2c);z=m.read(pose+8);y=m.read(pose+4);x=m.read(pose);m.call(0x4657d0,{1,x,y,z,owner});
 z=m.read(pose+8);y=m.read(pose+4);x=m.read(pose);m.call(0x43e7d0,{0x32,x,y,z});
 z=m.read(pose+8);y=m.read(pose+4);x=m.read(pose);m.call(0x45fcf0,{x,y,z,200,0,0});
 auto n=m.read(pose);m.write(frame,n);n=m.read(pose+4);m.write(frame+4,n);n=m.read(pose+8);m.write(frame+8,n);
 const auto half=signed_word((count>>1)|((count&0x80000000u)?0x80000000u:0));
 for(U shift:{15u,16u})for(std::int32_t i=0;i<half;++i){const auto yaw=m.call(0x477860,{});const auto pitch=m.call(0x477860,{});const auto current_yaw=m.read(pose+12);m.write(frame+16,pitch<<shift);m.write(frame+12,current_yaw+(yaw<<16));const auto speed=(m.call(0x477860,{})|0x100u)*20u;owner=m.read(frame+0x2c);m.call(0x463200,{frame,owner,speed});}
}
U mission_object_effect_allocate(const MissionObjectEffectBindings& b,U frame){
 const Memory m{b};const auto index=m.call(0x4828c0,{0xfce});const auto base=m.read(0xc23810);const auto definition=base+index*0x13cu;m.call(0x49a620,{frame,0,0x114});m.write(frame,index);m.write(frame+4,definition);
 auto n=m.read(definition+0x6c);m.write(frame+0x10,n);n=m.read(frame+0x124);m.write(frame+0x24,n);n=m.read(frame+0x128);m.write(frame+0x28,n);n=m.read(frame+0x12c);m.write(frame+0x2c,n);n=m.read(frame+0x120);m.write(frame+0x6c,0);m.write(frame+0xf8,n);n=m.read(definition+0x84);m.write(frame+0x110,n);n=m.read(definition+0x90);m.write(frame+0x10c,n);return m.call(0x45d970,{frame,0});
}
}
