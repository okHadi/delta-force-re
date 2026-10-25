#include "dfre/mission_client_ready.hpp"
#include "dfre/mission_peer_parser.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_event_actor_orders.hpp"
#include "dfre/mission_hud_message.hpp"
#include "dfre/motion_queries.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
namespace {using U=std::uint32_t;template<class F>bool fails(F f){try{f();}catch(const std::exception&){return true;}return false;}}
int main(){using namespace dfre;
 // Declared current startup inputs: clients, actor/person, marker lists and weapon/content tables.
 MissionClientLive clients;MissionTickState state;AiActorTables tables;tables.groups[3].resize(1);tables.groups[3][0].active=true;tables.groups[3][0].definition_index=1;tables.groups[3][0].person_slot=0;tables.groups[3][0].field_6c=2;tables.groups[3][0].field_70=1;tables.groups[3][0].position[0]=0x20000;tables.groups[3][0].position[1]=0x30000;tables.groups[3][0].position[2]=0x40000;std::vector<AiPersonState>people(1);init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::move(people),0);
 state.world.behavior_world.lists.resize(128);auto&list=state.world.behavior_world.lists[1];list.flags=2;list.count=2;list.indices[0]=17;list.indices[1]=18;auto physical=mission_event_actor_order_bindings(state);
 TerrainGeometry terrain(AssetBytes(1024*1024,7),AssetBytes(1024*1024),std::vector<U>(65536));state.world.terrain=&terrain;
 const U c=MissionClientLive::source_address(1),actor=mission_actor_source_address(3,0),wire=0x1300000,frame=0x1310000,reject=0x1311000;std::map<U,std::uint8_t>memory;std::vector<std::uint8_t>packet;MissionClientAddedBindings b;
 b.read=[&](U at,U n){if(at>=wire&&std::uint64_t(at-wire)+n<=packet.size()){U v=0;for(U i=0;i<n;++i)v|=U(packet.at(at-wire+i))<<(8*i);return v;}if(clients.claims(at,n))return clients.read(at,n);if((at>=actor&&std::uint64_t(at-actor)+n<=0x114)||(at>=0x78f280&&std::uint64_t(at)+n<=0x793680))return physical.read(at,n);U v=0;for(U i=0;i<n;++i)v|=U(memory[at+i])<<(8*i);return v;};
 b.write=[&](U at,U n,U value){if(clients.claims(at,n)){clients.write(at,n,value);return;}if(at>=actor&&std::uint64_t(at-actor)+n<=0x114){auto image=snapshot_mission_actor(state,{3,0});for(U i=0;i<n;++i)image.at(at-actor+i)=std::uint8_t(value>>(8*i));apply_mission_actor_image(state,{3,0},image);return;}if(at>=0x794680&&std::uint64_t(at)+n<=0x7cca80){physical.write(at,n,value);return;}for(U i=0;i<n;++i)memory[at+i]=std::uint8_t(value>>(8*i));};
 const auto put=[&](U at,U v,U n=4){b.write(at,n,v);};const auto text=[&](U at,const std::string&str){for(U i=0;i<=str.size();++i)put(at+i,i<str.size()?std::uint8_t(str[i]):0,1);};
 put(c,77);put(c+8,actor);put(c+0x18c,1,1);put(c+0x1c4,1,2);for(U i=0;i<10;++i)put(c+0xac+i*4,i);put(c+0x168,1);put(0xad3ed0,0x1320000);put(0xad3ee4,0x1330000);for(U i=0;i<10;++i){put(0x1320000+i*0xa8+0x74,i);put(0x1320000+i*0xa8+0x6c,1);put(0x1320000+i*0xa8+0xa4,1,1);put(0x1330000+i*0x34+4,3);put(0x1330000+i*0x34+8,7);}for(U i=0;i<9;++i)put(0xbd67d4+i*4,100+i);put(0x8cf9b4,200);put(0x8cf9b8,201);put(0x67f520,1);put(0x67f528,1);put(0x67db40,0x12345678);put(0x503238,0x1340000);put(0x1340100,0x1340200);text(0x1340200,"Ridge");text(0x1340300,"");
 GameMessageStage messages;MissionServerMessageBuffer server;MissionClientOutboxBindings ob;ob.client=[&](auto slot){return clients.client(slot);};ob.cell=[&](U at){return b.read(at,4);};MissionClientOutbox outbox(messages,server,ob);U ready_calls=0,resupplies=0,rejects=0,popups=0,lookups=0;bool fail_fill=false,fail_resupply=false;MissionHudMessageBindings hud;hud.read=b.read;hud.write=b.write;hud.call=[&](U site,const auto&args)->U{assert(site==0x49a092&&args[0]==0x1340000&&args[1]==0x4cc684);++lookups;put(0x1340100,lookups%2?0x1340200:0x1340300);return 0x1340100;};
 b.call=[&](U site,const std::vector<U>&args)->U{
  if(site==0x49a620){if(fail_fill)throw MissionClientAddedError("Required fill failure");for(U i=0;i<args.at(2);++i)put(args.at(0)+i,args.at(1),1);return args[0];}
  if(site==0x493a80){if(args.at(1)==0x4cc67c){assert(args.size()==3);text(args[0],"WPN_"+std::to_string(args[2]));}else if(args[1]==0x4cc570){assert(args.size()==3);text(args[0],"WP"+std::to_string(args[2]));}else{assert(args[1]==0x4cc278&&args.size()==4&&args[3]==c+0xc);text(args[0],"Reject Peer");}return 0;}
  if(site==0x404d80)return mission_hud_triggered_lookup(hud,args.at(0),args.at(1));
  if(site==0x441750){assert(args.size()==4&&args[0]>=mission_client_source_base);const U slot=(args[0]-mission_client_source_base)/mission_client_record_size;std::vector<std::uint8_t>data(args[3]);for(U i=0;i<args[3];++i)data[i]=std::uint8_t(b.read(args[2]+i,1));assert(outbox.enqueue(slot,std::int32_t(args[1]),data.data(),args[3]));return 0;}
  if(site==0x447e80){if(fail_resupply)throw MissionClientAddedError("Required resupply failure");++resupplies;mission_client_resupply(b,args.at(0));return 0;}
  if(site==0x447fb0){++rejects;put(reject+0x48,args.at(0));mission_client_reject(b,reject,0x4c);return 0;}
  if(site==0x404cf0){assert(args==std::vector<U>({0x4cc178,0x4cc6bc}));return 0x1340200;}
  if(site==0x479f00){assert(args==std::vector<U>({reject,0x12345678}));++popups;return 0;}
  throw MissionClientAddedError("Required ready service is not bound");};
 MissionPeerParserBindings parser;parser.read=b.read;parser.write=b.write;parser.client=ob.client;parser.resolve=[&](U at,std::size_t n){if(at<wire||std::uint64_t(at-wire)+n>packet.size())throw MissionClientAddedError("Current parser packet is not mapped");return packet.data()+at-wire;};
 parser.call=[&](const MissionPeerParserCall&call)->U{if(call.address==0x446a30){assert(call.count==2&&call.args[0]==1&&call.payload_argument==1&&call.length==0xe2);assert(call.payload==packet.data()+call.args[1]-wire);++ready_calls;put(frame+0x120,call.args[0]);put(frame+0x124,call.args[1]);mission_client_ready(b,frame,0x128);return 0;}if(call.address==0x47df40){assert(call.count==2&&state.world.terrain);std::int32_t x,y;std::memcpy(&x,&call.args[0],4);std::memcpy(&y,&call.args[1],4);return U(ground_height_query(*state.world.terrain,x,y));}if(call.address==0x448c00){assert(call.count==1&&call.args[0]==1);return 0;}throw MissionClientAddedError("Required parser ready handler is not bound");};
 U sequence=1;const auto build=[&](U id,U bytes){packet.clear();packet.push_back(std::uint8_t(id));if(messages.table().descriptor(std::uint8_t(id)).flags&6){packet.push_back(std::uint8_t(sequence));packet.push_back(std::uint8_t(sequence>>8));++sequence;}const U start=U(packet.size());packet.resize(start+bytes);put(0x8e61b4,wire);put(0x8e61b8,U(packet.size()));put(0x8e61bc,77);return start;};
 const auto store_packet=[&](U at,U value,U n=4){for(U i=0;i<n;++i)packet.at(at+i)=std::uint8_t(value>>(8*i));};const auto ready_packet=[&]{const U start=build(0xb,0xe2);store_packet(start,'A',1);store_packet(start+0x20,'B',1);for(U i=0;i<9;++i)store_packet(start+0xba+i*4,100+i);return start;};const auto parse=[&]{mission_peer_parser_parse(outbox,messages.table(),parser);};
 ready_packet();assert(b.read(c+0x1c4,2)==1);parse();assert(ready_calls==1&&resupplies==1&&rejects==0&&b.read(c+0x1c4,2)==5);assert((snapshot_mission_actor(state,{3,0})[0x6c]&2)==0&&state.people[0].field_ac==1);assert(b.read(frame+0x48,2)==2&&b.read(frame+0x4a,2)==17&&b.read(frame+0x4c,2)==18);assert(b.read(frame+0x5a,1)=='R'&&b.read(frame+0x66,1)=='W');for(U i=0;i<10;++i)assert(b.read(c+0xd4+i*4,4)==(i>=3&&i<6?43u:22u));assert(b.read(c+0x154,4)==1000&&b.read(c+0x4c,1)=='A'&&b.read(c+0x2c,1)=='B');assert(lookups==2);
 // Ready was produced by actual message11; message10 now reaches the current actor/terrain writes.
 U start=build(10,16);store_packet(start,0xfffe0000);store_packet(start+4,0x30000);store_packet(start+8,0x12,1);store_packet(start+9,0x34,1);store_packet(start+10,5,1);store_packet(start+11,6,1);store_packet(start+12,1);put(0x67dbc0,0);put(0x67f538,10);parse();auto image=snapshot_mission_actor(state,{3,0});U value=0;std::memcpy(&value,image.data()+0x24,4);assert(value==0xfffe0000);std::memcpy(&value,image.data()+0x2c,4);assert(value==U(ground_height_query(terrain,-0x20000,0x30000)));assert(image[0x7c]==5&&image[0x94]==6&&b.read(c+0x170,4)==1&&b.read(c+0x174,4)==0xfffe0000);
 start=ready_packet();store_packet(start+0xda,999);parse();assert(rejects==1&&popups==1&&resupplies==2&&b.read(c+0x1c4,2)==5);
 // A reached fill failure keeps early source readiness, actor flag and strings, then stops.
 ready_packet();put(c+0x1c4,1,2);fail_fill=true;assert(fails(parse));assert(b.read(c+0x1c4,2)==5&&b.read(c+0x4c,1)=='A');fail_fill=false;ready_packet();fail_resupply=true;const auto before=resupplies;assert(fails(parse));assert(resupplies==before&&b.read(c+0x1c4,2)==5);fail_resupply=false;
 auto missing=b;missing.call={};put(reject+0x48,1);assert(fails([&]{mission_client_reject(missing,reject,0x4c);}));assert(fails([&]{mission_client_ready(b,frame,0x127);}));assert(fails([&]{mission_client_reject(b,reject,0x4b);}));for(U slot:std::array<U,3>{0,17,0xffffffffu}){put(reject+0x48,slot);mission_client_reject(missing,reject,0x4c);}put(reject+0x48,1);put(c+0x18c,0,1);mission_client_reject(missing,reject,0x4c);put(c+0x18c,1,1);put(0x67f520,0);mission_client_reject(missing,reject,0x4c);
 std::cout<<"Current message11 ready/resupply/outbox/movement checks passed\n";
}
