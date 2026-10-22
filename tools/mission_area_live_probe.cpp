#include "dfre/mission_area_live.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

// Rows: stage(0 listener,1 queue,2 unowned),type,health,mutation,ray answer,RNG,alternate.
// Mutation: 1 health,2 position,4 update error,8 query RNG,16 local-hit error,
// 32 append burn once,64 listener error.
int main() {
    using namespace dfre;
    try {
        std::string line;while(std::getline(std::cin,line)) {
            if(line.empty())continue;std::istringstream in(line);
            unsigned stage,type,health,mutation,random,alternate;std::int32_t answer;
            if(!(in>>stage>>type>>health>>mutation>>answer>>random>>alternate))throw std::invalid_argument("Short live area row");
            MissionTickState state;AiActorTables tables;tables.groups[3].resize(1);
            auto& actor=tables.groups[3][0];actor.active=true;actor.definition_index=1;actor.field_110=0x1234;
            std::vector<AiItemDefinition> definitions(2);definitions[1].field_28=3;definitions[1].field_84=0x1234;
            init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},random);
            state.config.local_actor=0;state.globals.mission_67f528=1;state.extra_for({3,0}).field_74=std::uint16_t(health);
            const MissionGeometry geometry(std::vector<MissionGeometryModel>{});MissionAreaLiveBindings b;
            b.globals=[&](const auto&){MissionAreaLiveGlobals values;values.alternate_listener_6ba3ec=alternate;values.alternate_pose.position.x=777;return values;};
            std::ostringstream trace;bool appended=false;
            b.segment=[&](MissionTickState& live,const auto&,const auto&,const auto&){trace<<'q'<<',';if(mutation&8)live.behavior.rng=1;return answer;};
            b.update=[&](MissionTickState& live,const auto& ref,auto callback,auto message){
                trace<<'u'<<message<<':'<<live.extra_for(ref).field_74<<',';
                if(callback!=0x1234)throw std::runtime_error("Bad live callback");
                if(mutation&1)live.extra_for(ref).field_74=70;
                if(mutation&2)live.tables.groups[3][0].position[0]=333;
                if((mutation&32)&&!appended){appended=true;live.pending_spawns.push_back({{0,0,0},4,0});}
                if(mutation&4)throw std::runtime_error("update stopped");
            };
            b.local_hit=[&](MissionTickState& live){trace<<'l'<<':'<<live.extra_for({3,0}).field_74<<',';if(mutation&16)throw std::runtime_error("local hit stopped");};
            b.listener=[&](MissionTickState& live,const auto& pose){trace<<'v'<<pose.position.x<<',';live.tables.groups[3][0].position[0]=99;if(mutation&64)throw std::runtime_error("listener stopped");};
            MissionStageCall call;call.address=stage==0?0x45e050:stage==1?0x465bd0:0x1234;
            if(stage==1)state.pending_spawns.push_back({{0,0,0},std::int32_t(type),0});
            bool handled=false,failed=false;
            try {handled=mission_area_live_stage(state,geometry,call,b).has_value();}catch(const std::runtime_error&){failed=true;}
            std::cout<<handled<<' '<<failed<<' '<<state.extra_for({3,0}).field_74<<' '<<state.tables.groups[3][0].position[0]<<' '
                <<unsigned(state.extra_for({3,0}).field_6d)<<' '<<unsigned(state.runtime_for({3,0}).byte_6e)<<' '<<state.behavior.rng<<' '
                <<state.pending_spawns.size()<<' '<<(trace.str().empty()?"-":trace.str())<<'\n';
        }
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
