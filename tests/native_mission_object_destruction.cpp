#include "dfre/mission_object_destruction.hpp"
#include "dfre/mission_object_generic.hpp"
#include "dfre/mission_object_wood_large.hpp"
#include <cassert>
using namespace dfre;
namespace {unsigned get(const std::uint8_t* p,unsigned width=4){unsigned n=0;for(unsigned i=0;i<width;++i)n|=unsigned(p[i])<<(8*i);return n;}}
int main(){MissionTickState s;AiActorTables tables;tables.groups[1].resize(1);tables.groups[1][0].active=true;tables.groups[1][0].definition_index=1;init_mission_tick(s,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},123);
 MissionObjectDestructionBindings b;bool failed=false;try{mission_object_destroy_generic(s,{1,0},b);}catch(const MissionTickError&){failed=true;}assert(failed);
 b.definition=[](unsigned index,unsigned off){assert(index==1&&off==0x20);return 0u;};std::vector<unsigned> sites;unsigned factor=0;
 b.call=[&](unsigned site,const auto& args,std::uint8_t* data,std::size_t size){sites.push_back(site);if(site==0x460a30){assert(size==24&&get(data+8,2)==0&&get(data+10,2)==1);factor=args[3];data[0]=0x42;data[10]=3;}else{assert(site==0x447610&&args[1]==0x42&&args[3]==0x28000);}};
 mission_object_destroy_generic(s,{1,0},b);assert((sites==std::vector<unsigned>{0x460a30,0x447610}));assert(factor==((0x867e80u)<<14));
 // The real callback reaches the helper, then the real client outbox.
 GameMessageStage messages;MissionServerMessageBuffer buffer;std::vector<std::array<std::uint8_t,mission_client_record_size>> clients(17);clients[0][0x18c]=1;clients[0][0x1c4]=5;MissionClientOutboxBindings ob;ob.client=[&](std::size_t i){return MissionClientRecordView{clients.at(i).data(),clients.at(i).size()};};ob.cell=[](unsigned){return 0u;};MissionClientOutbox outbox(messages,buffer,ob);
 MissionObjectLiveBindings live;live.global=[](auto&,unsigned at){assert(at==0xa158c0);return 0x12345678u;};live.call=[&](auto& current,const auto& ref,const auto& action)->MissionObjectCallResult{if(action.site==0x453270)mission_object_destroy_generic(current,ref,b);else{assert(action.site==0x447980);mission_object_destroy_broadcast(outbox,0x867e80,action.args[1]);}return {};};s.globals.mission_67f528=1;s.tables.groups[1][0].field_6c=4;mission_object_generic(s,{1,0},live);assert(s.tables.groups[1][0].field_6c==12&&s.tables.groups[1][0].field_fc==0x20);const auto* row=clients[0].data()+0x204;assert(row[8]==0x39&&get(row+10,2)==(((0x867e80u-0x7cca80u)/0x114)&0xffff)&&get(row+12,2)==0x5678);
 // Wood keeps one local actor image across required synchronous insertion calls.
 b.call={};failed=false;try{mission_object_destroy_wood(s,{1,0},b,false);}catch(const MissionTickError&){failed=true;}assert(failed);
 unsigned lookup=0,spawns=0,sounds=0;b.lookup_4828c0=[&](unsigned){return ++lookup;};b.item_table_base=[](){return 0x1200000u;};b.item_word=[](unsigned ptr,unsigned off){return ptr+off;};
 b.call=[&](unsigned site,const auto& args,std::uint8_t* data,std::size_t size){if(site==0x42a440){assert(!data&&size==0);s.tables.groups[1][0].position[0]=0x8000;}else if(site==0x45d970){assert(size==0x114&&args[1]==0);assert(data[0xe0]==spawns);data[0xe0]=std::uint8_t(++spawns);}else{assert(site==0x43e7d0&&args[0]==0x139);++sounds;}};
 mission_object_destroy_wood(s,{1,0},b,false);assert(spawns==6&&sounds==1&&lookup==2);
 // The large-wood callback uses the same helper and sends the real39 packet.
 lookup=spawns=0;s.tables.groups[1][0].field_72=0xffff;s.tables.groups[1][0].field_6c=0;
 b.model=[&](unsigned,unsigned off){assert(off==0x64);s.tables.groups[1][0].position[0]=0x9000;return 0x100u;};
 unsigned particles=0,area=0;b.call=[&](unsigned site,const auto& args,std::uint8_t* data,std::size_t size){if(site==0x42a440)++particles;else if(site==0x45d970){assert(size==0x114&&data[0xe0]==spawns);data[0xe0]=std::uint8_t(++spawns);}else if(site==0x43e7d0)assert(args[0]==0x13b);else{assert(site==0x44fcb0&&args[0]==0x8f00&&args[2]==0x9100&&args[5]==0xffffffff);++area;}};
 live.call=[&](auto& current,const auto& ref,const auto& action)->MissionObjectCallResult{if(action.site==0x44f270)mission_object_destroy_wood(current,ref,b,true);else{assert(action.site==0x447980);mission_object_destroy_broadcast(outbox,0x867e80,action.args[1]);}return {};};
 mission_object_wood_large(s,{1,0},live,1);assert(spawns==64&&particles==20&&area==1&&s.tables.groups[1][0].field_6c==12);row=clients[0].data()+0x204+0x264;assert(row[8]==0x39&&get(row+12,2)==0x5678);
 b.lookup_4828c0={};failed=false;try{mission_object_destroy_wood(s,{1,0},b,true);}catch(const MissionTickError&){failed=true;}assert(failed); // particles run before the missing lookup
}
