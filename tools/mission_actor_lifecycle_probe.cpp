#include "dfre/mission_actor_lifecycle.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {
void put(AiActorImage& image,unsigned at,std::uint32_t value){std::memcpy(image.data()+at,&value,4);}
std::string hex(const std::uint8_t* bytes,std::size_t length){const char* digits="0123456789abcdef";std::string result;for(std::size_t i=0;i<length;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;}
}
int main(){std::string line;while(std::getline(std::cin,line)){try{
    std::istringstream input(line);std::string op;unsigned type,flags,link,person,mutation;std::uint32_t seed;
    input>>op>>type>>flags>>link>>person>>mutation>>seed;
    MissionTickState state;AiActorTables tables;tables.groups[3].resize(1);tables.groups[0].resize(1);
    for(auto group:{0,3}){auto& actor=tables.groups[group][0];actor.active=true;actor.definition_index=1;actor.position[0]=seed;actor.position[1]=seed^0x98765432;actor.position[2]=seed+10;actor.field_6c=std::uint8_t(flags);}
    tables.groups[3][0].angle_yaw=seed;tables.groups[3][0].field_18=kCharacterInstanceBase;
    if(person)tables.groups[3][0].person_slot=0;
    std::vector<AiPersonState> people(2);for(auto& p:people){p.owner=11;p.name="name";p.field_70=12;}
    std::vector<AiItemDefinition> definitions(2);definitions[1].field_28=type;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),std::move(people),0);
    for(auto group:{0,3})state.person_encoding[group][0]=ActorPersonEncoding::handle;
    state.person_extras[0].field_6c=99;state.person_extras[1].field_6c=88;
    auto target=snapshot_mission_actor(state,{0,0});put(target,0x6c,0x1234ffff);apply_mission_actor_image(state,{0,0},target);
    state.runtime_for({3,0}).step_cache={111,222,333};
    state.runtime_for({3,0}).link_90=link?actor_handle(0,0):0;
    CharacterModel character;character.runtime_prepared=true;character.bone_map={0};
    MissionAnimations animations({{kCharacterInstanceBase,&character,{}},{kCharacterInstanceBase+1,&character,{}}});
    std::array<MissionAudioRegistration,1024> registrations{};registrations[0]={1,actor_handle(3,0),3,4};registrations[1]={5,actor_handle(0,0),7,8};registrations[1023]={9,actor_handle(3,0),11,12};
    std::vector<std::vector<std::uint32_t>> calls;
    MissionActorLifecycleBindings b;
    b.sincos_4776c0=[&](auto angle,auto& sine,auto& cosine){calls.push_back({0x4776c0,angle});sine=-96;cosine=160;
        if(mutation){state.tables.groups[3][0].position[0]=0xfffffff0;state.tables.groups[3][0].position[1]=32;state.tables.groups[3][0].field_18=kCharacterInstanceBase+1;state.tables.groups[3][0].person_slot=1;}};
    b.terrain_47df40=[&](auto x,auto y){calls.push_back({0x47df40,x,y});return x*7+y*13;};
    MissionActorLifecycle lifecycle(state,animations,registrations,b);
    if(op=="remove")lifecycle.remove(AiActorRef{3,0});else if(op=="drop")lifecycle.drop_link({3,0});else if(op=="null")lifecycle.remove(std::nullopt);else throw std::runtime_error("Unknown lifecycle probe operation");
    const auto self=snapshot_mission_actor(state,{3,0}),linked=snapshot_mission_actor(state,{0,0});
    std::cout<<"{\"self\":\""<<hex(self.data(),self.size())<<"\",\"linked\":\""<<hex(linked.data(),linked.size())<<"\",\"people\":[";
    for(unsigned i=0;i<2;++i){const BehaviorActor view{{3,0},0,state.tables.groups[3][0],state.people[i],state.extra_for({3,0}),state.person_extras[i]};const auto image=behavior_person_image(view);std::cout<<(i?",\"":"\"")<<hex(image.data(),image.size())<<'"';}
    const auto cache=state.runtime_for({3,0}).step_cache;
    std::cout<<"],\"cache\":["<<cache.x<<','<<cache.y<<','<<cache.z<<"],\"instances\":["<<(animations.pose(kCharacterInstanceBase)!=nullptr)<<','<<(animations.pose(kCharacterInstanceBase+1)!=nullptr)<<"],\"registrations\":[";
    for(auto i:{0,1,1023}){auto& r=registrations[i];std::cout<<(i?",[":"[")<<r.definition<<','<<r.actor<<','<<r.identity<<','<<r.reserved<<']';}
    std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){std::cout<<(i?",[":"[");for(std::size_t j=0;j<calls[i].size();++j)std::cout<<(j?",":"")<<calls[i][j];std::cout<<']';}std::cout<<"]}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
