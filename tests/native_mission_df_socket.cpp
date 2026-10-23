#include "dfre/mission_df_socket.hpp"
#include <cassert>
#include <map>
using namespace dfre;
namespace {using U=std::uint32_t;struct Fixture {
 std::map<U,std::uint8_t> bytes;MissionDfSocketBindings b;unsigned depth=0,released=0,attempts=0,fail_binds=0;U error=0x2740,wsa=0,socket=77,send=65536,receive=32768,allocation=0x1400000,direct=0x0100007f,host=0;std::vector<U> ports,bind_sockets;
 void put(U a,U width,U value){for(U i=0;i<width;++i)bytes[a+i]=std::uint8_t(value>>(i*8));}U get(U a,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes[a+i])<<(8*i);return n;}
 Fixture(){b.read=[this](auto a,auto w){return get(a,w);};b.write=[this](auto a,auto w,auto n){put(a,w,n);};b.frame=[this](auto,auto size,const auto& arguments){const auto at=0x1300000u+depth++*0x100;for(U i=0;i<size;++i)put(at+i,1,0xa5);if(!arguments.empty())put(at,4,arguments.at(0));return at;};b.release=[this](auto,auto,auto){--depth;++released;};b.call=[this](auto site,const auto& args)->U{
  if(site==0x4c55f8){assert((args==std::vector<U>{0x101,0x8e1390}));return wsa;}if(site==0x4c55c8){assert((args==std::vector<U>{2,2,0}));return socket;}if(site==0x4c55c2){assert(args.at(1)==0x8004667e&&get(args.at(2))==1);return 0;}if(site==0x4c55bc)return ((args.at(0)&255)<<8)|((args.at(0)>>8)&255);
  if(site==0x4c55b6){assert(args.at(2)==16&&get(args.at(1),2)==2&&get(args.at(1)+4)==0);for(U i=8;i<16;++i)assert(get(args.at(1)+i,1)==0xa5);const auto n=get(args.at(1)+2,2);ports.push_back(((n&255)<<8)|(n>>8));bind_sockets.push_back(args.at(0));return attempts++<fail_binds?0xffffffffu:0;}
  if(site==0x4c55e0)return error;if(site==0x4c55ce){assert(get(args.at(3))==65536);return 0;}if(site==0x4c55d4){assert(get(args.at(4))==4);put(args.at(3),4,args.at(2)==0x1001?send:receive);return 0;}if(site==0x49dc80){assert(args.at(0)==get(0x8e3544));return allocation;}if(site==0x4c55b0)return direct;if(site==0x4c55ec)return host;throw MissionClientStartupError("Unknown OS fixture");};}
};}
int main(){
 {Fixture f;assert(mission_df_socket_start(f.b,2,0,0xf28)==0);assert(f.get(0x8e3530)==2&&f.get(0x8e354c,2)==0xf28&&f.get(0x8e354e,2)==0&&f.get(0x8e3544)==65548&&f.get(0x8e3540)==0x1400000&&f.get(0x8e352c)==0);assert(f.depth==0&&f.released==3);}
 {Fixture f;f.put(0x8e3530,4,99);f.wsa=1;f.error=5;assert(mission_df_socket_start(f.b,2,0,0)==5&&f.get(0x8e3530)==99&&f.released==0);}
 {Fixture f;f.fail_binds=3;f.b.before_call=[&](auto site,const auto&){if(site==0x4c55bc)f.put(0x8e353c,4,123);};assert(mission_df_socket_open_send(f.b,0xfffe)==0);assert((f.ports==std::vector<U>{0xffff,0,1,2}));for(auto n:f.bind_sockets)assert(n==123);assert(f.depth==0&&f.released==1);}
 {Fixture f;f.fail_binds=2;f.put(0x1200000,2,0xf28);assert(mission_df_socket_open_receive(f.b,2,0x1200000)==0&&f.get(0x1200000,2)==0x2f28);assert((f.ports==std::vector<U>{0xf28,0x1f28,0x2f28}));}
 {Fixture f;f.fail_binds=1;f.put(0x1200000,2,0xf28);assert(mission_df_socket_open_receive(f.b,1,0x1200000)==1&&f.get(0x1200000,2)==0xf28&&f.ports.size()==1);}
 {Fixture f;f.send=0xffffffff;assert(mission_df_socket_buffers(f.b)==0&&f.get(0x8e3544)==12);f.send=0x7fffffff;f.allocation=0;assert(mission_df_socket_buffers(f.b)==0x271e&&f.get(0x8e3544)==0x8000000b&&f.get(0x8e3540)==0);}
 {Fixture f;f.b.before_call=[&](auto site,const auto&){if(site==0x454850)f.put(0x8e354e,2,1);};assert(mission_df_socket_buffers(f.b)==0&&f.released==5);}
 {Fixture f;f.direct=0xffffffff;assert(mission_df_socket_resolve(f.b,0x1200300)==0);f.host=0x1200000;f.put(f.host+0xc,4,0x1200100);f.put(0x1200100,4,0x1200200);f.put(0x1200200,4,0x0100007f);assert(mission_df_socket_resolve(f.b,0x1200300)==0x0100007f);}
 {Fixture f;const auto original=f.b.call;f.b.call=[&](auto site,const auto& args)->U{if(site==0x4c55bc)throw MissionClientStartupError("Port backend failed");return original(site,args);};bool failed=false;try{mission_df_socket_open_send(f.b,0);}catch(const MissionClientStartupError&){failed=true;}assert(failed&&f.get(0x8e353c)==77&&f.depth==0&&f.released==1);}
 {Fixture f;f.b.call={};bool failed=false;try{mission_df_socket_open_send(f.b,0);}catch(const MissionClientStartupError&){failed=true;}assert(failed&&f.depth==0&&f.released==1);failed=false;try{mission_df_socket_open_send({},0);}catch(const MissionClientStartupError&){failed=true;}assert(failed);}
}
