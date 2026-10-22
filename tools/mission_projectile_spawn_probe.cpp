#include "dfre/mission_projectile_spawn.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
using namespace dfre;
namespace {
std::uint32_t next(std::istream& s){std::uint64_t n;if(!(s>>n))throw std::runtime_error("Short spawn row");return std::uint32_t(n);}
std::int32_t si(std::uint32_t n){return n<=0x7fffffff?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::string hex(const std::vector<std::uint8_t>& v){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto n:v)s<<std::setw(2)<<unsigned(n);return s.str();}
void put(MissionSpawnPacket& p,unsigned at,std::uint32_t n,unsigned width){for(unsigned i=0;i<width;++i)p[at+i]=std::uint8_t(n>>(8*i));}
}
int main(int argc,char** argv){if(argc!=2)return 2;try{std::string line;while(std::getline(std::cin,line)){
    if(line.empty())continue;std::istringstream in(line);MuzzleResult m; m.origin={si(next(in)),si(next(in)),si(next(in))};m.aim={next(in),next(in),next(in)};
    auto slot=next(in),kind=next(in),flags=next(in),speed=next(in),f10=next(in),f14=next(in),f18=next(in),sound=next(in),host=next(in),rulea=next(in),ruleb=next(in),team=next(in),ring=next(in),mutation=next(in);auto height=si(next(in)),level=si(next(in));const auto active=next(in);
    MissionTickState live;AiActorTables tables;tables.groups[3].resize(32);for(auto& a:tables.groups[3]){a.active=active!=0;a.definition_index=1;a.field_70=std::uint16_t(team);}init_mission_tick(live,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0);live.globals.mission_67f528=host;live.projectiles.resize(512);
    std::vector<RuntimeItemDefinition> defs(4);for(unsigned i=1;i<4;++i)defs[i].model=0x7000+i;defs[1].id=0xfa1;defs[2].id=0xfc1;defs[3].id=0xfca;defs[3].ambient_handles[0]=11;defs[3].ambient_sound[0]=12;
    MissionProjectileSpawnCells cells;cells.ring_index=ring;cells.rules_8cf982=std::uint8_t(rulea);cells.rules_67dbc9=std::uint8_t(ruleb);auto& cls=cells.classes[kind];cls.field_30=flags;cls.field_0c=speed;cls.field_10=f10;cls.field_14=f14;cls.field_18=f18;cls.field_20=sound;
    std::ostringstream trace;MissionSpawnPacket input{};put(input,0,std::uint32_t(m.origin.x),4);put(input,4,std::uint32_t(m.origin.y),4);put(input,8,slot,2);put(input,10,m.aim.yaw>>16,2);put(input,12,m.aim.pitch>>16,2);input[14]=std::uint8_t(kind);
    const bool receive=std::string(argv[1])=="receive";
    auto mutate_delivery=[&]{if(mutation&2){m.origin.x=si(std::uint32_t(m.origin.x)+777);m.aim.pitch+=0x40000;live.tables.groups[3][slot].field_70=2;cls.field_30^=0x50;defs[1].model+=100;defs[2].model+=100;defs[3].model+=100;}};
    MissionProjectileSpawnBindings b;
    b.ground=[&](auto&,auto x,auto y){trace<<"G,"<<std::uint32_t(x)<<','<<std::uint32_t(y)<<';';if(mutation&1){if(receive){input[8]=std::uint8_t(slot^1);put(input,10,((m.aim.yaw>>16)+1)&0xffff,2);}else{m.origin.x=si(std::uint32_t(m.origin.x)^0x7f);m.origin.z=si(std::uint32_t(m.origin.z)+0x123);m.aim.yaw+=0x10000;}}return height;};
    b.host_message=[&](auto&,auto& bytes){trace<<"H,"<<hex({bytes.begin(),bytes.end()})<<';';mutate_delivery();};
    b.queue=[&](auto&,auto id,const auto& bytes){trace<<"Q,"<<id<<','<<hex(bytes)<<';';mutate_delivery();};
    b.attached_sound=[&](auto&,auto index,auto s,auto v,auto f){trace<<"A,"<<index<<','<<s<<','<<v<<','<<f<<';';if(mutation&4){++cls.field_20;cls.field_30^=6;}};
    b.positional_sound=[&](auto&,auto s,auto p){trace<<"S,"<<s<<','<<std::uint32_t(p.x)<<','<<std::uint32_t(p.y)<<','<<std::uint32_t(p.z)<<';';if(mutation&8){++cls.field_20;cls.field_30^=0x86;}};
    b.loudness=[&](auto&,auto s){trace<<"L,"<<s<<';';if(mutation&16){cls.field_30^=0x84;live.tables.groups[3][slot].field_70=0xffff;}if(mutation&32)cells.ring_index=127;return level;};
    b.alert=[&](auto&,auto p,auto l,auto t,auto mode){trace<<"E,"<<std::uint32_t(p.x)<<','<<std::uint32_t(p.y)<<','<<std::uint32_t(p.z)<<','<<std::uint32_t(l)<<','<<std::uint32_t(t)<<','<<mode<<';';};
    MissionProjectileSpawn spawn(live,cells,defs,b);if(receive)spawn.receive(input);else if(std::string(argv[1])=="request")spawn.request(actor_handle(3,slot),kind,m);else spawn.spawn(actor_handle(3,slot),kind,m);
    const auto& e=live.projectiles[(ring+1)&511];std::cout<<cells.ring_index;
    for(auto n:{std::uint32_t(e.item_main),std::uint32_t(e.item_alt),std::uint32_t(e.item_field),std::uint32_t(e.position.x),std::uint32_t(e.position.y),std::uint32_t(e.position.z),e.aim_yaw,e.aim_pitch,std::uint32_t(e.velocity.x),std::uint32_t(e.velocity.y),std::uint32_t(e.velocity.z),e.class_flags,e.field_7c,e.field_80,e.field_84,e.color,std::uint32_t(e.owner_slot),std::uint32_t(e.ttl),std::uint32_t(live.extra_for({3,slot}).field_6d)})std::cout<<' '<<n;
    std::cout<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
