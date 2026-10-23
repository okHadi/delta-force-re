#include "dfre/mission_object_live.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {
using U=std::uint32_t;
void put(AiActorImage& image,std::size_t at,U n){std::memcpy(image.data()+at,&n,4);}
std::string hex(const AiActorImage& image){const char* digits="0123456789abcdef";std::string s;for(auto n:image){s+=digits[n>>4];s+=digits[n&15];}return s;}
}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);U v[16];for(auto& n:v){std::uint64_t x;if(!(input>>x))throw MissionTickError("Short veh0 fixture");n=U(x);}
    MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];actor.active=true;actor.definition_index=1;actor.person_slot=0;
    MissionTickConfig config;config.host=v[0]!=0;
    init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(2),0x12345678);
    auto image=snapshot_mission_actor(state,{0,0});put(image,0x1c,v[4]);image[0x6c]=std::uint8_t(v[1]);image[0x74]=std::uint8_t(v[2]);image[0x75]=std::uint8_t(v[2]>>8);
    put(image,0x7c,v[3]);put(image,0x2c,0x7fff0000);put(image,0xf8,0x123);apply_mission_actor_image(state,{0,0},image);
    state.person_extra(0).field_a4=v[5];state.people[0].tail_flag=v[5]!=0;state.people[0].field_ac=v[6];state.people[0].field_b0=v[7];
    state.person_extra(1).field_a4=3;state.people[1].tail_flag=true;state.people[1].field_ac=2;state.people[1].field_b0=5;
    std::ostringstream trace;unsigned calls=0,markers=0;MissionObjectLiveBindings bindings;
    bindings.global=[&](auto&,U at){trace<<'G'<<at<<';';const U offset=(at-0x78f280u)%0x88u;if(offset==4)return v[8];if(offset==0)return v[9];throw MissionTickError("Unexpected global read");};
    bindings.type_slot=[&](auto&,U slot,U at){trace<<'T'<<slot<<','<<at<<';';if(at!=0x3c)throw MissionTickError("Unexpected type read");return v[15];};
    bindings.call=[&](auto& current,const auto&,const ObjectBehaviorAction& action)->MissionObjectCallResult{
        trace<<'C'<<action.site;for(auto n:action.args)trace<<','<<n;trace<<',';const char* digits="0123456789abcdef";for(auto n:action.payload)trace<<digits[n>>4]<<digits[n&15];trace<<';';
        if(calls++==0){if(v[10]&1)current.tables.groups[0][0].person_slot=1;if(v[10]&2)current.person_extra(0).field_a4=0;
            if(v[10]&8)current.behavior.rng=0xcafebabe;
            if(v[10]&16){current.tables.groups[0][0].field_7c=1;auto changed=snapshot_mission_actor(current,{0,0});put(changed,0x1c,0);apply_mission_actor_image(current,{0,0},changed);}
        }
        if(action.site==0x42ee30 && (v[10]&4)){current.people[0].field_ac=2;current.people[0].field_b0=0xffffffff;}
        if(v[10]&32)throw MissionTickError("Fixture foreign failure");
        MissionObjectCallResult result;if(action.site==0x45c6a0){result.values={v[11],v[14],v[12],v[13],0,0};if(markers++!=0&&(v[10]&64)){result.values[0]=0;result.output_mask=0;}}return result;
    };
    bool failed=false;try{mission_object_veh0(state,{0,0},bindings);}catch(const MissionTickError&){failed=true;}
    std::cout<<failed<<' '<<state.behavior.rng<<' '<<hex(snapshot_mission_actor(state,{0,0}));
    for(std::size_t slot=0;slot<2;++slot)std::cout<<' '<<state.person_extra(slot).field_a4<<' '<<state.people[slot].field_ac<<' '<<state.people[slot].field_b0;
    std::cout<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
