#include "dfre/mission_client_finalize.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_sound_live.hpp"
#include <iostream>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool value){if(!value)throw MissionClientAddedError("Client finalization check failed");}
}
int main(){try{
    MissionTickState state;AiActorTables tables;tables.groups[3].resize(2);
    for(std::size_t i=0;i<2;++i){auto& a=tables.groups[3][i];a.active=true;a.definition_index=1;a.field_6c=3;a.person_slot=i;}
    std::vector<AiPersonState> people(2);for(std::size_t i=0;i<2;++i)people[i].owner=mission_actor_source_address(3,i);
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable({AiItemDefinition{},AiItemDefinition{}}),std::move(people),0);
    MissionClientLive clients;auto records=mission_sound_live_bindings(state,[]{return 0u;});MissionClientAddedBindings b;
    b.read=[&](U at,U width)->U{
        if(at==0x67f528&&width==4)return state.globals.mission_67f528;
        if(clients.claims(at,width))return clients.read(at,width);
        for(std::size_t i=0;i<2;++i)if(at==mission_actor_source_address(3,i)+0x0c&&width==4)return 0x794680u+U(*state.tables.groups[3][i].person_slot)*0xb4;
        if(at>=0x794680&&std::uint64_t(at-0x794680)+width<=state.people.size()*0xb4){const auto delta=at-0x794680;check(delta%0xb4+width<=0xb4);const auto image=ai_person_image(state.people[delta/0xb4]);U n=0;for(U i=0;i<width;++i)n|=U(image[delta%0xb4+i])<<(i*8);return n;}
        const auto base=records.record_base(at,width);if(!base)throw MissionClientAddedError("Finalization read has no current owner");return records.record_read(*base,at-*base,width);
    };
    b.write=[&](U at,U width,U value){
        if(clients.claims(at,width)){clients.write(at,width,value);if(at==mission_client_source_base+8)clients.write(at,4,mission_actor_source_address(3,1));return;}
        if(at>=0x794680&&std::uint64_t(at-0x794680)+width<=state.people.size()*0xb4){const auto delta=at-0x794680,offset=delta%0xb4;check(width==1&&offset>=4&&offset<0x24);auto& name=state.people[delta/0xb4].name;const auto index=std::size_t(offset-4);if(!value)name.resize(index);else{if(name.size()<=index)name.resize(index+1,0);name[index]=char(value);}return;}
        const auto base=records.record_base(at,width);if(!base)throw MissionClientAddedError("Finalization write has no current owner");records.record_store(*base,at-*base,width,value);
    };
    clients.write(mission_client_source_base,4,77);clients.write(mission_client_source_base+0x18c,1,1);clients.write(mission_client_source_base+0x9c,2,0x1234);clients.write(mission_client_source_base+0xa0,4,7);clients.write(mission_client_source_base+0xa4,4,2);
    std::vector<U> calls;b.call=[&](U site,const std::vector<U>& args)->U{
        calls.push_back(site);
        if(site==0x45d520){check(args==std::vector<U>({7,2}));return mission_actor_source_address(3,0);}
        if(site==0x43f610){check(args==std::vector<U>{mission_client_source_base});clients.write(mission_client_source_base+8,4,mission_actor_source_address(3,0));state.tables.groups[3][0].person_slot=1;const char* s="after";for(U i=0;i<=5;++i)clients.write(mission_client_source_base+0x0c+i,1,std::uint8_t(s[i]));return 0;}
        throw MissionClientAddedError("Finalization fixture body is not bound");
    };
    mission_client_finalize(b);check(calls==std::vector<U>({0x45d520,0x43f610}));check(state.tables.groups[3][0].object_id==77&&state.tables.groups[3][1].field_70==0x1234);check(state.people[0].name.empty()&&state.people[1].name=="after");check(clients.read(mission_client_source_base+8,4)==mission_actor_source_address(3,0));
    state.globals.mission_67f528=0;b.call={};mission_client_finalize(b);
    state.globals.mission_67f528=1;bool failed=false;try{mission_client_finalize(b);}catch(const MissionClientAddedError&){failed=true;}check(failed);
    b.call=[](U site,const std::vector<U>&)->U{if(site==0x45d520)return 0;throw MissionClientAddedError("Unexpected body after null factory result");};failed=false;try{mission_client_finalize(b);}catch(const MissionClientAddedError&){failed=true;}check(failed&&clients.read(mission_client_source_base+8,4)==mission_actor_source_address(3,1));
    std::cout<<"Current client/canonical actor/person finalization passed; nested services are explicit fixtures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
