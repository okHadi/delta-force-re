#include "dfre/mission_object_live.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {using U=std::uint32_t;void put(AiActorImage& image,std::size_t at,U n){std::memcpy(image.data()+at,&n,4);}std::string hex(const AiActorImage& image){const char* d="0123456789abcdef";std::string s;for(auto n:image){s+=d[n>>4];s+=d[n&15];}return s;}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);U v[21];for(auto& n:v){std::uint64_t x;if(!(input>>x))throw MissionTickError("Short searchlight AI fixture");n=U(x);}
    MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);tables.groups[3].resize(1);
    auto& self=tables.groups[0][0];self.active=true;self.definition_index=1;self.person_slot=0;self.field_72=1;
    auto& other=tables.groups[3][0];other.active=true;other.definition_index=1;other.field_72=2;other.field_6c=std::uint8_t(v[13]);for(unsigned axis=0;axis<3;++axis)other.position[axis]=v[14+axis];
    MissionTickConfig config;config.host=v[0]!=0;init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(2),v[20]);
    auto image=snapshot_mission_actor(state,{0,0});image[0x6c]=std::uint8_t(v[1]);image[0x6d]=0x80;image[0x74]=std::uint8_t(v[2]);image[0x75]=std::uint8_t(v[2]>>8);put(image,0xf8,0x123);put(image,0x2c,0x7fff0000);
    const unsigned offsets[]={0xe0,0xe4,0xe8,0x30,0x34};for(unsigned i=0;i<5;++i)put(image,offsets[i],v[3+i]);apply_mission_actor_image(state,{0,0},image);
    state.people[0].flags24=std::uint8_t(v[10]);state.person_extra(0).field_38=v[9];state.person_extra(0).field_44=v[8]?mission_actor_source_address(3,0):0;
    state.people[1].flags24=0xa0;auto& p1=state.person_extra(1);p1.field_38=123456;p1.field_44=0x123;p1.field_48=11;p1.field_4c=22;p1.field_50=33;
    std::ostringstream trace;unsigned calls=0,angles=0;MissionObjectLiveBindings b;
    b.call=[&](auto& current,const auto&,const ObjectBehaviorAction& action){trace<<'C'<<action.site;for(auto n:action.args)trace<<','<<n;trace<<',';const char* d="0123456789abcdef";for(auto n:action.payload)trace<<d[n>>4]<<d[n&15];trace<<';';
        const auto call=calls++;auto changed=snapshot_mission_actor(current,{0,0});
        if(call==0){if(v[17]&1)current.tables.groups[0][0].person_slot=1;if(v[17]&2){auto& p=current.person_extra(0);p.field_38=99;p.field_44=0;current.people[0].flags24|=0x40;}
            if(v[17]&4){current.tables.groups[3][0].position[0]^=0xffffffff;current.tables.groups[3][0].field_72=3;}
            if(v[17]&8)current.behavior.rng=0xcafebabe;
            if(v[17]&16){put(changed,0xe0,0x55555555);put(changed,0xe4,0x66666666);}
            if(v[17]&128)current.tables.groups[0][0].person_slot.reset();
        }
        // Preserve the callback's current person link while changing raw fields.
        const auto current_image=snapshot_mission_actor(current,{0,0});std::memcpy(changed.data()+0xc,current_image.data()+0xc,4);
        if(call==1&&(v[17]&32)){put(changed,0xe0,0x11111111);put(changed,0xe4,0x22222222);}
        apply_mission_actor_image(current,{0,0},changed);
        if(v[17]&64)throw MissionTickError("Searchlight AI foreign failure");
        std::array<U,6> result{};if(action.site==0x44f540){result[0]=v[11]?mission_actor_source_address(3,0):0;result[1]=v[12];}
        else if(action.site==0x477770)result[0]=v[18+(angles++?1:0)];
        else if(action.site!=0x446fb0&&action.site!=0x42edd0)throw MissionTickError("Unknown searchlight AI call");return result;
    };
    bool failed=false;try{mission_object_slgt_ai(state,{0,0},b);}catch(const std::exception&){failed=true;}
    const auto link=state.tables.groups[0][0].person_slot;std::cout<<failed<<' '<<state.behavior.rng<<' '<<(link?std::int64_t(*link):-1)<<' '<<hex(snapshot_mission_actor(state,{0,0}));
    for(unsigned slot=0;slot<2;++slot){const auto& p=state.person_extra(slot);std::cout<<' '<<U(state.people[slot].flags24)<<' '<<p.field_38<<' '<<p.field_44<<' '<<p.field_48<<' '<<p.field_4c<<' '<<p.field_50;}
    std::cout<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
