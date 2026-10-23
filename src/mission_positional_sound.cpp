#include "dfre/mission_positional_sound.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/world_hits.hpp"
#include <limits>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
U absolute(U n){return si(n)<0?0u-n:n;}
template<class F> const F& need(const F& f,const char* text){if(!f)throw MissionSoundBankError(text);return f;}
U read(const MissionSoundBankBindings& b,U at,U width=4){return need(b.read,"Positional sound shared reader is not bound")(at,width);}
void put(const MissionSoundBankBindings& b,U at,U value){need(b.write,"Positional sound shared writer is not bound")(at,4,value);}
U product(U a,U c){return U(std::uint64_t(std::int64_t(si(a))*si(c))>>16);}
U reciprocal(U value){if(!value)throw MissionSoundBankError("Sound occlusion divides by zero");const auto q=0x100000000LL/si(value);if(q<std::numeric_limits<std::int32_t>::min()||q>std::numeric_limits<std::int32_t>::max())throw MissionSoundBankError("Sound occlusion signed divide overflows");return U(q);}
}
U mission_sound_occlusion(const MissionSoundBankBindings& b,Fixed3 from,Fixed3 to){
    U x=U(from.x),y=U(from.y),z=U(from.z),end_x=U(to.x),end_y=U(to.y);
    const auto dx=U(to.x)-x,dy=U(to.y)-y,dz=U(to.z)-z,ax=absolute(dx),ay=absolute(dy);
    if((!dx&&!dy)||(si(ax)<0x10000&&si(ay)<0x10000))return 0x10000;
    U blocked=0;
    const auto sample=[&]{const auto ix=(x>>16)&0x3ffu,iy=(y>>16)&0x3ffu;const auto base=read(b,0xbd5284);const auto height=read(b,base+iy*0x400+ix,1)<<14;
        if(si(z)<si(height))blocked+=U(si(height-z)>>16);
    };
    if(si(ay)<si(ax)){
        if(to.x<from.x){x=U(to.x);y=U(to.y);z=U(to.z);end_x=U(from.x);}
        const auto factor=reciprocal(dx),step_y=product(dy,factor),step_z=product(dz,factor);
        while(si(x)<=si(end_x)){sample();const auto next=x+0x10000u;if(si(next)<si(x))throw MissionSoundBankError("Sound occlusion signed traversal wraps");x=next;y+=step_y;z+=step_z;}
    }else{
        if(to.y<from.y){x=U(to.x);y=U(to.y);z=U(to.z);end_y=U(from.y);}
        const auto factor=reciprocal(dy),step_x=product(dx,factor),step_z=product(dz,factor);
        while(si(y)<=si(end_y)){sample();const auto next=y+0x10000u;if(si(next)<si(y))throw MissionSoundBankError("Sound occlusion signed traversal wraps");y=next;x+=step_x;z+=step_z;}
    }
    return (blocked+0x200u)<<7;
}
void mission_sound_positional_request(const MissionSoundBankBindings& b,U id,U x,U y,U z,U params){
    if(id>=512)throw MissionSoundBankError("Positional sound id exceeds its definition table");const auto at=0x4f2ea8u+id*0x28;
    if(!read(b,at))return;
    const auto listener_z=read(b,0x6e06b0),listener_x=read(b,0x6e06a8),listener_y=read(b,0x6e06ac);
    const auto dx=x-listener_x,dy=y-listener_y,dz=z-listener_z;
    const auto distance=U(collision_approximate_length(si(absolute(dx)),si(absolute(dy)),si(absolute(dz))));
    if(U(si(distance)>>16)>read(b,read(b,at)+0x48))return;
    const auto current_z=read(b,0x6e06b0),current_y=read(b,0x6e06ac),current_x=read(b,0x6e06a8);
    const auto occlusion=mission_sound_occlusion(b,{si(x),si(y),si(z)},{si(current_x),si(current_y),si(current_z)});
    const auto adjusted=product(distance,occlusion);
    if(U(si(adjusted)>>16)>read(b,read(b,at)+0x48))return;
    const auto angle=query_angle_477770(si(dx),si(dy));const auto pan=255u-((read(b,0x6e06b4)-angle)>>24);
    for(unsigned offset=0;offset<32;offset+=4)put(b,params+offset,0);
    put(b,params,0x10000);put(b,params+8,0xff);put(b,params+0xc,0xffffffff);put(b,params+0x10,0);put(b,params+4,pan);put(b,params+0x14,adjusted);
    const auto flags=read(b,at+0x20,1);mission_sound_bank_play(b,read(b,at),params,(flags&4u)?0x6e05e0u:0x6e0518u);
}
}
