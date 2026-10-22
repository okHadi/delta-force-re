#include "dfre/live_world.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("live actors: " #x);}while(false)
namespace {
std::uint32_t word(const dfre::AiActorImage& image,std::size_t at){std::uint32_t x;std::memcpy(&x,image.data()+at,4);return x;}
}
int main(){try{
    using namespace dfre;
    CHECK(mission_actor_source_address(0,0)==0x7dde80 && mission_actor_source_address(1,0)==0x867e80);
    CHECK(mission_actor_source_address(2,1)==0x8acf94 && mission_actor_source_address(3,1)==0x7ccb94);
    MissionRuntimeState state;state.people.resize(2);state.groups[3].resize(1);state.health[3]={37};
    auto& actor=state.groups[3][0];actor.active=true;actor.definition_index=1;actor.object_id=10;actor.person_slot=0;actor.position[0]=0xffffffffu;
    state.group3_count=3;state.actors.emplace(ActorDefinitionTable{},ActorFactoryConfig{});
    auto& created=state.actors->actor(2);created.definition_index=2;created.object_id=20;created.person=person_handle(1);created.health=99;created.argument=0xfeed1234;
    auto seed=seed_mission_actors(state);
    CHECK(seed.tables.groups[3].size()==3);CHECK(seed.tables.groups[3][0].person_slot==0);CHECK(seed.tables.groups[3][2].person_slot==1);
    CHECK(seed.extras[3][0].field_74==37);CHECK(seed.extras[3][2].field_74==99);
    for(std::size_t g=0;g<4;++g)for(std::size_t i=0;i<seed.images[g].size();++i)
        CHECK(merge_live_actor_image(seed.images[g][i],seed.tables.groups[g][i],seed.extras[g][i],seed.person_encoding[g][i])==seed.images[g][i]);
    auto stored=seed.images[3][2];for(std::size_t i=0x98;i<0xf8;++i)stored[i]=std::uint8_t(i^0xa5);
    auto& moving=seed.tables.groups[3][2];moving.position[0]=12345;moving.angle_pitch=0xc0000000;moving.field_fc=78;moving.person_slot=0;
    seed.extras[3][2].field_74=12;seed.extras[3][2].field_94=7;
    const auto current=merge_live_actor_image(stored,moving,seed.extras[3][2],seed.person_encoding[3][2]);
    CHECK(word(current,0x24)==12345);CHECK(word(current,0x34)==0xc0000000);CHECK(word(current,0xc)==person_handle(0));CHECK(word(current,0xfc)==78);
    for(std::size_t i=0x98;i<0xf8;++i)CHECK(current[i]==stored[i]);
    CHECK(seed.images[3][2]!=current);
    created.person=kPersonHandleBase+1;bool invalid=false;try{seed_mission_actors(state);}catch(const std::invalid_argument&){invalid=true;}CHECK(invalid);
    std::cout<<"live actor checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
