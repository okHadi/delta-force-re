#include "dfre/mission_object_helo.hpp"
#include "dfre/projectiles.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;using S=std::int32_t;
S sign(U n){S s;std::memcpy(&s,&n,4);return s;}
U absolute(U n){return sign(n)<0?0u-n:n;}
U product(U a,U b,unsigned shift){return U((std::int64_t(sign(a))*sign(b))>>shift);}
struct Body {
    const MissionObjectHeloBindings& b;U a;
    U read(U at,U width=4)const{if(!b.read)throw MissionTickError("Helicopter read owner is not bound");return b.read(at,width);}
    void write(U at,U n,U width=4)const{if(!b.write)throw MissionTickError("Helicopter write owner is not bound");b.write(at,width,width==1?n&0xffu:width==2?n&0xffffu:n);}
    U r(U offset,U width=4)const{return read(a+offset,width);}
    void w(U offset,U n,U width=4)const{write(a+offset,n,width);}
    U call(U site,std::vector<U> args)const{if(!b.call)throw MissionTickError("Helicopter required body is not bound "+std::to_string(site));return b.call(site,args);}
};
}
void mission_object_helo_turn(const MissionObjectHeloBindings& b,U actor){
    Body h{b,actor};const auto yaw=h.r(0x30),target=h.r(0x8c);h.w(0x90,0);if(yaw==target)return;
    const auto delta=U(sign(target-h.r(0x30))>>16);const auto definition=h.r(4);const auto step=h.read(definition+0xe0);
    if(sign(absolute(delta))<sign(step)){h.w(0x30,h.r(0x8c));return;}
    const auto scaled=step<<16;
    if(sign(delta)>0){const auto current=h.r(0x30);h.w(0x90,scaled);h.w(0x30,current+scaled);}
    else{const auto current=h.r(0x30);h.w(0x30,current-scaled);h.w(0x90,(0u-step)<<16);}
}
void mission_object_helo_speed(const MissionObjectHeloBindings& b,U actor,U target){
    Body h{b,actor};const auto current=h.r(0x7c);if(target==current)return;
    const auto definition=h.r(4);const auto flags=h.r(0x6c,1);auto step=h.read(definition+0xdc);
    const auto delta=target-current;if(flags&4)step+=step;
    if(sign(absolute(delta))<sign(step)){h.w(0x7c,target);return;}
    if(sign(delta)>0)h.w(0x7c,h.r(0x7c)+step);else h.w(0x7c,h.r(0x7c)-step);
}
void mission_object_helo_integrate(const MissionObjectHeloBindings& b,U actor){
    Body h{b,actor};const auto speed=h.r(0x7c);
    if(!speed){h.w(0x3c,0);h.w(0x40,0);}
    else{
        const auto index=h.r(0x30)>>21;const auto sine=U(kSineTableQ22[index]),cosine=U(kSineTableQ22[index+512]);
        const auto first=product(h.r(0x7c),cosine,22);h.w(0x3c,U(sign(first)>>6));
        const auto second=product(h.r(0x7c),sine,22);h.w(0x40,U(sign(second)>>6));
    }
    const auto dx=h.r(0x3c),x=h.r(0x24),y=h.r(0x28);const auto dy=h.r(0x40);
    h.w(0x24,x+dx);h.w(0x28,y+dy);
}
void mission_object_helo_bank(const MissionObjectHeloBindings& b,U actor){
    Body h{b,actor};U target=0;
    if(sign(h.r(0xb4))>0x60000){
        mission_object_helo_turn(b,actor);const auto turn=h.r(0x90);const auto speed=h.r(0x7c);
        target=product((0u-turn)<<3,speed,16);
        if(sign(target)>0x10000000)target=0x10000000;
        else if(sign(target)<-0x10000000)target=0xf0000000;
    }
    const auto roll=h.r(0x38);const auto delta=target-roll;
    if(sign(delta)>0)h.w(0x3a,h.r(0x3a,2)+0x40,2);
    else if(sign(delta)<0)h.w(0x38,roll-0x400000);
}
void mission_object_helo_move(const MissionObjectHeloBindings& b,U actor){
    Body h{b,actor};const auto rotor=h.r(0x98),rotor_position=h.r(0x9c),person=h.r(0xc);
    const auto target=h.read(person+0x44);h.w(0x9c,rotor_position+rotor);
    const auto rotor2_position=h.r(0xa4),z=h.r(0x2c),rotor2=h.r(0xa0),velocity=h.r(0xac);
    const auto z_velocity=h.r(0xac);h.w(0xa4,rotor2_position+rotor2);
    const auto acceleration=h.r(0xb0);h.w(0x2c,z+z_velocity);
    const auto flags=h.r(0x6c,1);h.w(0xac,velocity+acceleration);
    if(!(flags&4)){
        const auto spin=h.r(0xd4);if(sign(spin)>2){auto turn=spin<<22;if(sign(turn)>0x1400000)turn=0x1400000;h.w(0x30,h.r(0x30)+turn);}
    }
    if(sign(h.r(0xac))<0){
        const auto y=h.r(0x28),x=h.r(0x24);auto height=h.call(0x47df40,{x,y});const auto floor=h.r(0xdc);
        if(sign(height)<sign(floor))height=floor;
        const auto definition=h.r(4);height+=h.read(definition+0x20)==0x7e5?0x37000:0x3f000;
        if(sign(h.r(0x2c)-height)<0){h.w(0x2c,height);
            if(h.read(0x67f528)&&sign(h.r(0xac))<-0x1000){
                h.call(0x450d50,{actor});h.call(0x49a620,{actor+0x3c,0,0x18});h.call(0x49a620,{actor+0x54,0,0x18});
                const auto current=h.r(0x6c,2);h.w(0x6c,current|0x10c,2);
            }
        }
    }
    const auto first_target=h.r(0xcc),first_current=h.r(0xc4);auto delta=first_target-first_current;
    if(sign(absolute(delta))>0x1000000){if(sign(delta)>0)h.w(0xc6,h.r(0xc6,2)+0x80,2);else h.w(0xc4,h.r(0xc4)-0x800000);}
    const auto second_target=h.r(0xd0),second_current=h.r(0xc8);delta=second_target-second_current;
    if(sign(absolute(delta))>0x1000000){if(sign(delta)>0)h.w(0xca,h.r(0xca,2)+0x80,2);else h.w(0xc8,h.r(0xc8)-0x800000);}
    if(!h.read(0x67f528)||!target){
        const auto desired=sign(h.r(0x98))<0x14000000?0:h.r(0x88);
        mission_object_helo_speed(b,actor,desired);mission_object_helo_integrate(b,actor);
        h.w(0x34,(0u-h.r(0x7c))<<8);mission_object_helo_bank(b,actor);return;
    }
    mission_object_helo_bank(b,actor);const auto desired=h.r(0x88);mission_object_helo_speed(b,actor,desired);
    mission_object_helo_integrate(b,actor);h.w(0x34,(0u-h.r(0x7c))<<8);
    const auto current=h.r(0xc4);delta=h.r(0xcc)-current;if(sign(absolute(delta))>=0xc000000)return;
    const auto delay=h.read(person+0x90);if(sign(delay)>0){h.write(person+0x90,delay-1);return;}
    const auto count=h.read(person+0x6c);if(sign(count)>0){h.write(person+0x6c,count-1);if(!(h.read(person+0x6c,1)&7))h.call(0x450830,{actor});return;}
    const auto first=h.call(0x477860,{});h.write(person+0x90,(first&0x3f)+0x40);
    const auto second=h.call(0x477860,{});h.write(person+0x6c,(second&0x1f)+0x10);
    const auto value=h.r(0xb8),again=h.r(0xb8);h.w(0xb8,again-U(sign(value)>>2));
}
void bind_mission_object_helo_move(MissionTickState& state,MissionObjectHeloBindings b){
    if(state.secondary_callbacks.count(0x451590))throw MissionTickError("Helicopter move callback already has an owner");
    state.secondary_callbacks.emplace(0x451590,[b=std::move(b)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::secondary||action.target!=0x451590)throw MissionTickError("Helicopter callback received another body");
        static constexpr std::size_t capacities[]{1024,1024,512,256};
        if(ref.group>=4||ref.index>=capacities[ref.group]||!current.actor_at(ref))throw MissionTickError("Helicopter actor has no physical owner");
        mission_object_helo_move(b,mission_actor_source_address(ref.group,ref.index));
    });
}
} // namespace dfre
