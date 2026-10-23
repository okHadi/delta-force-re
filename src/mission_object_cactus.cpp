#include "dfre/mission_object_cactus.hpp"
#include <cstring>
#include <utility>
namespace dfre { namespace {
std::uint16_t half(const AiActorImage& image,std::size_t at){std::uint16_t n;std::memcpy(&n,image.data()+at,2);return n;}
void delay(MissionTickState& state,const AiActorRef& ref,std::uint32_t ticks,bool destroyed){
    auto image=snapshot_mission_actor(state,ref);const auto flags=image[0x6c];
    std::memcpy(image.data()+0xfc,&ticks,4);if(destroyed)image[0x6c]=std::uint8_t(flags|0x0c);
    apply_mission_actor_image(state,ref,image);
}
}
void mission_object_cactus(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& b,std::uint32_t param2){
    if(!state.actor_at(ref))throw MissionTickError("Cactus actor has no current owner");
    const auto call=[&](std::uint32_t site,std::uint32_t value=0){
        if(!b.call)throw MissionTickError("Cactus foreign body is not bound "+std::to_string(site));
        ObjectBehaviorAction action;action.site=site;action.args[0]=actor_handle(ref.group,ref.index);action.args[1]=value;
        b.call(state,ref,action);
    };
    if(state.globals.mission_67f528==0){
        if(param2!=4)return;
        call(0x44df90);delay(state,ref,0x400,true);return;
    }
    const auto image=snapshot_mission_actor(state,ref);
    if(image[0x6c]&4){delay(state,ref,0x400,false);return;}
    if(static_cast<std::int16_t>(half(image,0x74))>0){delay(state,ref,0x780,false);return;}
    call(0x44df90);
    if(!b.global)throw MissionTickError("Cactus needs its currentA158C0 owner");
    const auto value=b.global(state,0xa158c0);call(0x447980,value);
    delay(state,ref,0x400,true);
}
void bind_mission_object_cactus(MissionTickState& state,MissionObjectLiveBindings b){
    if(state.update_callbacks.count(0x4534a0))throw MissionTickError("Cactus callback already has an owner");
    state.update_callbacks.emplace(0x4534a0,[b=std::move(b)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=0x4534a0)throw MissionTickError("Cactus callback received a different body");
        mission_object_cactus(current,ref,b,0);
    });
}
}
