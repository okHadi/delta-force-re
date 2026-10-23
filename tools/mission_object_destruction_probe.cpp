#include "dfre/mission_object_destruction.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {std::string hex(const std::uint8_t* p,std::size_t size){const char* d="0123456789abcdef";std::string s;for(std::size_t i=0;i<size;++i){s+=d[p[i]>>4];s+=d[p[i]&15];}return s;}}
int main(){try{unsigned body,id,incoming,seed,mut;while(std::cin>>body>>id>>incoming>>seed>>mut){
 if(body==3){GameMessageStage messages;MissionServerMessageBuffer buffer;std::vector<std::array<std::uint8_t,mission_client_record_size>> clients(17);clients[0][0x18c]=1;clients[0][0x1c4]=5;MissionClientOutboxBindings ob;ob.client=[&](std::size_t i){return MissionClientRecordView{clients.at(i).data(),clients.at(i).size()};};ob.cell=[](unsigned){return 0u;};MissionClientOutbox outbox(messages,buffer,ob);mission_object_destroy_broadcast(outbox,incoming,id);std::cout<<"B"<<hex(clients[0].data()+0x20e,4)<<'\n';continue;}
 AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;tables.groups[0][0].position[0]=0x123400;tables.groups[0][0].position[1]=0x234500;tables.groups[0][0].position[2]=0x345600;MissionTickState s;init_mission_tick(s,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},seed);s.tables.groups[0][0].field_72=(mut&16)?0xffff:0;
 auto initial=snapshot_mission_actor(s,{0,0});std::memcpy(initial.data()+0x30,&incoming,4);std::memcpy(initial.data()+0x34,&seed,4);std::memcpy(initial.data()+0x38,&id,4);apply_mission_actor_image(s,{0,0},initial);
 std::ostringstream trace;unsigned lookups=0,spawns=0,calls=0;MissionObjectDestructionBindings b;
 b.before_actor_read=[&](unsigned at){if((mut&256)&&at==0x7dde80+0x24)s.tables.groups[0][0].position[1]+=0x100;};
 b.definition=[&](auto index,auto off){trace<<'D'<<index<<','<<off<<';';return id;};
 b.model=[&](auto index,auto off){trace<<'M'<<index<<','<<off<<';';return 0x4000u;};
 b.lookup_4828c0=[&](auto n){trace<<'L'<<n<<';';if(mut&1)s.tables.groups[0][0].position[0]+=0x100;return ++lookups;};
 b.item_table_base=[&](){return 0x1211000u+((mut&2)&&lookups==2?0x1000u:0u);};
 b.item_word=[&](auto ptr,auto off){trace<<'I'<<ptr<<','<<off<<';';return ptr^off;};
 b.call=[&](auto site,const auto& args,std::uint8_t* data,std::size_t size){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<':'<<hex(data,size)<<';';
  if((mut&4)&&calls++==0)s.tables.groups[0][0].position[1]+=0x200;
  if(site==0x45d970){if(mut&8)data[0xe0]=std::uint8_t(++spawns);if(mut&32)s.behavior.rng^=0x99999999;}
  if(site==0x460a30&&(mut&8)){data[0]^=0x80;data[10]^=0x40;}
  if(site==0x43e7d0&&(mut&64))s.tables.groups[0][0].position[2]+=0x300;
  if((mut&128)&&site==0x45d970)throw MissionTickError("Fixture failure");
 };
 bool failed=false;try{if(body==0)mission_object_destroy_generic_raw(s,{0,0},b,incoming);else mission_object_destroy_wood(s,{0,0},b,body==2);}catch(const std::exception&){failed=true;}
 std::cout<<failed<<' '<<s.behavior.rng<<' '<<s.tables.groups[0][0].position[0]<<' '<<s.tables.groups[0][0].position[1]<<' '<<s.tables.groups[0][0].position[2]<<' '<<trace.str()<<'\n';
 }}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
