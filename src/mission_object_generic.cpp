#include "dfre/mission_object_generic.hpp"
#include <utility>
namespace dfre {namespace {
AiActorState& actor(MissionTickState& state,const AiActorRef& ref){auto* a=state.actor_at(ref);if(!a)throw MissionTickError("Generic actor has no current owner");return *a;}
void mark(MissionTickState& state,const AiActorRef& ref,std::uint32_t delay,std::uint8_t mask){auto& a=actor(state,ref);const auto flags=a.field_6c;a.field_fc=delay;a.field_6c=std::uint8_t(flags|mask);}
}
void mission_object_generic(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& b,std::uint32_t message){
 const auto call=[&](std::uint32_t site,std::uint32_t value=0){actor(state,ref);if(!b.call)throw MissionTickError("Generic foreign body is not bound "+std::to_string(site));ObjectBehaviorAction action;action.site=site;action.args[0]=actor_handle(ref.group,ref.index);action.args[1]=value;b.call(state,ref,action);};
 if(!state.globals.mission_67f528){if(message!=4)return;call(0x453270);mark(state,ref,0x400,0x0c);return;}
 const auto flags=actor(state,ref).field_6c;
 if(flags&8){actor(state,ref).field_fc=0x400;return;}
 if(flags&4){
  call(0x453270);if(!b.global)throw MissionTickError("Generic actor needs currentA158C0");
  const auto value=b.global(state,0xa158c0);call(0x447980,value);mark(state,ref,0x20,8);return;
 }
 const auto health=state.extra_for(ref).field_74;
 if(health>0 && health<0x8000){actor(state,ref).field_fc=0x780;return;}
 auto& a=actor(state,ref);a.field_fc=0x20;a.field_6c=std::uint8_t(flags|4);
}
void bind_mission_object_generic(MissionTickState& state,MissionObjectLiveBindings b){
 if(state.update_callbacks.count(0x4533f0))throw MissionTickError("Generic callback already has an owner");
 state.update_callbacks.emplace(0x4533f0,[b=std::move(b)](auto& current,const auto& ref,const auto& action){if(action.kind!=AiActorActionKind::update||action.target!=0x4533f0)throw MissionTickError("Generic callback received a different body");mission_object_generic(current,ref,b,0);});
}
}
