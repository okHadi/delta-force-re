#include "dfre/mission_object_update.hpp"
#include "dfre/mission_event_actor_flags.hpp"
#include "dfre/mission_object_sound.hpp"
#include <cassert>
using namespace dfre;
int main(){MissionTickState s;AiActorTables tables;tables.groups[0].resize(1);auto& a=tables.groups[0][0];a.active=true;a.definition_index=1;a.person_slot=0;a.field_72=0xffff;a.field_110=0x45c3a0;init_mission_tick(s,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(1),123);s.globals.mission_67f528=1;
 unsigned damage=99,effects=0;MissionObjectLiveBindings object;object.global=[](auto&,unsigned at){assert(at==0xa158c0);return 7u;};object.call=[&](auto& current,const auto&,const ObjectBehaviorAction& action)->MissionObjectCallResult{assert(damage==0&&current.extra_for({0,0}).field_74==0xffce);assert(action.site==(effects==0?0x44df90:0x447980));if(effects==1)assert(action.args[1]==7);++effects;return {};};
 MissionEventActorFlagBindings event;event.before_write=[&](unsigned,unsigned width,unsigned){if(width==2)s.tables.groups[0][0].field_110=0x4534a0;};event.update=[&](const auto& ref,unsigned callback,unsigned message){assert(callback==0x4534a0&&message==1);s.tables.groups[0][0].field_110=0x45c3a0;mission_object_update(s,ref,callback,message,object);};mission_event_actor_flags(s,0xffffffff,damage,event);assert(effects==2&&s.tables.groups[0][0].field_110==0x45c3a0&&(s.tables.groups[0][0].field_6c&0xc)==0xc&&s.tables.groups[0][0].field_fc==0x400);
 // Event message1 reaches the sound body's early return without data providers.
 event.before_write={};event.update=[&](const auto& ref,unsigned callback,unsigned message){mission_object_update(s,ref,callback,message,{});};const auto rng=s.behavior.rng;mission_event_actor_flags(s,0xffffffff,damage,event);assert(s.behavior.rng==rng&&effects==2);
 bool failed=false;try{mission_object_update(s,{0,0},0x123456,1,{});}catch(const MissionTickError&){failed=true;}assert(failed);
 for(auto message:{1u,4u,0x80000000u,0xffffffffu})mission_object_update(s,{4,999},0x45c3a0,message,{});
 // The registered frame call retains message0 and reaches its real sound body.
 unsigned starts=0;MissionObjectLiveBindings sound;sound.definition=[](auto&,unsigned i,unsigned at){assert(i==1&&at==0x138);return 77u;};sound.call=[&](auto&,const auto&,const ObjectBehaviorAction& action)->MissionObjectCallResult{assert(action.site==0x43e7d0&&action.args[0]==77);++starts;return {};};bind_mission_object_sound(s,0x45c3a0,sound);MissionTickReport report;run_actor_action(s,{AiActorActionKind::update,{0,0},0x45c3a0},report);assert(starts==1&&report.callbacks_run==1);
 // Existing three-argument entry points remain frame-message0 calls.
 s.globals.mission_67f528=0;s.tables.groups[0][0].field_7c=0;auto image=snapshot_mission_actor(s,{0,0});for(unsigned i=0;i<4;++i)image[0x1c+i]=0;apply_mission_actor_image(s,{0,0},image);mission_object_veh0(s,{0,0},{});assert(s.tables.groups[0][0].field_fc==0x10);mission_object_slgt_ai(s,{0,0},{});assert(s.tables.groups[0][0].field_fc==0x400);
 s.globals.mission_67f528=1;s.tables.groups[0][0].field_6c=0;s.tables.groups[0][0].field_110=0x4533f0;mission_event_actor_flags(s,0xffffffff,damage,event);assert(s.tables.groups[0][0].field_6c==4&&s.tables.groups[0][0].field_fc==0x20);
 // Event message1 reaches the large-wood destruction body through the dispatcher.
 unsigned wood_calls=0;MissionObjectLiveBindings wood;wood.global=[](auto&,unsigned at){assert(at==0xa158c0);return 19u;};
 wood.call=[&](auto&,const auto&,const ObjectBehaviorAction& action)->MissionObjectCallResult{assert(action.site==(wood_calls==0?0x44f270:0x447980));if(wood_calls)assert(action.args[1]==19);++wood_calls;return {};};
 s.tables.groups[0][0].field_6c=0;s.tables.groups[0][0].field_110=0x453930;event.update=[&](const auto& ref,unsigned callback,unsigned message){mission_object_update(s,ref,callback,message,wood);};
 mission_event_actor_flags(s,0xffffffff,damage,event);assert(wood_calls==2&&s.tables.groups[0][0].field_6c==0xc&&s.tables.groups[0][0].field_fc==0x400);
 s.globals.mission_67f528=0;mission_object_update(s,{4,999},0x4533f0,1,{});
}
