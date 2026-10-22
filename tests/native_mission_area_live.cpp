#include "dfre/mission_area_live.hpp"
#include <stdexcept>
#include <string>

using namespace dfre;
namespace {
void check(bool value) {if(!value)throw std::runtime_error("Mission area live test failed");}
MissionStageCall call(std::uint32_t address) {MissionStageCall result;result.address=address;return result;}
void init(MissionTickState& state,std::size_t people=1,std::size_t objects=0) {
    AiActorTables tables;tables.groups[3].resize(people);tables.groups[0].resize(objects);
    for(auto group:{3u,0u})for(auto& actor:tables.groups[group]) {
        actor.active=true;actor.definition_index=group==3?1:2;actor.field_10=17;actor.field_110=0x1234;actor.field_70=1;
    }
    std::vector<AiItemDefinition> definitions(3);definitions[1].field_28=3;definitions[1].field_84=0x1234;
    definitions[2].field_28=0;definitions[2].field_84=0x1234;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},123);
    state.globals.mission_67f528=1;
    for(auto group:{3u,0u})for(std::size_t slot=0;slot<state.tables.groups[group].size();++slot)state.extra_for({std::uint8_t(group),slot}).field_74=100;
}
MissionGeometry geometry(Model& model,std::int32_t height=0) {
    model.version=5;model.lods.resize(1);MissionGeometryModel binding;binding.handle=17;binding.model=&model;
    binding.bounds={0,0};binding.height_68=height;return MissionGeometry({binding});
}
void live_callbacks() {
    MissionTickState state;init(state);state.config.local_actor=0;
    Model model;auto shape=geometry(model);MissionAreaLiveGlobals globals;
    MissionAreaLiveBindings b;b.globals=[&](const auto&){return globals;};
    unsigned queries=0,updates=0,hits=0;
    b.segment=[&](MissionTickState& live,const AiActorRef& ref,const Fixed3& start,const Fixed3& end) {
        check(ref.group==3&&ref.index==0&&start.x==0&&end.z==0x8000);++queries;
        live.tables.groups[3][0].position[0]=65536;live.extra_for(ref).field_74=99;
        live.world.water_level=1;live.behavior.rng=0x12345678;return 1;
    };
    b.update=[&](MissionTickState& live,const AiActorRef& ref,auto callback,auto message) {
        check(callback==0x1234);++updates;
        if(updates==1) {
            check(message==2&&live.extra_for(ref).field_74==95&&live.extra_for(ref).field_84==0xfe);
            live.extra_for(ref).field_74=77;live.tables.groups[3][0].position[0]=333;
            live.pending_spawns.push_back({{0,0,0},4,0});
        } else check(message==5&&live.extra_for(ref).field_74==76);
    };
    b.local_hit=[&](MissionTickState& live){check(updates==1&&live.extra_for({3,0}).field_74==77&&live.tables.groups[3][0].position[0]==333);++hits;};
    state.pending_spawns.push_back({{0,0,0},3,0});
    check(mission_area_live_stage(state,shape,call(0x465bd0),b)==0);
    check(queries==1&&updates==2&&hits==1&&state.pending_spawns.empty()&&state.extra_for({3,0}).field_74==76);
    check(state.behavior.rng==0x12345678&&state.runtime_for({3,0}).byte_6e==0x10);
}
void live_rules() {
    MissionTickState state;init(state,2);Model model;auto shape=geometry(model);MissionAreaLiveGlobals globals;
    MissionAreaLiveBindings b;b.globals=[&](const auto&){return globals;};unsigned updates=0;
    b.update=[&](MissionTickState& live,const AiActorRef& ref,auto,auto message) {
        check(ref.index==1&&message==3);++updates;globals.rules_67f549=0x40;
        live.pending_spawns.push_back({{0,0,0},4,actor_handle(3,0)});
    };
    state.pending_spawns.push_back({{0,0,0},2,actor_handle(3,0)});
    mission_area_live_stage(state,shape,call(0x465bd0),b);
    check(updates==1&&state.extra_for({3,1}).field_74==92&&state.pending_spawns.empty());
}
void exceptions() {
    MissionTickState state;init(state);Model model;auto shape=geometry(model);MissionAreaLiveBindings b;
    b.globals=[](const auto&){return MissionAreaLiveGlobals{};};
    state.pending_spawns.push_back({{0,0,0},2,0});bool rejected=false;
    try {mission_area_live_stage(state,shape,call(0x465bd0),b);}catch(const MissionTickError&){rejected=true;}
    check(rejected&&state.extra_for({3,0}).field_74==92&&state.pending_spawns.size()==1);
    state.extra_for({3,0}).field_74=100;
    b.update=[](MissionTickState& live,const AiActorRef& ref,auto,auto) {
        check(live.extra_for(ref).field_74==92);live.extra_for(ref).field_74=66;
        live.tables.groups[3][0].position[0]=333;live.behavior.rng=0xabcdef12;
        throw std::runtime_error("update stopped");
    };
    rejected=false;
    try {mission_area_live_stage(state,shape,call(0x465bd0),b);}catch(const std::runtime_error& e){rejected=std::string(e.what())=="update stopped";}
    check(rejected&&state.extra_for({3,0}).field_74==66&&state.tables.groups[3][0].position[0]==333&&state.behavior.rng==0xabcdef12&&state.pending_spawns.size()==1);
    state.pending_spawns[0].type=3;state.tables.groups[3][0].field_6c=0;
    b.segment=[](MissionTickState& live,const AiActorRef& ref,const auto&,const auto&)->std::int32_t {
        live.extra_for(ref).field_74=55;throw std::runtime_error("query stopped");
    };
    rejected=false;
    try {mission_area_live_stage(state,shape,call(0x465bd0),b);}catch(const std::runtime_error& e){rejected=std::string(e.what())=="query stopped";}
    check(rejected&&state.extra_for({3,0}).field_74==55&&state.pending_spawns.size()==1);
    state.config.local_actor=0;b.segment=[](auto&,const auto&,const auto&,const auto&){return 1;};
    b.update=[](MissionTickState& live,const AiActorRef& ref,auto,auto){live.extra_for(ref).field_74=77;};
    b.local_hit=[](MissionTickState& live){live.tables.groups[3][0].position[0]=222;throw std::runtime_error("local hit stopped");};
    rejected=false;
    try {mission_area_live_stage(state,shape,call(0x465bd0),b);}catch(const std::runtime_error& e){rejected=std::string(e.what())=="local hit stopped";}
    check(rejected&&state.extra_for({3,0}).field_74==77&&state.tables.groups[3][0].position[0]==222&&state.pending_spawns.size()==1);
}
void bounds_and_listener() {
    MissionTickState state;init(state,0,1);Model model;auto shape=geometry(model,0x40000);
    state.tables.groups[0][0].position[2]=0x40000;MissionAreaLiveBindings b;
    b.globals=[](const auto&){return MissionAreaLiveGlobals{};};unsigned updates=0;
    b.segment=[](auto&,const auto&,const auto&,const auto&){return 1;};
    b.update=[&](auto&,const auto& ref,auto,auto){check(ref.group==0);++updates;};
    state.pending_spawns.push_back({{0,0,0},1,0});mission_area_live_stage(state,shape,call(0x465bd0),b);
    check(updates==1&&state.extra_for({0,0}).field_74==68);
    MissionTickState listener;init(listener);listener.config.local_actor=0;listener.tables.groups[3][0].position[0]=88;
    unsigned listeners=0;b.listener=[&](MissionTickState& live,const auto& pose){check(pose.position.x==88);++listeners;live.tables.groups[3][0].position[0]=99;};
    mission_area_live_stage(listener,shape,call(0x45e050),b);
    check(listeners==1&&listener.tables.groups[3][0].position[0]==99);
    b.listener=[](MissionTickState& live,const auto& pose){check(pose.position.x==99);live.tables.groups[3][0].position[0]=111;throw std::runtime_error("listener stopped");};
    bool rejected=false;
    try {mission_area_live_stage(listener,shape,call(0x45e050),b);}catch(const std::runtime_error& e){rejected=std::string(e.what())=="listener stopped";}
    check(rejected&&listener.tables.groups[3][0].position[0]==111);
    check(!mission_area_live_stage(listener,shape,call(0x1234),{}));
    listener.globals.mission_67f528=0;listener.pending_spawns.push_back({{0,0,0},1,0});
    check(mission_area_live_stage(listener,shape,call(0x465bd0),{})==0&&listener.pending_spawns.size()==1);
}
}
int main(){try{live_callbacks();live_rules();exceptions();bounds_and_listener();}catch(...){return 1;}return 0;}
