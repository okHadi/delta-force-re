#include "dfre/mission_round_checks.hpp"
#include "dfre/mission_client_outbox.hpp"
#include <cassert>
#include <map>
using namespace dfre;
int main(){
 MissionTickState state;AiActorTables tables;tables.groups[3].resize(2);tables.groups[3][0].active=true;tables.groups[3][1].active=true;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(1)),{},0);state.config.local_actor=0;
 std::map<std::uint32_t,std::uint32_t> cells;std::vector<std::uint32_t> calls;MissionRoundCheckBindings host;
 host.read=[&](auto at,auto){const auto p=cells.find(at);if(p==cells.end())throw MissionTickError("Unbound cell");return p->second;};host.write=[&](auto at,auto,auto n){cells.at(at)=n;};host.call=[&](auto site,const auto& args){calls.push_back(site);assert(site==0x444ca0&&args.at(0)==2);return 0u;};
 auto b=mission_round_check_bindings(state,host);mission_round_check_end(b);assert(calls.empty());state.tables.groups[3][0].field_6c=4;mission_round_check_end(b);assert(calls==std::vector<std::uint32_t>{0x444ca0});
 state.config.local_actor=1;mission_round_check_end(b);assert(calls.size()==1);state.globals.menu_flag_67dbc0=1;state.config.local_actor.reset();mission_round_check_end(b);assert(calls.size()==1);
 b.write(0x7cca80+0x114+0x74,2,0xffff);assert(state.extra_for({3,1}).field_74==0xffff);state.globals.multiplayer_67f520=0;mission_round_check_clients(b);
 bool failed=false;try{mission_round_check_end({});}catch(const MissionTickError&){failed=true;}assert(failed);
 // Mode1 keeps its second limit read across all rows and rereads the timer after result.
 calls.clear();cells={{0x67dbc0,0},{0x67f520,1},{0x67f53c,1},{0x67f544,1},{0x67dbe8,1}};for(unsigned i=0;i<17;++i)cells[0x6e9c1c+i*0x9b84]=0;cells[0x6e9c1c]=1;cells[0x6e9c30]=2;cells[0x6e9c44]=0;host.call=[&](auto site,const auto& args){assert(site==0x444ca0&&args.at(0)==0);calls.push_back(site);cells[0x67dbe8]=0;return 0u;};mission_round_check_end(host);assert(calls.size()==2);
 // A missing reached drop body preserves the timeout increment.
 cells={{0x67f520,1},{0x67f53c,5}};for(unsigned i=1;i<17;++i)cells[0x6e9c1c+i*0x9b84]=0;const unsigned c=0x9b84;cells[c+0x6e9c1c]=1;cells[c+0x6e9c54]=5;cells[c+0x6e9c30]=0;cells[c+0x6e9c40]=0;cells[c+0x6e9c44]=0;cells[c+0x6e9a98]=0x1200000;cells[0x1200090]=0x1200114;cells[0x1200118]=0x1210000;cells[0x1210020]=0xfc3;cells[c+0x6e9c50]=599;host.call={};failed=false;try{mission_round_check_clients(host);}catch(const MissionTickError&){failed=true;}assert(failed&&cells[c+0x6e9c50]==600);
 calls.clear();cells[c+0x6e9c50]=599;cells[0x12002a0]=0;host.call=[&](auto site,const auto& args){calls.push_back(site);if(site==0x4604c0){assert(args.at(0)==0x1200000);cells[c+0x6e9a98]=0x120022c;}else{assert(site==0x4605d0&&args.at(0)==0x1200114);}return 0u;};mission_round_check_clients(host);assert(calls.size()==2&&cells[0x12002a0]==0xffff);
 // Actual status broadcast always visits slot0 and uses current later rows.
 std::vector<AssetBytes> clients(17,AssetBytes(mission_client_record_size));clients[0][0x18d]=1;clients[1][0x18c]=1;clients[1][0x1c4]=5;clients[1][0x18d]=1;
 GameMessageStage messages;MissionServerMessageBuffer buffer;MissionClientOutboxBindings transport;transport.client=[&](auto slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};transport.cell=[](auto){return 0u;};MissionClientOutbox outbox(messages,buffer,transport);
 MissionRoundCheckBindings status;status.read=[&](auto at,auto width){const auto delta=at-0x6e9a90;std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(clients.at(delta/0x9b84).at(delta%0x9b84+i))<<(8*i);return value;};std::vector<unsigned> slots;status.enqueue_441750=[&](auto slot,auto type,auto* payload,auto length){slots.push_back(slot);assert(outbox.enqueue(slot,std::int32_t(type),payload,length));if(slot==0){clients[2][0x18c]=1;clients[2][0x1c4]=5;payload[0]=77;}};
 std::uint8_t byte=19;mission_round_broadcast(status,0x30,&byte,1);assert((slots==std::vector<unsigned>{0,1,2}));assert(clients[0][0x204+10]==19&&clients[1][0x204+10]==77&&clients[2][0x204+10]==77);
 failed=false;try{mission_round_broadcast({},0x30,&byte,1);}catch(const MissionTickError&){failed=true;}assert(failed);
}
