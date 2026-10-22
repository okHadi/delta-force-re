#include "dfre/mission_actor_lifecycle.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("actor lifecycle: " #x);}while(false)
using namespace dfre;
int main(){try{
    MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);tables.groups[3].resize(1);
    for(auto group:{0,3}){auto& actor=tables.groups[group][0];actor.active=true;actor.definition_index=1;actor.position[0]=10;actor.position[1]=20;}
    tables.groups[3][0].field_18=kCharacterInstanceBase;tables.groups[3][0].person_slot=0;
    std::vector<AiPersonState> people(2);people[0].owner=1;people[0].name="first";people[1].owner=2;people[1].name="second";
    std::vector<AiItemDefinition> definitions(2);definitions[1].field_28=3;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),std::move(people),0);
    state.runtime_for({3,0}).step_cache={111,222,333};
    state.runtime_for({3,0}).link_90=actor_handle(0,0);state.person_extras[1].field_6c=55;
    CharacterModel character;character.runtime_prepared=true;character.bone_map={0};
    MissionAnimations animations({{kCharacterInstanceBase,&character,{{0,0,0,false,0,0,0,0}}},{kCharacterInstanceBase+1,&character,{}}});
    bind_mission_animations(state,animations);
    std::array<MissionAudioRegistration,1024> registrations{};
    registrations[0]={1,actor_handle(3,0),3,4};registrations[2]={5,actor_handle(0,0),7,8};registrations[1023]={9,actor_handle(3,0),11,12};
    unsigned calls=0;MissionActorLifecycleBindings b;
    b.sincos_4776c0=[&](auto,auto& sine,auto& cosine){
        CHECK(state.runtime_for({3,0}).link_90==0);CHECK((state.extra_for({0,0}).field_6d&1)!=0);
        sine=-96;cosine=160;state.tables.groups[3][0].field_18=kCharacterInstanceBase+1;
        state.tables.groups[3][0].person_slot=1;state.tables.groups[3][0].position[0]=0xfffffff0;++calls;
    };
    b.terrain_47df40=[&](auto x,auto y){CHECK(x==0xfffffff5&&y==17);CHECK(animations.size()==2);CHECK(state.people[1].owner==2);++calls;return 123u;};
    MissionActorLifecycle lifecycle(state,animations,registrations,b);lifecycle.remove(std::nullopt);CHECK(calls==0);
    lifecycle.remove(AiActorRef{3,0});CHECK(calls==2);CHECK(snapshot_mission_actor(state,{3,0})==AiActorImage{});
    CHECK(!state.runtime_for({3,0}).driver&&state.runtime_for({3,0}).animation==nullptr);
    CHECK((state.runtime_for({3,0}).step_cache==CachedStep{111,222,333}));
    CHECK(state.people[0].owner==1&&state.people[1].owner==0&&state.people[1].name.empty()&&state.person_extras[1].field_6c==0);
    CHECK(animations.pose(kCharacterInstanceBase)!=nullptr&&animations.pose(kCharacterInstanceBase+1)==nullptr);
    CHECK(state.tables.groups[0][0].position[2]==123);
    CHECK(registrations[0].definition==0&&registrations[1023].definition==0&&registrations[2].definition==5);
    lifecycle.release_character(0);bool failed=false;try{lifecycle.release_character(kCharacterInstanceBase+1);}catch(const std::out_of_range&){failed=true;}CHECK(failed);
    // Missing terrain keeps the earlier drop writes and current person/CHR intact.
    auto& actor=state.tables.groups[3][0];actor.active=true;actor.definition_index=1;actor.field_6c=4;actor.field_18=kCharacterInstanceBase;actor.person_slot=0;
    state.runtime_for({3,0}).link_90=actor_handle(0,0);MissionActorLifecycle missing(state,animations,registrations,{});
    failed=false;try{missing.remove(AiActorRef{3,0});}catch(const MissionTickError&){failed=true;}
    CHECK(failed&&actor.active&&actor.field_18==kCharacterInstanceBase&&state.runtime_for({3,0}).link_90==0&&state.people[0].owner==1);
    CHECK(animations.pose(kCharacterInstanceBase)!=nullptr);
    MissionTickState empty;AiActorTables vacant;vacant.groups[0].resize(1);
    init_mission_tick(empty,{},std::move(vacant),AiItemDefinitionTable{}, {},0);
    MissionActorLifecycle empty_lifecycle(empty,animations,registrations,{});
    empty_lifecycle.remove(AiActorRef{0,0});CHECK(snapshot_mission_actor(empty,{0,0})==AiActorImage{});
    std::cout<<"actor lifecycle checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
