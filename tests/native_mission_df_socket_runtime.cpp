#include "dfre/mission_df_socket_runtime.hpp"
#include <cassert>
#include <map>
#include <thread>
using namespace dfre;
namespace {
using U=std::uint32_t;
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const MissionClientStartupError&){caught=true;}assert(caught);}
struct Memory {
 std::map<U,std::uint8_t> bytes;MissionDfSocketRuntimeMemory bindings;
 Memory(){bindings.read=[this](U at,U width){assert(valid(at,width));U value=0;for(U i=0;i<width;++i)value|=U(bytes[at+i])<<(8*i);return value;};bindings.write=[this](U at,U width,U value){assert(valid(at,width));for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(8*i));};}
 static bool valid(U at,U width){return (at>=0x8e0000&&std::uint64_t(at)+width<=0x8e4000)||(at>=0x1200000&&std::uint64_t(at)+width<=0x1201000);}
 void text(U at,const char* s){do{bindings.write(at++,1,U(std::uint8_t(*s)));}while(*s++);}
};
void startup(const MissionDfSocketBindings& b){assert(b.call(0x4c55f8,{0x101,0x8e1390})==0);}
U make_socket(const MissionDfSocketBindings& b){const auto value=b.call(0x4c55c8,{2,2,0});assert(value!=0xffffffffu);return value;}
U bind_port(MissionDfSocketRuntime& r,const MissionDfSocketBindings& b,U token,U port){r.write(0x1200000,2,2);r.write(0x1200002,2,b.call(0x4c55bc,{port}));r.write(0x1200004,4,0x0100007f);r.write(0x1200008,4,0);r.write(0x120000c,4,0);return b.call(0x4c55b6,{token,0x1200000,16});}
}
int main(){
 {Memory m;MissionDfSocketRuntimeOptions options;options.allocation_limit=48;options.new_memory_byte=0xa5;MissionDfSocketRuntime r(m.bindings,options);auto b=r.bindings();
  assert(b.call(0x49dc80,{0})==0&&b.call(0x49dc80,{0xffffffd5})==0);const auto first=b.call(0x49dc80,{1}),second=b.call(0x49dc80,{17});assert(first==0x24000000&&second==first+16);assert(r.read(first,4)==0xa5a5a5a5);r.write(first+14,4,0x12345678);assert(r.read(second,2)==0x1234&&r.read(first+14,4)==0x12345678);assert(b.call(0x49dc80,{1})==0);assert(r.claims(first+47,1));rejects([&]{r.read(first+40,1);});
  r.write(0x8e3544,4,123);assert(m.bindings.read(0x8e3544,4)==123);m.bindings.write(0x8e3544,4,456);assert(r.read(0x8e3544,4)==456);assert(!r.claims(0x8e3544,4));rejects([&]{r.read(first,3);});
  const auto frame=b.frame(0x454a10,16,{});assert(r.read(frame,4)==0xa5a5a5a5);r.write(frame+8,4,0x11121314);const auto nested=b.frame(0x454890,4,{65536});assert(nested==frame+0x100&&r.read(nested,4)==65536);b.release(0x454890,nested,4);b.release(0x454a10,frame,16);assert(r.frame_depth()==0);const auto reused=b.frame(0x454930,20,{});assert(reused==frame&&r.read(reused+8,4)==0x11121314);b.release(0x454930,reused,20);rejects([&]{b.frame(0x454930,16,{});});assert(r.frame_depth()==0);rejects([&]{r.read(frame+40,1);});
  bool thread_rejected=false;std::thread other([&]{try{r.read(first,1);}catch(const MissionClientStartupError&){thread_rejected=true;}});other.join();assert(thread_rejected);
 }
 {Memory m;MissionDfSocketRuntime r(m.bindings);auto b=r.bindings();rejects([&]{make_socket(b);});startup(b);assert(r.claims(0x8e1390,4)&&r.read(0x8e1390,2)==0x101);m.text(0x1200100,"127.0.0.1");assert(mission_df_socket_resolve(b,0x1200100)==0x0100007f);const auto host=b.call(0x4c55ec,{0x1200100});assert(r.read(host+8,2)==2&&r.read(host+10,2)==4);const auto list=r.read(host+12,4);assert(r.read(r.read(list,4),4)==0x0100007f&&r.read(list+4,4)==0);m.text(0x1200100,"192.0.2.9");assert(b.call(0x4c55ec,{0x1200100})==host);assert(r.read(r.read(r.read(host+12,4),4),4)==0x090200c0);m.text(0x1200100,"localhost");rejects([&]{mission_df_socket_resolve(b,0x1200100);});
  const auto first=make_socket(b),second=make_socket(b);assert(bind_port(r,b,first,0)==0);const auto port=r.local_port(first);assert(port!=0&&bind_port(r,b,second,port)==0xffffffffu);assert(b.call(0x4c55e0,{})==10048);assert(b.call(0x4c55bc,{1234})==0xd204&&b.call(0x4c55e0,{})==10048);
  r.write(0x1200020,4,1);assert(b.call(0x4c55c2,{second,0x8004667e,0x1200020})==0);assert(b.call(0x4c55c2,{0xdead,0x8004667e,0x1200020})==0xffffffffu&&b.call(0x4c55e0,{})==10038);
  r.write(0x8e353c,4,first);assert(mission_df_socket_set_buffer(b,false,65536)==0);const auto size=mission_df_socket_get_buffer(b,false);assert(size>0&&r.frame_depth()==0);rejects([&]{b.call(0x4c55ce,{first,0xffff,0x1234,0x1200020,4});});rejects([&]{b.call(0x4c55c8,{2,1,0});});rejects([&]{b.call(0xdead,{});});
  // The source send body binds ANY. A loopback reservation must still force a retry.
  assert(mission_df_socket_open_send(b,U(port)-1)==0);assert(r.local_port(r.read(0x8e353c,4))!=port&&r.frame_depth()==0);assert(r.socket_count()==3);
 }
 {Memory m;MissionDfSocketRuntime r(m.bindings);auto b=r.bindings();startup(b);const auto occupied=make_socket(b);assert(bind_port(r,b,occupied,0)==0);const auto initial=r.local_port(occupied);r.write(0x1200040,2,initial);assert(mission_df_socket_start(b,2,0x1200040,0xf28)==0);const auto receive=r.read(0x8e3548,4),send=r.read(0x8e353c,4),chosen=r.read(0x1200040,2);assert(receive!=send&&chosen!=initial&&r.local_port(receive)==chosen);assert(r.read(0x8e354e,2)==chosen&&r.read(0x8e354c,2)==0xf28&&r.read(0x8e3530,4)==2);const auto work=r.read(0x8e3540,4),size=r.read(0x8e3544,4);assert(work&&size>12&&r.claims(work,size>3?4:1)&&r.frame_depth()==0);r.write(work+size-4,4,0xfedcba98);assert(r.read(work+size-4,4)==0xfedcba98&&r.read(0x8e352c,4)==0);
 }
 {Memory m;U released_port=0;{MissionDfSocketRuntime r(m.bindings);auto b=r.bindings();startup(b);const auto token=make_socket(b);assert(bind_port(r,b,token,0)==0);released_port=r.local_port(token);}MissionDfSocketRuntime r(m.bindings);auto b=r.bindings();startup(b);assert(bind_port(r,b,make_socket(b),released_port)==0);}
 {Memory m;MissionDfSocketRuntimeOptions options;options.allocation_limit=0;MissionDfSocketRuntime r(m.bindings,options);auto b=r.bindings();startup(b);const auto token=make_socket(b);assert(bind_port(r,b,token,0)==0);const auto p=r.local_port(token);r.write(0x1200040,2,p);assert(mission_df_socket_start(b,2,0x1200040,0)==0x271e);assert(r.read(0x8e3540,4)==0&&r.read(0x8e3544,4)>12&&r.frame_depth()==0);}
 {Memory m;MissionDfSocketRuntime r(m.bindings);auto b=r.bindings();startup(b);const auto original=b.call;b.call=[&](auto site,const auto& args)->U{if(site==0x4c55bc)throw MissionClientStartupError("port test error");return original(site,args);};rejects([&]{mission_df_socket_open_send(b,0);});assert(r.frame_depth()==0&&r.socket_count()==1&&r.read(0x8e353c,4)!=0);}
}
