#include "dfre/mission_df_network_callbacks.hpp"
#include "dfre/mission_client_admission_helpers.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_client_startup.hpp"
#include "dfre/mission_actor_lifecycle.hpp"
#include "dfre/mission_round_checks.hpp"
#include "dfre/mission_df_system_receive.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <algorithm>
namespace {using U=std::uint32_t;template<class F>bool fails(F f){try{f();}catch(const std::exception&){return true;}return false;}}
int main(){using namespace dfre;
 MissionClientLive clients;std::map<U,std::uint8_t>memory;MissionClientAddedBindings b;
 b.read=[&](U at,U width){if(clients.claims(at,width))return clients.read(at,width);U v=0;for(U i=0;i<width;++i)v|=U(memory[at+i])<<(8*i);return v;};
 b.write=[&](U at,U width,U value){if(clients.claims(at,width)){clients.write(at,width,value);return;}for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(value>>(8*i));};
 const auto put=[&](U at,U value,U width=4){b.write(at,width,value);};const auto text=[&](U at,const char*p){do{put(at++,std::uint8_t(*p),1);}while(*p++);};
 MissionTickState state;AiActorTables tables;tables.groups[3].resize(2);tables.groups[3][1].active=true;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable{}, {},0);MissionAnimations animations({});std::array<MissionAudioRegistration,1024> registrations{};registrations[0].actor=mission_actor_source_address(3,1);registrations[0].definition=12;MissionActorLifecycle lifecycle(state,animations,registrations,{});
 GameMessageStage messages;MissionServerMessageBuffer server;MissionClientOutboxBindings ob;ob.client=[&](auto slot){return clients.client(slot);};ob.cell=[&](U at){return b.read(at,4);};MissionClientOutbox outbox(messages,server,ob);MissionRoundCheckBindings rb;rb.read=b.read;rb.write=b.write;rb.enqueue_441750=[&](U slot,U type,std::uint8_t*data,U n){assert(outbox.enqueue(slot,std::int32_t(type),data,n));};
 const U c0=MissionClientLive::source_address(0),c1=MissionClientLive::source_address(1),scratch=0x1300000,sys=0x1301000,packet=0x1302000;
 put(c0+0x18c,1,1);put(c0+0x1c4,5,2);put(c0+0x1d0,1,1);put(c1,77);put(c1+8,mission_actor_source_address(3,1));put(c1+0x18c,1,1);put(c1+0x1c4,5,2);text(c1+0xc,"Peer");put(0x67f528,1);put(0x501368,1);put(0x501348,2);put(0x501358,0);text(0x1303000,"$A left");
 U removed=0,broadcasts=0,diagnostics=0,popup=0;bool fail_broadcast=false,mutate_diagnostic=true;std::vector<U>calls;
 b.call=[&](U site,const std::vector<U>&args)->U{calls.push_back(site);
  if(site==0x493a80){assert(args.size()==3&&args[2]==77);put(args[0],0,1);return args[0];}
  if(site==0x4564b0){++diagnostics;if(mutate_diagnostic)put(0x501344,99);return 0;}
  if(site==0x404cf0){assert(args[0]==0x4cc178);return 0x1303000;}
  if(site==0x43f770)return mission_client_name_message(b,args.at(0),args.at(1));
  if(site==0x45da30){assert(args==std::vector<U>{mission_actor_source_address(3,1)});lifecycle.remove(AiActorRef{3,1});++removed;return 0;}
  if(site==0x49a620){for(U i=0;i<args.at(2);++i)put(args.at(0)+i,args.at(1),1);return args[0];}
  if(site==0x442450){mission_client_clear_references(b,args.at(0));return 0;}
  if(site==0x4423f0){++broadcasts;if(fail_broadcast)throw MissionClientAddedError("Required broadcast failure");std::vector<std::uint8_t>payload(args.at(2));for(U i=0;i<args.at(2);++i)payload[i]=std::uint8_t(b.read(args.at(1)+i,1));mission_round_broadcast(rb,args.at(0),payload.data(),args.at(2));return 0;}
  if(site==0x479f00){assert(args==std::vector<U>({0x1303000,0x11223344}));++popup;return 0;}
  throw MissionClientAddedError("Required callback service is not bound");};
 MissionClientStartupBindings startup;startup.read=b.read;startup.write=b.write;mission_client_register_callback(startup,0x497500,0x43fd60);mission_client_register_callback(startup,0x4974e0,0x43ff20);mission_client_register_callback(startup,0x4974d0,0x43fef0);
 MissionGameTransportBindings system;system.read=b.read;system.write=b.write;system.call=[&](U site,const auto&args)->U{assert(args.empty());mission_df_network_callback(b,site,scratch,0xe8);return 0;};
 for(U i=0;i<4;++i)put(0x4b93f0+4*i,std::vector<U>{0x4b9432,0x4b94a2,0x4b94d9,0x4b94f2}[i]);put(sys+0x98,packet);put(packet,0x1051,2);put(packet+4,77);mission_df_system_receive(system,sys);
 assert(b.read(0x501344,4)==99&&b.read(0x501348,4)==1&&b.read(0x501358,4)==1);assert(removed==1&&broadcasts==2&&diagnostics==1);assert(snapshot_mission_actor(state,{3,1})==AiActorImage{}&&registrations[0].definition==0);assert(b.read(c1+0x18c,1)==0&&b.read(c0+0x1d0,1)==0);assert(b.read(c0+0x204+8,1)==0x10&&b.read(c0+0x204+10,2)==1);assert(b.read(c0+0x204+0x264+8,1)==0x12);assert(b.read(c0+0x204+0x264+10,2)==13);assert(calls==std::vector<U>({0x493a80,0x4564b0,0x404cf0,0x43f770,0x45da30,0x4423f0,0x49a620,0x4423f0,0x442450}));
 put(packet,0x1052,2);put(0x67db68,0);mission_df_system_receive(system,sys);assert(b.read(0x67db68,4)==1);
 put(0x67db40,0x11223344);auto popup_binding=b;popup_binding.call=[&](U site,const auto&args)->U{if(site==0x404cf0){put(0x67db40,999);return 0x1303000;}return b.call(site,args);};mission_df_network_callback(popup_binding,b.read(0x50137c,4));assert(popup==1);auto required=b;required.call={};assert(fails([&]{mission_df_network_callback(required,0x43fef0);}));assert(fails([&]{mission_df_network_callback(b,0x43fd60,scratch,0xe7);}));assert(fails([&]{mission_df_network_callback(b,0);}));
 // Broadcast failure leaves actual actor removal before client clear/reference cleanup.
 state.tables.groups[3][1].active=true;put(c1,77);put(c1+8,mission_actor_source_address(3,1));put(c1+0x18c,1,1);put(c0+0x1d0,1,1);put(0x501344,77);fail_broadcast=true;assert(fails([&]{mission_df_network_callback(b,0x43fd60,scratch,0xe8);}));assert(snapshot_mission_actor(state,{3,1})==AiActorImage{}&&b.read(c1+0x18c,1)==1&&b.read(c0+0x1d0,1)==1);
 // Exact current pointer reload: the system receiver reloads after its gate read.
 auto current=system;current.read=[&](U at,U n){const auto result=b.read(at,n);if(at==0x501380)put(0x501380,0x43ff20);return result;};put(0x501380,0x43fef0);put(0x67db68,0);put(packet,0x1052,2);mission_df_system_receive(current,sys);assert(b.read(0x67db68,4)==1);
 put(0x67f528,0);put(0x501344,77);fail_broadcast=false;const auto prior=removed;mission_df_network_callback(b,0x43fd60,scratch,0xe8);assert(removed==prior);
 std::cout<<"DF current network callback checks passed\n";
}
