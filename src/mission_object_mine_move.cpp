#include "dfre/mission_object_mine_move.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t sign(U n){std::int32_t s;std::memcpy(&s,&n,4);return s;}
}
void mission_object_mine_move(const MissionObjectMineMoveBindings& b,U actor){
    const auto read=[&](U offset){if(!b.read)throw MissionTickError("Mine movement read is not bound");return b.read(actor+offset,4);};
    const auto write=[&](U offset,U n){if(!b.write)throw MissionTickError("Mine movement store is not bound");b.write(actor+offset,4,n);};
    const auto dx=read(0x3c),x=read(0x24),y=read(0x28),dy=read(0x40),z=read(0x2c),speed=read(0x7c);
    write(0x24,x+dx);
    const U vertical=(speed>>7)|((speed&0x80000000u)?0xfe000000u:0);
    write(0x28,y+dy);
    const auto current_speed=read(0x7c);
    write(0x2c,z+vertical);
    const U step=sign(current_speed)<0x30000?0x3000000u:0x2000000u;
    if(read(0xe4)){
        const auto next=read(0xe0)+step;write(0xe0,next);
        if(sign(next)>0x18000000)write(0xe4,0);
    }else{
        const auto next=read(0xe0)-step;write(0xe0,next);
        if(sign(next)<-0x18000000)write(0xe4,1);
    }
}
void bind_mission_object_mine_move(MissionTickState& state,MissionObjectMineMoveBindings b){
    if(state.secondary_callbacks.count(0x451dd0))throw MissionTickError("Mine movement callback already has an owner");
    state.secondary_callbacks.emplace(0x451dd0,[b=std::move(b)](auto& current,const auto& ref,const auto& action){
        static constexpr std::size_t capacities[]{1024,1024,512,256};
        if(action.kind!=AiActorActionKind::secondary||action.target!=0x451dd0)throw MissionTickError("Mine movement received another callback");
        if(ref.group>=4||ref.index>=capacities[ref.group]||!current.actor_at(ref))throw MissionTickError("Mine movement actor has no physical owner");
        mission_object_mine_move(b,mission_actor_source_address(ref.group,ref.index));
    });
}
}
