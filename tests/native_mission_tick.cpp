#include "dfre/mission_tick.hpp"
#include <iostream>
#include <cstring>
#include <stdexcept>
#define CHECK(condition) do{if(!(condition))throw std::runtime_error("mission tick: " #condition);}while(false)
using namespace dfre;
void seed(MissionTickState& state,bool paused=false){
    MissionTickConfig config;config.paused=paused;
    AiActorTables tables;
    init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable{}, {},0x12345678);
    state.world.external_stage=[](auto& current,const auto& call)->std::optional<std::uint32_t>{
        if(call.address==0x482b00)mission_weapon_part(current,false);return 0;
    };
    state.world.trigger_held_47a290=[](std::uint32_t command){CHECK(command==0x8c);return false;};
    state.world.weapon_effect=[](auto&,const auto&){return true;};
    state.world.bounds=[](std::uint32_t){return AiActorBounds{};};
}
int main(){
    {
        MissionTickState state;seed(state,true);state.mission_timer_67dbe8=10;
        auto first=mission_tick(state,{});CHECK(first.paused);CHECK(first.tick==1);CHECK(state.globals.command_tick_6e471c==1);CHECK(state.tick_6e4720==0);CHECK(state.tick_6e4724==0);CHECK(state.mission_timer_67dbe8==10);
        CHECK(state.globals.mission_67f528==1);
        state.globals.pause_6e4714=0;auto second=mission_tick(state,{});CHECK(second.tick==2);CHECK(state.globals.command_tick_6e471c==2);CHECK(state.tick_6e4720==1);CHECK(state.tick_6e4724==1);CHECK(state.mission_timer_67dbe8==9);
    }
    {
        MissionTickState state;seed(state,true);state.weapons.set_slot_map({0,0,0,1,2,0,0,0,0,0});state.weapons.resupply(false);state.weapons.select_slot(3);
        MissionTickInputs input;input.commands={{1,0},{2,0}};int calls=0;
        input.dispatch_command=[&](std::uint32_t id,std::int32_t){
            ++calls;DispatchResult result;
            if(id==1)result.calls.push_back({DispatchCallKind::weapon_select,0x483dd0,{4},false});
            else CHECK(state.weapons.slot()==4);
            return result;
        };
        mission_tick(state,input);CHECK(calls==2);CHECK(state.weapons.slot()==4);
    }
    {
        MissionTickState state;seed(state);state.tables.groups[0].resize(1);
        auto& actor=state.tables.groups[0][0];actor.active=true;actor.definition_index=1;actor.field_fc=0;actor.field_110=0x1234;
        // Definition type zero rebuilds no target list.
        state.definitions=AiItemDefinitionTable({AiItemDefinition{},AiItemDefinition{1}});
        state.tick_67dbf4=1;
        int primary=0,secondary=0;
        register_actor_callback(state,0x1234,false,[&](auto& s,const auto& ref,const auto&){++primary;CHECK(s.actor_at(ref)->field_fc==0);s.actor_at(ref)->field_fc=8;s.actor_at(ref)->field_10c=0x5678;});
        register_actor_callback(state,0x5678,true,[&](auto& s,const auto& ref,const auto&){++secondary;CHECK(s.actor_at(ref)->field_fc==8);s.actor_at(ref)->field_fc=10;});
        mission_tick(state,{});CHECK(primary==1 && secondary==1);CHECK(state.tables.groups[0][0].field_fc==9);
    }
    {
        MissionTickState state;seed(state);state.tables.groups[3].resize(2);state.people.resize(1);state.person_extras.resize(1);
        auto& actor=state.tables.groups[3][0];actor.active=true;actor.definition_index=1;actor.person_slot=0;
        auto& other=state.tables.groups[3][1];other.field_fc=0;
        MissionTickReport report,previous;state.current_report=&previous;
        int old_query=0,old_effect=0,query=0,effects=0;
        state.behavior.on_query=[&](auto a){++old_query;return BehaviorInput{a.site,0,0};};
        state.behavior.on_effect=[&](auto){++old_effect;};
        state.world.behavior_query.answer=[&](const auto&,auto site,const auto&,const auto&){++query;return std::optional<BehaviorInput>{{site,190,0}};};
        state.world.behavior_query.effect=[&](const auto&,const BehaviorAction& action,BehaviorActor& view){
            ++effects;
            if(action.site==behavior_site::kSetAnimation){CHECK(view.actor_extra.field_84==190);other.field_fc+=1;}
            else{CHECK(action.site==behavior_site::kNotifyEvent11);CHECK(other.field_fc==1);CHECK(view.actor.field_6c&4);}
            return true;
        };
        run_actor_action(state,{AiActorActionKind::update,{3,0},0x45b5b0},report);
        CHECK(query==1 && effects==2 && other.field_fc==1);CHECK(report.callbacks_run==1);CHECK(state.current_report==&previous);
        state.behavior.on_query({BehaviorCallKind::query,1,{}});state.behavior.on_effect({});CHECK(old_query==1 && old_effect==1);
        // A missing effect stops the callback and restores provider/report scopes.
        actor.field_6c=0;state.extra_for({3,0}).field_84=0;state.world.behavior_query.effect={};
        bool failed=false;try{run_actor_action(state,{AiActorActionKind::update,{3,0},0x45b5b0},report);}catch(const MissionTickError&){failed=true;}
        CHECK(failed);CHECK(state.current_report==&previous);CHECK(state.extra_for({3,0}).field_84==190);
        state.behavior.on_effect({});CHECK(old_effect==2);
        state.globals.mission_67f528=0;actor.field_fc=0;
        run_actor_action(state,{AiActorActionKind::update,{3,0},0x45b5b0},report);CHECK(actor.field_fc==0x400);
    }
    {
        MissionTickState state;seed(state);state.tables.groups[3].resize(1);state.people.resize(1);state.person_extras.resize(1);
        auto& actor=state.tables.groups[3][0];actor.active=true;actor.definition_index=1;actor.person_slot=0;
        state.extra_for({3,0}).field_74=1;state.people[0].field_64=4;state.person_extras[0].field_6c=1;
        // CombatResult reads current person state. WeaponStep reads the real +9C table field.
        bind_mission_actor_queries(state,[](const auto&){return ActorQueryData{};},{});
        MissionTickReport report;
        run_actor_action(state,{AiActorActionKind::update,{3,0},0x45b5b0},report);
        CHECK(actor.field_fc==std::uint32_t(weapon_definitions()[4].fire_delay));
        CHECK(report.behavior_queries==2);
        CHECK(state.current_report==nullptr);
        CHECK(state.movement.jump_impulse_c2382c==0x1111);
    }
    {
        MissionRuntimeState initial;initial.definitions.resize(3);initial.people.resize(1);initial.groups[3].resize(1);initial.health[3]={23};initial.group3_count=2;
        auto& converted=initial.groups[3][0];converted.active=true;converted.definition_index=1;converted.person_slot=0;converted.position[0]=0x12345678;
        initial.actors.emplace(ActorDefinitionTable{},ActorFactoryConfig{});
        auto& created=initial.actors->actor(1);created.definition_index=2;created.person=person_handle(0);created.argument=0xabcddcba;created.weapon_slot=4;created.health=56;
        MissionTickState state;init_mission_tick(state,{},initial,123);
        CHECK(snapshot_mission_actors(state)==initial_actor_images(initial));
        state.tables.groups[3][1].position[0]=0x20000;state.extra_for({3,1}).field_74=11;
        state.runtime_for({3,1}).vertical_44=-123;state.runtime_for({3,1}).byte_6e=7;
        const auto images=snapshot_mission_actors(state);
        std::uint32_t face;std::memcpy(&face,images[3][1].data()+0x98,4);CHECK(face==0xabcddcba);
        std::uint32_t position;std::memcpy(&position,images[3][1].data()+0x24,4);CHECK(position==0x20000);
        CHECK(images[3][1][0x6e]==7 && images[3][1][0x74]==11);
        CHECK(snapshot_mission_actor(state,{3,1})==images[3][1]);
    }
    {
        MissionTickState state;seed(state);state.frame_event_67dbf0=99;
        bool failed=false;try{mission_frame(state,{});}catch(const MissionTickError&){failed=true;}
        CHECK(failed);CHECK(state.tick_67dbf4==0 && state.frame_event_67dbf0==99);
        const auto stopped=mission_frame(state,[](auto&,const auto&)->std::optional<std::uint32_t>{return 1;});
        CHECK(stopped.result==1 && stopped.exit_reason==MissionFrameExit::early_stop);
        CHECK(stopped.frame_calls.size()==1 && state.frame_event_67dbf0==99);
        auto provider=[](auto& current,const MissionStageCall& call)->std::optional<std::uint32_t>{
            if(call.address==0x4562a0){CHECK(call.args[0]==99);current.frame_event_67dbf0=123;}
            if(call.address==0x479450){current.globals.menu_state_67dbbc=4;current.shutdown_503424=1;}
            return 0;
        };
        const auto menu=mission_frame(state,provider);
        CHECK(menu.result==0 && menu.frame_calls.back().address==0x404e30);
        CHECK(state.frame_event_67dbf0==0);
    }
    {
        MissionTickState state;seed(state,true);state.message_idle_bc5d20=10;
        unsigned polls=0,handlers=0,flush=0;
        state.world.external_stage=[&](auto& current,const MissionStageCall& call)->std::optional<std::uint32_t>{
            if(call.address==0x482b00)mission_weapon_part(current,false);
            if(call.address==0x461050){++polls;return polls<=2?1:0;}
            if(call.address==0x46d9f0){++handlers;CHECK(current.message_idle_bc5d20==(handlers==1?11u:0u));}
            if(call.address==0x46f4b0)++flush;
            return 0;
        };
        const auto report=mission_tick(state,{});
        CHECK(polls==3 && handlers==2 && flush==1 && state.message_idle_bc5d20==0);
        auto command=report.trace.end(),weapon=report.trace.end();
        for(auto it=report.trace.begin();it!=report.trace.end();++it){
            if(it->address==0x479450)command=it;
            if(it->address==0x482b00)weapon=it;
        }
        CHECK(command!=report.trace.end() && weapon!=report.trace.end() && command<weapon);
    }
    {
        MissionTickState state;seed(state);state.tables.groups[0].resize(1);
        MissionTickReport report;bool failed=false;
        try{run_actor_action(state,{AiActorActionKind::update,{0,0},0xdeadbeef},report);}
        catch(const MissionTickError&){failed=true;}
        CHECK(failed && report.callbacks_missing==1);
        CHECK(state.current_report==nullptr);
    }
    {
        MissionTickState state;seed(state,true);MissionTickInputs input;DispatchResult dispatched;
        dispatched.calls={{DispatchCallKind::dialog,0x404cf0,{1,2,3,4},false},
                          {DispatchCallKind::ui_button,0x404ba0,{dispatch_arg_created_handle,9},true}};
        input.dispatch.push_back(dispatched);unsigned calls=0;
        state.world.dispatch_effect=[&](auto&,const DispatchCall& call)->std::optional<std::uint32_t>{
            ++calls;
            if(call.address==0x404cf0){CHECK(call.arguments.size()==4);return 123;}
            CHECK(call.arguments[0]==123 && call.arguments[1]==9);return 0;
        };
        mission_tick(state,input);CHECK(calls==2);
        state.world.dispatch_effect={};bool failed=false;
        try{mission_tick(state,input);}catch(const MissionTickError&){failed=true;}
        CHECK(failed);
    }
    {
        MissionTickState state;seed(state,true);state.scope_c23a24=6;
        std::size_t scoped=0,plain=0;
        for(std::size_t i=0;i<kWeaponCount;++i){if(weapon_definitions()[i].flags&1)scoped=i;else plain=i;}
        const auto select=[&](std::size_t weapon){std::array<std::uint8_t,10> slots{};slots.fill(std::uint8_t(weapon));state.weapons.set_slot_map(slots);};
        select(scoped);CHECK(mission_steady(state)==6);
        state.globals.control_c23a00=0x2;CHECK(mission_steady(state)==0);
        state.globals.control_c23a00=0;state.weapons.toggle_aim_block();CHECK(mission_steady(state)==0);
        state.weapons.toggle_aim_block();select(plain);CHECK(mission_steady(state)==0);
        select(scoped);MissionTickInputs input;DispatchResult result;
        result.calls.push_back({DispatchCallKind::weapon_unknown,0x4847c0,{},false});input.dispatch.push_back(result);
        mission_tick(state,input);CHECK(state.scope_c23a24==7);CHECK(mission_steady(state)==7);
    }
    {
        MissionTickState state;seed(state,true);state.tables.groups[3].resize(1);state.config.local_actor=0;
        MissionTickInputs input;input.commands={{1,0},{2,0}};
        state.world.dispatch_effect=[](auto& current,const auto&)->std::optional<std::uint32_t>{
            current.tables.groups[3][0].angle_yaw=55;return 0;
        };
        input.dispatch_command=[&](std::uint32_t id,std::int32_t){
            DispatchResult result;
            if(id==1)result.calls.push_back({DispatchCallKind::actor_call,0x4604c0,{},false});
            else CHECK(state.dispatch_actor.look_yaw_30==55);
            return result;
        };
        mission_tick(state,input);CHECK(state.tables.groups[3][0].angle_yaw==55);
    }
    {
        MissionTickState state;seed(state,true);std::array<std::uint8_t,kWeaponSlotCount> slots{};slots.fill(4);
        state.weapons.set_slot_map(slots);state.weapons.resupply(false);
        MissionTickInputs input;DispatchResult result;
        result.calls.push_back({DispatchCallKind::weapon_fire,0x483c70,{},false});input.dispatch.push_back(result);
        state.world.weapon_effect={};bool failed=false;unsigned prior=0,prior_query=0;
        state.weapons.trigger_query=[&]{++prior_query;return true;};
        state.weapons.on_event=[&](const auto&){++prior;};
        try{mission_tick(state,input);}catch(const MissionTickError&){failed=true;}
        CHECK(failed && prior==0 && state.current_report==nullptr);
        state.weapons.on_event({WeaponEventKind::sound,0});CHECK(prior==1);
        CHECK(state.weapons.trigger_query() && prior_query==1);
    }
    {
        MissionTickState state;seed(state,true);std::array<std::uint8_t,kWeaponSlotCount> slots{};slots.fill(4);
        state.weapons.set_slot_map(slots);state.weapons.resupply(false);unsigned shots=0;
        const auto weapon_class=weapon_definitions()[4].weapon_class;
        state.world.weapon_effect=[&](auto& current,const WeaponEvent& event){
            if(event.kind==WeaponEventKind::shot){
                ++shots;CHECK(current.weapons.pending(weapon_class)==1);CHECK(current.weapons.fire_timer()==0);
                CHECK(current.current_report!=nullptr);current.weapons.set_unlimited_ammo(true);
            }
            return true;
        };
        MissionTickInputs input;DispatchResult result;result.calls.push_back({DispatchCallKind::weapon_fire,0x483c70,{},false});input.dispatch.push_back(result);
        const auto report=mission_tick(state,input);CHECK(shots==1 && state.current_report==nullptr);
        unsigned observations=0;for(const auto& entry:report.trace)if(entry.kind==MissionTraceKind::weapon_event && entry.address==std::uint32_t(WeaponEventKind::shot))++observations;
        CHECK(observations==1);
    }
    {
        MissionTickState state;seed(state,true);std::size_t refire=0;
        for(std::size_t i=0;i<kWeaponCount;++i)if(weapon_definitions()[i].use_class==1 && (weapon_definitions()[i].flags&0x20)){refire=i;break;}
        CHECK(refire!=0);std::array<std::uint8_t,kWeaponSlotCount> slots{};slots.fill(std::uint8_t(refire));
        state.weapons.set_slot_map(slots);state.weapons.resupply(false);unsigned queries=0;
        state.world.trigger_held_47a290=[&](std::uint32_t command){CHECK(command==0x8c);++queries;return false;};
        MissionTickInputs input;DispatchResult result;result.calls.push_back({DispatchCallKind::weapon_fire,0x483c70,{},false});input.dispatch.push_back(result);
        mission_tick(state,input);for(unsigned tick=0;tick<200 && !queries;++tick)mission_tick(state,{});
        CHECK(queries==1 && !state.weapons.trigger_query);
        state.world.trigger_held_47a290={};bool failed=false;
        try{mission_tick(state,input);for(unsigned tick=0;tick<200;++tick)mission_tick(state,{});}
        catch(const MissionTickError&){failed=true;}
        CHECK(failed && !state.weapons.trigger_query && state.current_report==nullptr);
    }
    std::cout<<"mission tick join checks passed\n";
}
