#include "dfre/mission_object_barrel.hpp"
#include "dfre/world_runtime.hpp"
#include <utility>
namespace dfre {
void mission_object_barrel(const MissionObjectBarrelBindings& b,std::uint32_t actor,
        std::uint32_t frame,std::uint32_t message){
    (void)message;
    const auto read=[&](auto address,auto width){if(!b.read)throw MissionTickError("Barrel current memory is not bound");return b.read(address,width);};
    const auto write=[&](auto address,auto width,auto value){if(!b.write)throw MissionTickError("Barrel current store is not bound");b.write(address,width,value);};
    const auto call=[&](auto address,const std::vector<std::uint32_t>& args){if(!b.call)throw MissionTickError("Barrel required call is not bound");return b.call(address,args);};
    if(!read(0x67f528u,4u))return;
    const auto flags=read(actor+0x6c,1u);
    if(!(flags&4u)){
        const auto health=read(actor+0x74,2u);
        if((health&0x8000u)||!(health&0xffffu)){
            write(actor+0xfc,4u,10u);
            write(actor+0x6c,1u,flags|4u);
        }else write(actor+0xfc,4u,0x780u);
        return;
    }
    for(std::uint32_t offset=0;offset<24;offset+=4){const auto word=read(actor+0x24+offset,4u);write(frame+offset,4u,word);}
    call(0x446fb0u,{0,frame,10});
    const auto z=read(actor+0x2c,4u);
    const auto y=read(actor+0x28,4u);
    const auto x=read(actor+0x24,4u);
    call(0x447610u,{0,x,y,z});
    call(0x4471d0u,{actor});
}
void bind_mission_object_barrel(MissionTickState& state,MissionObjectBarrelBindings b,
        std::uint32_t frame){
    if(state.update_callbacks.count(0x45bf80))throw MissionTickError("Barrel callback already has an owner");
    state.update_callbacks.emplace(0x45bf80,[b=std::move(b),frame](auto& current,const auto& ref,const auto& action){
        static constexpr std::size_t capacities[]{1024,1024,512,256};
        if(action.kind!=AiActorActionKind::update||action.target!=0x45bf80)throw MissionTickError("Barrel received another callback");
        if(ref.group>=4||ref.index>=capacities[ref.group]||!current.actor_at(ref))throw MissionTickError("Barrel actor has no physical owner");
        mission_object_barrel(b,mission_actor_source_address(ref.group,ref.index),frame);
    });
}
}
