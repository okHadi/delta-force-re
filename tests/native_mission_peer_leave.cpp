#include "dfre/mission_peer_leave.hpp"
#include "dfre/mission_client_live.hpp"
#include "dfre/mission_logic.hpp"
#include "dfre/mission_round_services.hpp"
#include "dfre/mission_peer_frame.hpp"
#include "dfre/mission_peer_parser.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
struct Fixture {
 static constexpr U frame=0x1b006000,squad_base=0x6e8090;
 MissionClientLive clients;std::array<std::uint8_t,64*0x68> squads{};MissionLogicResultPacket packet{};
 std::map<U,U> cells{{0x67f534,1},{0x6e558c,0},{0x67f528,1},{0x67f520,0},{0x6e5588,1},{0x67f52c,1},{0x67f53c,0},{0x67f538,1},{0x501340,7},{0x50133c,7},{0x50321c,0},{0x8e61b4,0},{0x8e61b8,0},{0x8e61bc,0}};
 GameMessageStage messages;MissionServerMessageBuffer buffer;MissionClientOutbox outbox;MissionPeriodicBindings b;MissionLogicResultBindings result;MissionRoundServiceBindings round;
 unsigned built=0,enqueued=0,pumped=0,ticked=0;bool preserve_state=false,fail_enqueue=false,clear_network_after_parse=false;
 std::uint8_t* span(U at,std::size_t width){
  if(at>=mission_client_source_base&&std::uint64_t(at-mission_client_source_base)+width<=mission_client_source_bytes)return clients.client(0).bytes+at-mission_client_source_base;
  if(at>=squad_base&&std::uint64_t(at-squad_base)+width<=squads.size())return squads.data()+at-squad_base;
  if(at>=frame&&std::uint64_t(at-frame)+width<=packet.size())return packet.data()+at-frame;
  throw std::runtime_error("Peer leave native span is unowned");
 }
 U read(U at,U width=4){
  if(clients.claims(at,width))return clients.read(at,width);
  if(cells.count(at)){check(width==4,"Global width differs");return cells.at(at);}
  auto* p=span(at,width);U n=0;for(U i=0;i<width;++i)n|=U(p[i])<<(i*8);return n;
 }
 void write(U at,U width,U n){
  if(clients.claims(at,width)){clients.write(at,width,n);return;}
  if(cells.count(at)){check(width==4,"Global width differs");cells.at(at)=n;return;}
  auto* p=span(at,width);for(U i=0;i<width;++i)p[i]=std::uint8_t(n>>(i*8));
 }
 Fixture():outbox(messages,buffer,{
  [&](auto slot){return clients.client(slot);},[&](auto at){return read(at);},{},{}}){
  const auto cr=[&](auto slot,auto at,auto width){return clients.read(MissionClientLive::source_address(slot,at),width);};
  round.cell=[&](auto at,auto width){return read(at,width);};round.client=cr;
  round.squad=[&](auto slot,auto at,auto width){return read(squad_base+U(slot)*0x68+at,width);};round.squad_store=[&](auto slot,auto at,auto width,auto n){write(squad_base+U(slot)*0x68+at,width,n);};
  result.state.cell=[&](auto at){return read(at);};result.state.client=cr;
  result.state.call=[&](auto site,const auto& args)->U{if(site==0x446da0){mission_round_collect_squads(round);return 0;}if(site==0x448130)return U(mission_round_compare_clients(round,args.at(0),args.at(1)));throw std::runtime_error("Result child is unowned");};
  result.actor_source_address=[](auto physical){return physical;};
  b.read=[&](auto at,auto width){return read(at,width);};b.write=[&](auto at,auto width,auto n){write(at,width,n);};
  b.call=[&](U site,const std::vector<U>& args)->U{
   if(site==0x49a620){std::fill_n(span(args.at(0),args.at(2)),args.at(2),std::uint8_t(args.at(1)));return 0;}
   if(site==0x4403c0){check(args.at(0)==frame,"Builder lost shared packet");mission_logic_build_result(result,packet);++built;return 0;}
   if(site==0x441750){check(args.at(2)==frame&&args.at(3)==packet.size(),"Enqueue packet alias differs");const U slot=(args.at(0)-mission_client_source_base)/mission_client_record_size;check(outbox.enqueue(slot,std::int32_t(args.at(1)),packet.data(),U(packet.size())),"Real outbox rejected packet");++enqueued;if(preserve_state)write(args[0]+0x1c4,2,1);if(fail_enqueue)throw std::runtime_error("Enqueue callback failed after publication");return 0;}
   if(site==0x4421d0){outbox.tick();++ticked;return 0;}if(site==0x490880){++pumped;return 0;} // Explicit host pump observation.
   if(site==0x460e40){MissionPeerFrameBindings peer;peer.cell=[&](auto at){return read(at);};peer.store=[&](auto at,auto n){write(at,4,n);};return mission_peer_frame_receive(messages.peer_frame_buffer(),peer);}
   if(site==0x442ae0){MissionPeerParserBindings parser;parser.read=b.read;parser.write=b.write;parser.client=[&](auto slot){return clients.client(slot);};parser.resolve=[&](U at,std::size_t length){auto& bytes=messages.peer_frame_buffer().bytes_8ec1cc;if(at<0x8ec1cc||std::uint64_t(at-0x8ec1cc)+length>bytes.size())throw std::runtime_error("Peer source is unowned");return bytes.data()+at-0x8ec1cc;};mission_peer_parser_parse(outbox,messages.table(),parser);if(clear_network_after_parse)cells.at(0x67f520)=0;return 0;}
   throw std::runtime_error("Peer leave child is unowned");
  };
 }
 void client(U slot,U at,U width,U n){write(MissionClientLive::source_address(slot,at),width,n);}
 void setup(){client(0,0,4,7);client(0,8,4,0x7cca80);client(0,0x18c,1,1);client(0,0x1c4,2,3);client(0,0x1a0,4,9);client(0,0x1a8,4,2);client(0,0x1ac,4,4);client(0,0x1b8,4,123);client(0,0x1bc,4,99);client(0,0x1c8,2,0xffff);client(16,0x9b83,1,0xa5);write(squad_base+0x40,4,0x80000000);write(squad_base+0x44,4,0xffffffff);write(squad_base+0x64,4,0x80000000);}
};
void reset_join(){Fixture f;f.setup();f.preserve_state=true;mission_peer_reset(f.b,Fixture::frame);
 check(f.built==1&&f.enqueued==1&&f.packet[1]==1&&f.packet[2]==1,"Reset lost real result packet mode/count");
 check(f.clients.read(mission_client_source_base+0x1c4,2)==1&&f.clients.read(mission_client_source_base+8,4)==0,"Reset did not reread callback state before clear");
 check(f.clients.read(mission_client_source_base+0x1b8,4)==124&&f.clients.read(mission_client_source_base+0x1bc,4)==0&&f.clients.read(mission_client_source_base+0x1c8,2)==0,"Reset cleared a retained field or missed an exact field");
 check(f.clients.read(MissionClientLive::source_address(16,0x9b83),1)==0&&f.read(Fixture::squad_base+0x40)==0x80000000&&f.read(Fixture::squad_base+0x44)==0&&f.read(Fixture::squad_base+0x64)==0,"Physical tails or squads were not current");
 check(f.clients.client(0).bytes[0x204+8]==0x1e&&std::equal(f.packet.begin(),f.packet.end(),f.clients.client(0).bytes+0x204+10),"Real client queue did not copy shared packet bytes");
 f.outbox.tick();check(!f.messages.pending_local_echo().empty(),"Real result outbox did not reach SP echo");
 Fixture fail;fail.setup();fail.fail_enqueue=true;bool rejected=false;try{mission_peer_reset(fail.b,Fixture::frame);}catch(const std::runtime_error&){rejected=true;}check(rejected&&fail.enqueued==1&&fail.clients.read(mission_client_source_base+8,4)==0x7cca80&&fail.clients.read(MissionClientLive::source_address(16,0x9b83),1)==0xa5,"Failed enqueue lost committed prefix or ran later resets");
}
void drain_join(){Fixture f;f.setup();f.client(0,0x1c4,2,0);f.cells.at(0x67f520)=1;auto& peer=f.messages.peer_frame_buffer();peer.bytes_8ec1cc[0]=2;peer.bytes_8ec1cc[1]=0x34;peer.bytes_8ec1cc[2]=0x12;peer.count_8ee1cc=3;
 bool rejected=false;try{mission_peer_leave_drain(f.b);}catch(const std::runtime_error& e){check(std::string(e.what())=="Peer receive transport is not bound","Strict drain failed before its required transport boundary");rejected=true;}check(rejected&&peer.count_8ee1cc==0&&f.clients.read(mission_client_source_base+0x1c4,2)==3&&f.pumped==0&&f.ticked==0,"Missing transport did not preserve real receive/parser prefix");
 // A declared callback mutation clears networking after real parsing. The outer
 // gates stay captured; real receive now ends without reaching transport.
 Fixture current;current.setup();current.client(0,0x1c4,2,0);current.cells.at(0x67f520)=1;current.clear_network_after_parse=true;auto& data=current.messages.peer_frame_buffer();data.bytes_8ec1cc[0]=2;data.bytes_8ec1cc[1]=0x34;data.bytes_8ec1cc[2]=0x12;data.count_8ee1cc=3;
 check(mission_peer_leave_drain(current.b)==1&&current.ticked==1&&current.pumped==1&&current.cells.at(0x67f520)==0,"Drain lost source loop count or final call order after current gate change");
 MissionPeriodicBindings missing;missing.read=[](U at,U){check(at==0x67f528,"Disabled drain read a later gate");return 0u;};check(mission_peer_leave_drain(missing)==0,"Disabled drain called a child");
}
}
int main(){try{reset_join();drain_join();std::cout<<"Peer reset canonical clients/results/outbox and strict receive/parser checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
