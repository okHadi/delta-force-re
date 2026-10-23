#include "dfre/mission_event_actor_orders.hpp"
#include <iostream>
#include <sstream>
using namespace dfre;
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);std::uint32_t v[21];for(auto& n:v){std::uint64_t x;if(!(input>>x))throw MissionTickError("Short actor-order fixture");n=std::uint32_t(x);}
    AiActorTables tables;tables.groups[3].resize(3);tables.groups[0].resize(2);tables.groups[2].resize(3);
    for(unsigned i=0;i<5;++i){auto& a=i<3?tables.groups[3][i]:tables.groups[0][i-3];a.active=true;a.definition_index=1;a.field_72=std::uint16_t((v[7]&(1u<<i))?v[1]:v[1]+1);a.field_70=1;if(v[6]&(1u<<i))a.person_slot=i;for(unsigned axis=0;axis<3;++axis)a.position[axis]=v[8+axis]+i*0x10000;}
    for(unsigned i=0;i<3;++i)for(unsigned axis=0;axis<3;++axis)tables.groups[2][i].position[axis]=v[11+i*3+axis];
    MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(6),123);
    for(unsigned i=0;i<6;++i){state.person_extra(i).field_a4=10+i;state.people[i].tail_flag=true;state.people[i].field_ac=20+i;state.people[i].field_b0=30+i;}
    state.world.behavior_world.lists.resize(2);auto& list=state.world.behavior_world.lists[0];list.count=v[5];list.indices[0]=2;list.indices[1]=0;list.indices[2]=1;
    auto underlying=mission_event_actor_order_bindings(state);std::uint32_t c3=v[3],c0=v[4];unsigned pointers=0,counts=0,listcounts=0,xreads=0,yreads=0;std::ostringstream trace;MissionEventActorOrderBindings b;
    b.read=[&](auto at,auto width){trace<<'R'<<at<<','<<width<<';';
        if(at==0x7cca8c){const auto occurrence=pointers++;if(!v[0]){auto& p=state.tables.groups[3][0].person_slot;if(occurrence==0&&(v[20]&1))p=5;if(occurrence==1&&(v[20]&2))p=4;if(occurrence==2&&(v[20]&4))p.reset();if(occurrence==3&&(v[20]&8))p=3;if(occurrence==3&&(v[20]&1024))throw MissionTickError("Actor-order pointer read failure");}}
        if(at==0x8cfa18){if(counts++==1&&(v[20]&128))c3=1;return c3;}
        if(at==0x8cfa1c)return c0;
        if(at==0x78f284&&listcounts++==0&&(v[20]&32))state.world.behavior_world.lists[0].count=0;
        if(at==0x7ccaa4&&xreads++==0&&(v[20]&64))state.tables.groups[2][0].position[0]^=0xffffffff;
        if(at==0x8ace80+2*0x114+0x28&&yreads++==0&&(v[20]&256))state.tables.groups[3][0].position[1]^=0xffffffff;
        return underlying.read(at,width);};
    b.write=[&](auto at,auto width,auto value){trace<<'W'<<at<<','<<width<<','<<value<<';';underlying.write(at,width,value);if(at==0x794680+0xa4&&(v[20]&16))state.tables.groups[3][0].person_slot=4;if(at==0x794680+0xac&&(v[20]&512))state.tables.groups[3][0].person_slot.reset();};
    bool failed=false;std::uint32_t result=0;try{if(v[0])result=mission_event_closest_marker(b,0x7cca80,v[2]);else mission_event_actor_orders(b,std::int32_t(v[1]),v[2]);}catch(const std::exception&){failed=true;}
    std::cout<<failed<<' '<<result<<' '<<c3<<' '<<c0<<' '<<state.world.behavior_world.lists[0].count;
    for(unsigned i=0;i<5;++i){const auto& a=i<3?state.tables.groups[3][i]:state.tables.groups[0][i-3];std::cout<<' '<<(a.person_slot?std::int64_t(*a.person_slot):-1);for(auto n:a.position)std::cout<<' '<<n;}
    for(const auto& a:state.tables.groups[2])for(auto n:a.position)std::cout<<' '<<n;
    for(unsigned i=0;i<6;++i)std::cout<<' '<<state.person_extra(i).field_a4<<' '<<state.people[i].field_ac<<' '<<state.people[i].field_b0;
    std::cout<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
