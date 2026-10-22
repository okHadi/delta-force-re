#include "dfre/mission_projectile_spawn.hpp"
#include <stdexcept>
#include <string>
#include <iostream>
using namespace dfre;
namespace {
void check_impl(bool b,int line){if(!b)throw std::runtime_error("Projectile spawn test failed at "+std::to_string(line));}
#define check(value) check_impl((value),__LINE__)
std::vector<RuntimeItemDefinition> definitions(){std::vector<RuntimeItemDefinition> v(4);v[1].id=0xfa1;v[2].id=0xfc1;v[3].id=0xfca;for(unsigned i=1;i<4;++i)v[i].model=0x7000+i;return v;}
void init(MissionTickState& state){AiActorTables tables;tables.groups[3].resize(2);tables.groups[3][0].active=true;tables.groups[3][0].definition_index=1;tables.groups[3][0].field_70=1;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0);state.projectiles.resize(512);state.globals.mission_67f528=0;}
void request_and_receive(){
    MissionTickState state;init(state);auto items=definitions();MissionProjectileSpawnCells cells;cells.ring_index=511;auto& cls=cells.classes[3];cls.field_30=0x10;cls.field_18=100;cls.field_20=0;
    MuzzleResult muzzle{{0x12345,0x23456,0x34567},{0x12345678,0xabcdef12,0}};std::vector<unsigned> calls;MissionSpawnPacket saved{};
    MissionProjectileSpawnBindings b;
    b.ground=[&](auto&,auto x,auto y){check(x==0x12300&&y==0x23400&&muzzle.origin.z==0x34500);calls.push_back(1);muzzle.origin.x=0x1237f;muzzle.origin.z=0x34612;return 0x1200;};
    b.queue=[&](auto& live,auto id,const auto& bytes){check(id==9&&bytes.size()==15&&cells.ring_index==511);std::copy(bytes.begin(),bytes.end(),saved.begin());calls.push_back(2);muzzle.origin.x=777;live.tables.groups[3][0].field_70=2;cls.field_30=0x50;};
    b.loudness=[&](auto&,auto id){check(id==0&&cells.ring_index==0);calls.push_back(3);return 0;};
    MissionProjectileSpawn spawn(state,cells,items,b);spawn.request(actor_handle(3,0),3,muzzle);
    check(calls==std::vector<unsigned>({1,2,3})&&saved[0]==0x7f&&saved[8]==0&&state.projectiles[0].position.x==777&&state.projectiles[0].owner_slot==0);
    check(state.projectiles[0].item_main==2&&state.projectiles[0].item_alt==2&&state.projectiles[0].item_field==0x7002&&state.projectiles[0].color==0xb9ffa0a0);
    check(state.extra_for({3,0}).field_6d==0x40);
    state.globals.mission_67f528=1;calls.clear();b.ground=[](auto&,auto,auto){return 0;};
    b.host_message=[&](auto&,auto& p){calls.push_back(4);p[14]=27;muzzle.origin.x=888;};b.loudness=[&](auto&,auto){calls.push_back(3);return 0;};
    MissionProjectileSpawn host(state,cells,items,b);host.request(actor_handle(3,0),3,muzzle);
    check(calls==std::vector<unsigned>({4,3})&&state.projectiles[1].position.x==888&&state.projectiles[1].class_flags==0x50);
    state.tables.groups[3][0].active=false;calls.clear();saved[8]=0;saved[9]=0;saved[14]=3;
    b.ground=[&](auto&,auto,auto){calls.push_back(1);saved[8]=1;saved[10]=0x34;saved[11]=0x12;return 0;};
    b.queue=[&](auto&,auto id,const auto& bytes){check(id==0x17&&bytes==std::vector<std::uint8_t>({1,0}));calls.push_back(2);saved[14]=4;};
    cells.classes[4].field_30=0x80;cells.classes[4].field_18=7;cells.classes[4].field_20=0;
    MissionProjectileSpawn received(state,cells,items,b);received.receive(saved.data(),saved.size());
    check(calls==std::vector<unsigned>({1,2,3})&&state.projectiles[2].owner_slot==0&&state.projectiles[2].aim_yaw==0x12340000&&state.projectiles[2].ttl==7);
    const auto ring=cells.ring_index;
    bool rejected=false;try{received.receive(saved.data(),14);}catch(const MissionTickError&){rejected=true;}
    check(rejected&&cells.ring_index==ring);
    rejected=false;try{received.receive(nullptr,15);}catch(const MissionTickError&){rejected=true;}
    check(rejected&&cells.ring_index==ring);
}
void effects_and_errors(){
    MissionTickState state;init(state);auto items=definitions();items[3].ambient_handles[0]=11;items[3].ambient_sound[0]=12;MissionProjectileSpawnCells cells;
    auto& cls=cells.classes[1];cls.field_30=0x40;cls.field_18=100;cls.field_20=21;MuzzleResult muzzle{{1,2,3},{}};std::vector<unsigned> order;
    MissionProjectileSpawnBindings b;b.attached_sound=[&](auto&,auto slot,auto sound,auto value,auto flags){check(slot==1&&sound==11&&value==12&&flags==0);order.push_back(1);cls.field_20=22;cls.field_30=0x86;muzzle.origin.x=7;};
    b.positional_sound=[&](auto&,auto sound,auto p){check(sound==22&&p.x==7);order.push_back(2);cls.field_20=23;};
    b.loudness=[&](auto& live,auto id){check(id==23);order.push_back(3);live.tables.groups[3][0].field_70=0xffff;return -7;};
    b.alert=[&](auto&,auto p,auto level,auto team,auto mode){check(p.x==7&&level==-4&&team==-1&&mode==2);order.push_back(4);cls.field_30=2;cells.ring_index=17;};
    MissionProjectileSpawn spawn(state,cells,items,b);check(spawn.spawn(actor_handle(3,0),1,muzzle)==17);
    check(order==std::vector<unsigned>({1,2,3,4})&&state.projectiles[1].class_flags==0x40&&state.extra_for({3,0}).field_6d==0x48);
    cls.field_30=0x80;cls.field_20=0;b.loudness=[](auto& live,auto)->std::int32_t{live.extra_for({3,0}).field_74=91;throw std::runtime_error("effect failure");};
    MissionProjectileSpawn failed(state,cells,items,b);bool rejected=false;try{failed.spawn(actor_handle(3,0),1,muzzle);}catch(const std::runtime_error& e){rejected=std::string(e.what())=="effect failure";}
    check(rejected&&cells.ring_index==18&&state.projectiles[18].ttl==100&&state.extra_for({3,0}).field_74==91);
    b.loudness={};MissionProjectileSpawn missing(state,cells,items,b);rejected=false;try{missing.spawn(actor_handle(3,0),1,muzzle);}catch(const MissionTickError&){rejected=true;}check(rejected&&cells.ring_index==19&&state.projectiles[19].ttl==100);
    b.ground={};MissionProjectileSpawn no_ground(state,cells,items,b);muzzle.origin={0x12345,0x23456,0x34567};rejected=false;
    try{no_ground.request(actor_handle(3,0),1,muzzle);}catch(const MissionTickError&){rejected=true;}
    check(rejected&&muzzle.origin.x==0x12300&&muzzle.origin.y==0x23400&&muzzle.origin.z==0x34500&&cells.ring_index==19);
    b.ground=[](auto&,auto,auto){return 0;};b.queue=[](auto& live,auto,const auto&){live.extra_for({3,0}).field_74=92;throw std::runtime_error("queue failure");};
    MissionProjectileSpawn no_delivery(state,cells,items,b);rejected=false;
    try{no_delivery.request(actor_handle(3,0),1,muzzle);}catch(const std::runtime_error& e){rejected=std::string(e.what())=="queue failure";}
    check(rejected&&state.extra_for({3,0}).field_74==92&&cells.ring_index==19);
    state.projectiles.resize(1);rejected=false;try{MissionProjectileSpawn bad(state,cells,items,b);}catch(const MissionTickError&){rejected=true;}check(rejected);
}
}
int main(){try{request_and_receive();effects_and_errors();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
