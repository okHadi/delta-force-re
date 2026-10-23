#include "dfre/mission_object_mine_update.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t sign(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
U velocity(U speed,U trig){const auto product=std::int64_t(sign(speed))*sign(trig);const U shifted=U(std::uint64_t(product)>>22);return (shifted>>6)|((shifted&0x80000000u)?0xfc000000u:0);}
}
void mission_object_mine_update(const MissionObjectMineUpdateBindings& b,U actor,U frame){
    const auto read=[&](U at,U width=4){if(!b.read)throw MissionTickError("Mine update read is not bound");return b.read(at,width);};
    const auto write=[&](U at,U value,U width=4){if(!b.write)throw MissionTickError("Mine update store is not bound");b.write(at,width,value);};
    const auto call=[&](U site,const std::vector<U>& args){if(!b.call)throw MissionTickError("Mine update body is not bound");return b.call(site,args);};
    const auto speed=read(actor+0x7c);if(sign(speed)<0x30000)write(actor+0x7c,speed+0x4000);
    const auto angle=read(actor+0x30);call(0x477820,{angle,frame,frame+4});
    const auto first=read(actor+0x7c),cosine=read(frame+4);write(actor+0x3c,velocity(first,cosine));
    const auto sine=read(frame),second=read(actor+0x7c),height=read(actor+0x2c);write(actor+0x40,velocity(second,sine));
    if(sign(height)>0x640000)call(0x45da30,{actor});
    const auto flags=read(actor+0x6d,1);write(actor+0xfc,0x10);write(actor+0x6d,(flags|1)&255u,1);
}
void bind_mission_object_mine_update(MissionTickState& state,MissionObjectMineUpdateBindings b,U frame){
    if(state.update_callbacks.count(0x451d40))throw MissionTickError("Mine update callback already has an owner");
    state.update_callbacks.emplace(0x451d40,[b=std::move(b),frame](auto& current,const auto& ref,const auto& action){
        static constexpr std::size_t capacities[]{1024,1024,512,256};
        if(action.kind!=AiActorActionKind::update||action.target!=0x451d40)throw MissionTickError("Mine update received another callback");
        if(ref.group>=4||ref.index>=capacities[ref.group]||!current.actor_at(ref))throw MissionTickError("Mine update actor has no physical owner");
        mission_object_mine_update(b,mission_actor_source_address(ref.group,ref.index),frame);
    });
}
}
