#include "dfre/mission_object_wood_small.hpp"
#include <utility>
namespace dfre {namespace {
AiActorState& actor(MissionTickState& state,const AiActorRef& ref){auto* a=state.actor_at(ref);if(!a)throw MissionTickError("Small wood actor has no current owner");return *a;}
void destroyed(MissionTickState& state,const AiActorRef& ref){auto& a=actor(state,ref);const auto flags=a.field_6c;a.field_fc=0x400;a.field_6c=std::uint8_t(flags|0x0c);}
}
void mission_object_wood_small(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& b,std::uint32_t message){
 const auto call=[&](std::uint32_t site,std::uint32_t value=0){
  actor(state,ref);
  if(!b.call)throw MissionTickError("Small wood foreign body is not bound "+std::to_string(site));
  ObjectBehaviorAction action;action.site=site;action.args[0]=actor_handle(ref.group,ref.index);action.args[1]=value;b.call(state,ref,action);
 };
 if(!state.globals.mission_67f528){if(message!=4)return;call(0x44ed40);destroyed(state,ref);return;}
 if(actor(state,ref).field_6c&4){actor(state,ref).field_fc=0x400;return;}
 const auto health=state.extra_for(ref).field_74;
 if(health>0 && health<0x8000){actor(state,ref).field_fc=0x780;return;}
 call(0x44ed40);
 if(!b.global)throw MissionTickError("Small wood needs currentA158C0");
 const auto value=b.global(state,0xa158c0);call(0x447980,value);destroyed(state,ref);
}
void bind_mission_object_wood_small(MissionTickState& state,MissionObjectLiveBindings b){
 if(state.update_callbacks.count(0x453810))throw MissionTickError("Small wood callback already has an owner");
 state.update_callbacks.emplace(0x453810,[b=std::move(b)](auto& current,const auto& ref,const auto& action){
  if(action.kind!=AiActorActionKind::update||action.target!=0x453810)throw MissionTickError("Small wood callback received a different body");
  mission_object_wood_small(current,ref,b,0);
 });
}
}
