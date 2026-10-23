#include "dfre/mission_object_helo_shot.hpp"
#include <cstring>
namespace dfre {namespace {
using U=std::uint32_t;using S=std::int32_t;
S sign(U n){S s;std::memcpy(&s,&n,4);return s;}
struct Body {
    const MissionObjectHeloBindings& b;
    U read(U at)const{if(!b.read)throw MissionTickError("Helicopter shot read owner is not bound");return b.read(at,4);}
    void write(U at,U n)const{if(!b.write)throw MissionTickError("Helicopter shot write owner is not bound");b.write(at,4,n);}
    U call(U site,std::vector<U> args)const{if(!b.call)throw MissionTickError("Helicopter shot required body is not bound "+std::to_string(site));return b.call(site,args);}
};
}
void mission_object_helo_shot(const MissionObjectHeloBindings& b,U actor,U frame,std::size_t bytes){
    if(!frame||(frame&3)||bytes<0x1c||std::uint64_t(frame)+0x1c>0x100000000ull)throw MissionTickError("Helicopter shot needs a current aligned28-byte frame");
    Body h{b};const auto person=h.read(actor+0xc);h.write(actor+0x94,12);h.call(0x450630,{actor,frame});
    const auto target_x=h.read(person+0x48),origin_x=h.read(frame);const auto dx=target_x-origin_x;
    const auto x=U(sign(dx)>>16);const auto xx=x*x;
    const auto target_y=h.read(person+0x4c),origin_y=h.read(frame+4),origin_z=h.read(frame+8),target_z=h.read(person+0x50);
    const auto dy=target_y-origin_y,dz=target_z-origin_z,y=U(sign(dy)>>16);
    const auto horizontal=h.call(0x477620,{xx+y*y})<<16;h.write(frame+0x18,horizontal);
    const auto yaw=h.call(0x477770,{dx,dy});const auto current_horizontal=h.read(frame+0x18);h.write(frame+0xc,yaw);
    const auto pitch=h.call(0x477770,{current_horizontal,dz});h.write(frame+0x10,pitch);
    const auto actor_pitch=h.read(actor+0x34),definition=h.read(actor+4),current_yaw=h.read(frame+0xc),actor_yaw=h.read(actor+0x30),id=h.read(definition+0x20);
    const auto yaw_error=current_yaw-actor_yaw,pitch_error=pitch-actor_pitch;
    if(id==0x7e2){if(sign(yaw_error)<-0x20000000||sign(yaw_error)>0x20000000||sign(pitch_error)<-0x20000000||sign(pitch_error)>0)return;}
    else if(id==0x7e5){if(sign(yaw_error)<-0x60000000||sign(yaw_error)>-0x20000000||sign(pitch_error)<-0xc000000||sign(pitch_error)>0)return;}
    auto draw=h.call(0x477860,{});auto current=h.read(frame+0xc);h.write(frame+0xc,current+((draw&0xffff)<<6)-0x200000);
    draw=h.call(0x477860,{});current=h.read(frame+0x10);h.write(frame+0x10,current+((draw&0xffff)<<6)-0x200000);
    current=h.read(frame+0xc);const auto bias=h.read(actor+0xb8);h.write(frame+0xc,current+bias);
    h.call(0x45ea00,{actor,frame});
}
} // namespace dfre
