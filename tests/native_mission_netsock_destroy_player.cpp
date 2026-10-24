#include "dfre/mission_netsock_create_player.hpp"
#include "dfre/mission_netsock_destroy_player.hpp"
#include "dfre/mission_netsock_admission.hpp"
#include "dfre/mission_netsock_dispatch.hpp"
#include "dfre/mission_netsock_host_setup.hpp"
#include "dfre/mission_netsock_host_worker.hpp"
#include "dfre/mission_netsock_client_worker.hpp"
#include "dfre/mission_netsock_close.hpp"
#include "dfre/mission_netsock_join.hpp"
#include "dfre/mission_netsock_outgoing.hpp"
#include "dfre/mission_netsock_send_leaves.hpp"
#include "dfre/mission_netsock_send_exports.hpp"
#include "dfre/mission_netsock_periodic.hpp"
#include "dfre/mission_netsock_startup.hpp"
#include "dfre/mission_df_join.hpp"
#include "dfre/mission_df_system_receive.hpp"
#include "dfre/mission_game_transport.hpp"
#include "dfre/network.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace {
using U=std::uint32_t;using Bytes=dfre::AssetBytes;using Args=std::vector<U>;
struct Peer {
 struct Incoming{Bytes wire;U ip,port;};
 std::recursive_mutex memory_guard;std::mutex gate;std::condition_variable changed;
 std::map<U,Bytes> blocks;U next=0x22000000,ip,port;
 dfre::MissionNetsockPacketMemoryBindings foreign;std::unique_ptr<dfre::MissionNetsockPacketMemory> dll;dfre::MissionNetsockPacketBindings b;
 std::map<std::thread::id,std::vector<U>> frames;std::map<U,std::thread> threads;std::map<U,std::exception_ptr> errors;
 std::deque<Incoming> incoming;std::vector<U> sent;Peer*other=nullptr;bool allow_host=false,allow_enum=false,allow_join=false,join_done=false,host_park=false,host_parked=false,host_release=false;U enum_seen=0,received=0,completed=0;
 explicit Peer(const Bytes&raw,U address,U endpoint):ip(address),port(endpoint){
  blocks[0x400000].resize(0xa00000);blocks[0x10000000].resize(0x1b000);
  foreign.read=[&](U a,U n){std::lock_guard<std::recursive_mutex> lock(memory_guard);return rawread(a,n);};foreign.write=[&](U a,U n,U v){std::lock_guard<std::recursive_mutex> lock(memory_guard);rawwrite(a,n,v);};foreign.resolve=[&](U a,std::size_t n){std::lock_guard<std::recursive_mutex> lock(memory_guard);return rawptr(a,n);};
  if(!raw.empty())dll=std::make_unique<dfre::MissionNetsockPacketMemory>(raw,foreign);
  std::fill_n(ptr(0x10019880,0x426),0x426,0);
  if(!dll){for(U i=0;i<256;++i){U v=i<<24;for(U j=0;j<8;++j)v=(v<<1)^((v&0x80000000u)?0x04c11db7u:0);put(0x10007234+i*4,v);}text(0x10007014,"%08x:%04x");text(0x10007020,"CUST_ID.DAT");text(0x1000702c,"rt");text(0x10007030,"LOSEPCKT.NFO");text(0x100070ec,"NWUSAGE.CFG");put(0x10007060,0xe65ae7d0);put(0x10007064,0x2827,2);const U branches[]={0x100046e8,0x10004774,0x100046fc,0x10004710,0x10004724,0x10004738,0x10004760,0x1000474c,0x1000477b};for(U i=0;i<9;++i)put(0x1000477c+i*4,branches[i]);for(U i=0;i<0x71;++i)put(0x100047a0+i,8,1);for(auto[a,v]:std::vector<std::pair<U,U>>{{0,0},{2,1},{0x10,2},{0x20,3},{0x22,4},{0x30,5},{0x70,7}})put(0x100047a0+a,v,1);const U client_branches[]={0x10002f8a,0x10002fa1,0x10002fb8,0x10002fcf,0x10002fe2,0x10002ffa,0x10003003};for(U i=0;i<7;++i)put(0x10003014+i*4,client_branches[i]);for(U i=0;i<0x6f;++i)put(0x10003030+i,6,1);for(auto[a,v]:std::vector<std::pair<U,U>>{{0,5},{0x1f,0},{0x21,1},{0x2f,2},{0x40,3},{0x6e,4}})put(0x10003030+a,v,1);}
  b.read=[&](U a,U n){return get(a,n);};b.write=[&](U a,U n,U v){put(a,v,n);};b.resolve=[&](U a,std::size_t n){return ptr(a,n);};
  b.frame=[&](U s,U n,const Args&args){std::lock_guard<std::recursive_mutex> lock(memory_guard);const U f=allocate(n);frames[std::this_thread::get_id()].push_back(f);U off=s==0x10001530||s==0x10004280||s==0x10001be0||s==0x10001c30?8:s==0x10001660?0x18:s==0x100052e0?0x34:s==0x10005020?0x24:s==0x100020d0?0xc:s==0x100021d0?0x104:s==0x10002710?0x74:s==0x10002620?0x40:s==0x10002b80?0x60:4;for(U v:args){put(f+off,v);off+=4;}return f;};
  b.release=[&](U,U f,U){std::lock_guard<std::recursive_mutex> lock(memory_guard);auto&stack=frames.at(std::this_thread::get_id());assert(stack.back()==f);stack.pop_back();assert(blocks.erase(f)==1);};b.call=[&](U s,const Args&args){return call(s,args);};
  for(auto[a,v]:std::vector<std::pair<U,U>>{{0x1000605c,0x1100100},{0x10006050,0x1100200},{0x10006058,0x1100300},{0x10006068,0x1100400},{0x10006060,0x1100500},{0x10006018,0x1100600},{0x10006078,0x1100700},{0x10006064,0x1100800},{0x10006088,0x1100900},{0x1000604c,0x1100a00},{0x10006044,0x1100b00},{0x10006010,0x1100c00},{0x1000606c,0x1100d00},{0x10006048,0x1100e00},{0x100096b4,55},{0x10019880,1},{0x10019884,1},{0x10019854,allocate(0x10000)},{0x1001985c,0x10000},{0x10019c5c,allocate(0x10000)},{0x10019c64,0x10000}})put(a,v);
  assert(dfre::mission_netsock_startup_init(b)==0);put(0x10019884,1);put(0x10019854,allocate(0x10000));put(0x1001985c,0x10000);put(0x100198a8,ip);put(0x100199b4,ip);put(0x10019868,port,2);
 }
 std::uint8_t*rawptr(U a,std::size_t n){auto i=blocks.upper_bound(a);if(i==blocks.begin())throw std::runtime_error("Peer memory is not mapped");--i;if(std::uint64_t(a-i->first)+n>i->second.size())throw std::runtime_error("Peer range exceeds its owner");return i->second.data()+a-i->first;}
 U rawread(U a,U n){U v=0;for(U i=0;i<n;++i)v|=U(rawptr(a+i,1)[0])<<(8*i);return v;}void rawwrite(U a,U n,U v){for(U i=0;i<n;++i)rawptr(a+i,1)[0]=std::uint8_t(v>>(8*i));}
 U get(U a,U n=4){std::lock_guard<std::recursive_mutex> lock(memory_guard);return dll?dll->read(a,n):rawread(a,n);}void put(U a,U v,U n=4){std::lock_guard<std::recursive_mutex> lock(memory_guard);if(dll)dll->write(a,n,v);else rawwrite(a,n,v);}std::uint8_t*ptr(U a,std::size_t n){std::lock_guard<std::recursive_mutex> lock(memory_guard);return dll?dll->resolve(a,n):rawptr(a,n);}
 U allocate(U n){std::lock_guard<std::recursive_mutex> lock(memory_guard);if(!n||n>0x100000)throw std::runtime_error("Peer allocation size is not supported");const U a=next;next+=(n+4095)&~4095u;blocks[a].resize(n,0x5a);return a;}
 void text(U a,const std::string&s){for(U i=0;i<s.size();++i)put(a+i,std::uint8_t(s[i]),1);put(a+U(s.size()),0,1);}std::string text(U a){std::string s;for(U i=0;;++i){const U c=get(a+i,1);if(!c)return s;s.push_back(char(c));}}
 U run(U s,U n,const Args&args,const std::function<U(U)>&body){const U f=b.frame(s,n,args);try{const U v=body(f);b.release(s,f,n);return v;}catch(...){b.release(s,f,n);throw;}}
 U config(){const U a=allocate(0x100);std::fill_n(ptr(a,0x100),0x100,0);put(a,0x100);put(a+8,8);text(a+0x90,"pw");const U app[]={0xe243fc00,0x11d16b35,0xc0008787,0x25af16f0};for(U i=0;i<4;++i){put(a+0xd0+i*4,0x11111111u*(i+1));put(a+0xe0+i*4,app[i]);put(0x4cebc4+i*4,app[i]);}return a;}
 U player(const std::string&name){const U a=allocate(0x98),data=allocate(7);std::fill_n(ptr(a,0x98),0x98,0);text(a,name);for(U i=0;i<7;++i)put(data+i,0x80+i,1);put(a+0x80,data);put(a+0x84,7);return a;}
 void start(U site){assert(!threads.count(site));threads.emplace(site,std::thread([&,site]{
  {std::unique_lock<std::mutex>lock(gate);changed.wait(lock,[&]{return site==0x10004930?allow_host:site==0x10003680?allow_enum:site==0x10003960?allow_join:join_done;});}
  try{run(site,12,{},[&](U f){if(site==0x10004930)return dfre::mission_netsock_host_worker(b,f);if(site==0x10003680)return dfre::mission_netsock_close_call(b,site,f);if(site==0x10003960)return dfre::mission_netsock_join_call(b,site,f);return dfre::mission_netsock_client_worker_call(b,site,f);});}catch(...){std::lock_guard<std::mutex>lock(gate);errors[site]=std::current_exception();}
  {std::lock_guard<std::mutex>lock(gate);if(site==0x10003960)join_done=true;}changed.notify_all();
 }));}
 void join(U site){auto i=threads.find(site);if(i==threads.end())return;i->second.join();threads.erase(i);if(errors.count(site))std::rethrow_exception(errors.at(site));}
 template<class F>void wait(F predicate){std::unique_lock<std::mutex> lock(gate);if(!changed.wait_for(lock,std::chrono::seconds(12),[&]{return !errors.empty()||predicate();})){std::fprintf(stderr,"Peer %x timeout: received=%u completed=%u enum=%u role=%u active=%u join=%u ops",ip,received,completed,enum_seen,get(0x100199b0),get(0x100198ac),get(0x100199ec));for(U op:sent)std::fprintf(stderr," %x",op);std::fprintf(stderr,"\n");assert(false);}if(!errors.empty())std::rethrow_exception(errors.begin()->second);}
 U call(U s,const Args&args){using namespace dfre;switch(s){
 case 0x1100100:return allocate(args.at(0));case 0x1100200:{const U a=allocate(args.at(0)*args.at(1));std::fill_n(ptr(a,args[0]*args[1]),args[0]*args[1],0);return a;}case 0x1100300:{const U a=allocate(args.at(1));std::lock_guard<std::recursive_mutex>lock(memory_guard);auto i=blocks.find(args.at(0));assert(i!=blocks.end());std::copy_n(i->second.data(),std::min<std::size_t>(i->second.size(),args[1]),ptr(a,args[1]));blocks.erase(i);return a;}case 0x1100400:{std::lock_guard<std::recursive_mutex>lock(memory_guard);assert(blocks.erase(args.at(0))==1);return 0;}
 case 0x1100500:{const std::string name=text(args.at(0));assert(name=="CUST_ID.DAT"||name=="NWUSAGE.CFG"||name=="LOSEPCKT.NFO");assert(text(args.at(1))=="rt");return 0;} // Declared absent optional files.
 case 0x1100700:assert(args.at(0)==0);return U(std::time(nullptr));case 0x1100800:return 0;case 0x1100900:return U(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 case 0x1100a00:assert(args.size()==3&&args[1]==0&&args[2]==0);start(args[0]);return 1;
 case 0x1100b00:throw std::runtime_error("Required random provider reached");
 case 0x1100c00:{assert(args.size()==1&&args[0]<=1000);std::this_thread::sleep_for(std::chrono::milliseconds(args[0]));std::unique_lock<std::mutex>lock(gate);if(!errors.empty())std::rethrow_exception(errors.begin()->second);completed=received;changed.notify_all();if(args[0]==5&&host_park){host_parked=true;changed.notify_all();changed.wait(lock,[&]{return host_release;});}return 0;}
 case 0x1100d00:{assert(text(args.at(1))=="%08x:%04x");char output[80];const int n=std::snprintf(output,sizeof(output),"%08x:%04x",args[2],args[3]);text(args[0],output);return U(n);}
 case 0x1100e00:return 0; // Controlled test delegate returns; production ends its OS thread.
 case 0x1100f00:{std::lock_guard<std::mutex>lock(gate);++enum_seen;changed.notify_all();return 0;}
 case 0x10005934:{std::lock_guard<std::mutex>lock(gate);put(args.at(2),incoming.empty()?0:U(incoming.front().wire.size()));return 0;}
 case 0x1000595e:{std::lock_guard<std::mutex>lock(gate);assert(!incoming.empty()&&args[2]>=incoming.front().wire.size());auto packet=std::move(incoming.front());incoming.pop_front();++received;std::copy(packet.wire.begin(),packet.wire.end(),ptr(args[1],packet.wire.size()));put(args[4],2,2);put(args[4]+2,packet.port,2);put(args[4]+4,packet.ip);put(args[5],16);return U(packet.wire.size());}
 case 0x10005916:return 10065; // Captured send to an absent endpoint reports no route.
 case 0x1000592e:return((args.at(0)&255)<<8)|((args[0]>>8)&255);
 case 0x10005952:{Bytes wire;U to_ip,to_port;{std::lock_guard<std::recursive_mutex>lock(memory_guard);wire.assign(ptr(args[1],args[2]),ptr(args[1],args[2])+args[2]);to_ip=get(args[4]+4);to_port=get(args[4]+2,2);}const auto decoded=decode_received_datagram(wire,0x10000,{ip,std::uint16_t(port)});assert(decoded.status==ReceiveStatus::Ok);const auto&body=decoded.payload;assert(body.size()>=2);const U op=U(body[0])|(U(body[1])<<8);{std::lock_guard<std::mutex>lock(gate);sent.push_back(op);}changed.notify_all();if(other&&to_ip==other->ip&&to_port==(((other->port&255)<<8)|((other->port>>8)&255))){std::lock_guard<std::mutex>lock(other->gate);other->incoming.push_back({std::move(wire),ip,((port&255)<<8)|((port>>8)&255)});other->changed.notify_all();return args[2];}return 0xffffffffu;}
 case 0x100046c0:return run(s,12,args,[&](U f){mission_netsock_dispatch(b,f);return 0u;});case 0x10003f00:case 0x100040c0:return run(s,16,args,[&](U f){return mission_netsock_admission_call(b,s,f);});
 case 0x10004550:case 0x10004660:return run(s,16,args,[&](U f){return mission_netsock_destroy_player_call(b,s,f);});
 case 0x10004340:return run(s,16,args,[&](U f){return mission_netsock_create_player_call(b,s,f);});
 case 0x100048c0:return run(s,12,args,[&](U f){mission_netsock_outgoing(b,f);return 0u;});case 0x100028c0:mission_netsock_usage_tick(b);return 0;case 0x10004830:mission_netsock_periodic(b);return 0;case 0x100020d0:return run(s,16,args,[&](U f){mission_netsock_ping_timer(b,f);return 0u;});case 0x10002d90:mission_netsock_usage_free(b);return 0;case 0x10005020:return run(s,0x30,args,[&](U f){return mission_netsock_encode_direct(b,f);});
 default:throw std::runtime_error("Required paired peer service is not bound "+std::to_string(s));}}
};
}
int main(int argc,char**argv){using namespace dfre;Bytes raw;if(argc==2){std::ifstream f(argv[1],std::ios::binary);raw.assign(std::istreambuf_iterator<char>(f),{});}Peer host(raw,0x0100007f,0x3412),client(raw,0x0200007f,0x4523);host.other=&client;client.other=&host;
 const U host_config=host.config(),host_player=host.player("Host"),host_out=host.allocate(4);
 assert(host.run(0x10001320,8,{host_config},[&](U f){return mission_netsock_host_session(host.b,f);})==0);
 assert(host.run(0x10001780,16,{host_out,host_player,1},[&](U f){return mission_netsock_create_player_call(host.b,0x10001780,f);})==0);
 const U host_id=host.get(host_out),host_record=host.get(0x10019b00);assert(host_id==0x10000&&host_record==host.get(0x10019afc)&&host.get(host_record+0xa0)==host_id&&host.text(host_record)=="Host");
 {std::lock_guard<std::mutex>lock(host.gate);host.allow_host=true;}host.changed.notify_all();
 client.put(0x100198a8,host.ip);client.put(0x10019868,host.port,2);client.put(0x10019c70,host.port,2);const U client_config=client.config();for(U i=0;i<4;++i)client.put(0x100199cc+i*4,client.get(client_config+0xe0+i*4));
 assert(client.run(0x100018d0,0x18,{client_config,0,0x1100f00,99,0},[&](U f){return mission_netsock_close_call(client.b,0x100018d0,f);})==0);
 {std::lock_guard<std::mutex>lock(client.gate);client.allow_enum=true;}client.changed.notify_all();client.wait([&]{return client.enum_seen>0;});
 const U row=client.get(0x100199dc);assert(client.get(row+0x106)&&client.get(row)==host.ip&&client.get(row+4,2)==host.port);
 const U selected=client.allocate(0x20),password=client.allocate(3),df_frame=client.allocate(0x118);for(U i=0;i<4;++i)client.put(selected+0x10+i*4,client.get(row+0xd6+i*4));client.text(password,"pw");client.put(df_frame+0x110,selected);client.put(df_frame+0x114,password);client.put(0x501320,1);
 MissionGameTransportBindings game;game.read=client.b.read;game.write=client.b.write;game.call=[&](U s,const Args&args)->U{if(s==0x4c5544)return mission_netsock_close_call(client.b,0x10001990);if(s==0x49a620){for(U i=0;i<args.at(2);++i)client.put(args[0]+i,args[1]&255,1);return args[0];}if(s==0x4c553e)return client.run(0x10001420,8,args,[&](U f){return mission_netsock_join_call(client.b,0x10001420,f);});throw std::runtime_error("Required DF Join import is not bound");};
 assert(mission_df_join(game,df_frame)==1);client.join(0x10003680);assert(client.get(0x100199b8)==0);
 {std::lock_guard<std::mutex>lock(client.gate);client.allow_join=true;}client.changed.notify_all();client.wait([&]{return client.join_done;});client.join(0x10003960);assert(mission_df_join(game,df_frame)==0&&client.get(0x100199b0)==2&&client.get(0x100198ac)==1);assert(client.get(0x10019b38)==host.ip&&client.get(0x100199b4)==host.ip);
 const U client_player=client.player("Client"),client_out=client.allocate(4);assert(client.run(0x10001780,16,{client_out,client_player,0},[&](U f){return mission_netsock_create_player_call(client.b,0x10001780,f);})==0);
 const U client_id=client.get(client_out),remote=host.get(0x10019afc)+0xbc;assert(client_id==0x10001&&host.get(remote+0xa0)==client_id&&host.get(remote+0x98)==1&&host.text(remote)=="Client");for(U i=0;i<7;++i)assert(host.get(host.get(remote+0x80)+i,1)==0x80+i);assert(client.get(0x10019b50)==1&&client.get(0x10019b54)==0);
 // The current DF callback is explicit. Full43F830 is a separate integration boundary.
 U callback_count=0,remove_count=0;const U df_name=host.allocate(0x88),df_packet=host.allocate(0x1000),df_len=host.allocate(4),df_sender=host.allocate(4);host.put(0xd0a540,df_packet);host.put(0x501320,1);host.put(0x501374,0x1101000);host.put(0x501348,0);host.put(0x50134c,0);host.put(0x4b93f0,0x4b9432);host.put(0x4b93f4,0x4b94a2);host.put(0x501368,1);host.put(0x501378,0x1101001);
 MissionGameTransportBindings hg;hg.read=host.b.read;hg.write=host.b.write;hg.frame=[&](U s,U n){return host.b.frame(s,n,{});};hg.release=host.b.release;hg.call=[&](U s,const Args&args)->U{if(s==0x4c5520){const U result=host.run(0x10001aa0,12,args,[&](U f){return mission_netsock_player_name(host.b,f);});assert(result==0&&args[0]==client_id&&host.text(args[1])=="Client");return result;}if(s==0x1101001){assert(args.empty()&&host.get(0x501344)==client_id);++remove_count;return 0;}if(s==0x1101000){assert(args.empty()&&host.get(0x501344)==client_id&&host.get(0x501370)==7);for(U i=0;i<7;++i)assert(host.get(host.get(0x50136c)+i,1)==0x80+i);++callback_count;return 0;}throw std::runtime_error("Required DF callback is not bound");};
 host.wait([&]{return host.get(0x10019b18)>0;});host.put(df_len,0x1000);assert(host.run(0x10001c80,20,{df_packet,df_len,df_sender,0},[&](U f){return mission_netsock_get_message(host.b,f);})==0);assert(host.get(df_packet,2)==0x1050&&host.get(df_packet+4)==client_id);host.run(0x4b9400,0x9c,{df_packet},[&](U f){for(U i=0;i<0x88;i+=4)host.put(f+i,host.get(df_name+i));host.put(f+0x98,df_packet);mission_df_system_receive(hg,f);return 0u;});assert(callback_count==1&&host.get(0x501348)==1);
 assert(client.run(0x10001b80,8,{client_id+1u},[&](U f){return mission_netsock_destroy_player_call(client.b,0x10001b80,f);})==0);assert(client.get(0x10019bfc)==1&&client.get(0x10019c08)==1&&host.get(remote+0x9c)==1&&host.get(0x100198bc)==2);
 const U old_userdata=host.get(remote+0x80);assert(client.run(0x10001b80,8,{client_id},[&](U f){return mission_netsock_destroy_player_call(client.b,0x10001b80,f);})==0);assert(client.get(0x10019bfc)==1&&client.get(0x10019c08)==0&&client.get(0x10019bf8)==0&&host.get(remote+0x9c)==0&&host.get(0x100198bc)==1);{std::lock_guard<std::recursive_mutex>lock(host.memory_guard);assert(!host.blocks.count(old_userdata));}
 host.wait([&]{return host.get(0x10019b18)>0;});host.put(df_len,0x1000);assert(host.run(0x10001c80,16,{df_packet,df_len,df_sender},[&](U f){return mission_netsock_get_message(host.b,f);})==0);assert(host.get(df_packet,2)==0x1051&&host.get(df_packet+4)==client_id);host.run(0x4b9400,0x9c,{},[&](U f){host.put(f+0x98,df_packet);mission_df_system_receive(hg,f);return 0u;});assert(remove_count==1&&host.get(0x501348)==0);
 assert(client.run(0x10001780,16,{client_out,client_player,0},[&](U f){return mission_netsock_create_player_call(client.b,0x10001780,f);})==0);assert(client.get(client_out)==client_id&&host.get(remote+0x9c)==1&&host.get(0x100198bc)==2);const U new_userdata=host.get(remote+0x80);assert(new_userdata!=old_userdata);for(U i=0;i<7;++i)assert(host.get(new_userdata+i,1)==0x80+i);
 host.wait([&]{return host.get(0x10019b18)>0;});host.put(df_len,0x1000);assert(host.run(0x10001c80,16,{df_packet,df_len,df_sender},[&](U f){return mission_netsock_get_message(host.b,f);})==0);assert(host.get(df_packet,2)==0x1050);host.run(0x4b9400,0x9c,{},[&](U f){host.put(f+0x98,df_packet);mission_df_system_receive(hg,f);return 0u;});assert(callback_count==2&&host.get(0x501348)==1);
 const U payload=client.allocate(7);for(U i=0;i<7;++i)client.put(payload+i,i+1,1);assert(client.run(0x10001dc0,12,{payload,7},[&](U f){return mission_netsock_send_to_server(client.b,f);})==0);
 host.wait([&]{return host.get(0x10019b18)>0;});host.wait([&]{return host.get(0x10019b18)>0;});host.put(df_len,0x1000);assert(host.run(0x10001c80,20,{df_packet,df_len,df_sender,0},[&](U f){return mission_netsock_get_message(host.b,f);})==0);assert(host.get(df_sender)==client_id&&host.get(df_len)==7);for(U i=0;i<7;++i)assert(host.get(df_packet+i,1)==i+1);
 const U reply=host.allocate(5),client_data=client.allocate(0x100),client_len=client.allocate(4),client_sender=client.allocate(4);for(U i=0;i<5;++i)host.put(reply+i,0x40+i,1);assert(host.run(0x10001cf0,16,{reply,5,client_id},[&](U f){return mission_netsock_send_to_client(host.b,f);})==0);client.wait([&]{return client.get(0x10019c30)>0;});client.put(client_len,0x100);assert(client.run(0x10001c80,16,{client_data,client_len,client_sender},[&](U f){return mission_netsock_get_message(client.b,f);})==0);assert(client.get(client_sender)==host_id&&client.get(client_len)==5);for(U i=0;i<5;++i)assert(client.get(client_data+i,1)==0x40+i);
 const U remote_address=host.get(remote+0xa4);mission_netsock_client_close(client.b);client.join(0x10003300);assert(client.get(0x10019c14)==1&&client.get(0x10019c1c)==0&&host.get(remote+0x9c)==0&&host.get(remote_address)==0&&host.get(0x100198bc)==1);{std::lock_guard<std::recursive_mutex>lock(host.memory_guard);assert(!host.blocks.count(new_userdata));}
 host.wait([&]{return host.get(0x10019b18)>0;});host.put(df_len,0x1000);assert(host.run(0x10001c80,16,{df_packet,df_len,df_sender},[&](U f){return mission_netsock_get_message(host.b,f);})==0);assert(host.get(df_packet,2)==0x1051&&host.get(df_packet+4)==client_id);host.run(0x4b9400,0x9c,{},[&](U f){host.put(f+0x98,df_packet);mission_df_system_receive(hg,f);return 0u;});assert(remove_count==2&&host.get(0x501348)==0);
 {std::lock_guard<std::mutex>lock(host.gate);host.host_park=true;}host.wait([&]{return host.host_parked;});assert(mission_netsock_close_session(host.b)==0);{std::lock_guard<std::mutex>lock(host.gate);host.host_release=true;}host.changed.notify_all();host.join(0x10004930);assert(host.get(0x10019b08)==1);
 for(auto*peer:{&host,&client}){assert(peer->threads.empty());for(const auto&pair:peer->frames)assert(pair.second.empty());}
}
