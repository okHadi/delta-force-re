#include "dfre/mission_object_sound.hpp"
#include <cstring>
#include <utility>
namespace dfre { namespace {
std::pair<std::uint32_t,std::uint32_t> rate(std::uint32_t body){
    if(body==0x45c340)return {6,20};if(body==0x45c3a0)return {4,8};if(body==0x45c400)return {2,3};
    throw MissionTickError("Object sound body is not owned");
}
std::uint32_t word(const AiActorImage& image,std::size_t at){std::uint32_t n;std::memcpy(&n,image.data()+at,4);return n;}
}
void mission_object_sound(MissionTickState& state,const AiActorRef& ref,std::uint32_t body,const MissionObjectLiveBindings& b,std::uint32_t param2){
    const auto timing=rate(body);if(param2)return;
    const auto image=snapshot_mission_actor(state,ref);
    const auto z=word(image,0x2c),y=word(image,0x28),x=word(image,0x24),definition=word(image,4);
    if(!b.definition)throw MissionTickError("Object sound needs its current definition138 owner");
    const auto sound=b.definition(state,definition,0x138);
    if(!b.call)throw MissionTickError("Object sound requires43E7D0");
    ObjectBehaviorAction action;action.site=0x43e7d0;action.args={sound,x,y,z,0,0};b.call(state,ref,action);
    ObjectBehaviorState rng;rng.rng=state.behavior.rng;const auto draw=object_behavior_rng_next(rng);state.behavior.rng=rng.rng;
    const auto delay=((draw*timing.first>>16)+timing.second)<<6;
    auto current=snapshot_mission_actor(state,ref);std::memcpy(current.data()+0xfc,&delay,4);apply_mission_actor_image(state,ref,current);
}
void bind_mission_object_sound(MissionTickState& state,std::uint32_t body,MissionObjectLiveBindings b){
    rate(body);if(state.update_callbacks.count(body))throw MissionTickError("Object sound callback already has an owner");
    state.update_callbacks.emplace(body,[body,b=std::move(b)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=body)throw MissionTickError("Object sound callback received a different body");
        mission_object_sound(current,ref,body,b,0);
    });
}
}
