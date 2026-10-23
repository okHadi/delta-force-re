#include "dfre/mission_netsock_startup.hpp"
#include "dfre/mission_netsock_send_leaves.hpp"
#include "dfre/compression.hpp"
#include "dfre/network_send.hpp"
#include "dfre/network.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket=SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket=int;
#endif
namespace {using U=std::uint32_t;
template<class F>bool fails(F f){try{f();}catch(const std::exception&){return true;}return false;}
struct Memory{std::map<U,dfre::AssetBytes> blocks;U next=0x22000000;U allocated=0;std::uint8_t* pointer(U at,std::size_t n){auto i=blocks.upper_bound(at);if(i==blocks.begin())throw std::runtime_error("Startup memory is not mapped");--i;if(std::uint64_t(at-i->first)+n>i->second.size())throw std::runtime_error("Startup memory exceeds its owner");return i->second.data()+at-i->first;}U read(U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(*pointer(at+i,1))<<(8*i);return value;}void write(U at,U n,U value){for(U i=0;i<n;++i)*pointer(at+i,1)=std::uint8_t(value>>(8*i));}U allocate(U n){if(!n||n>0x100000)throw std::runtime_error("Startup allocation exceeds the host limit");const U at=next;next+=(n+0xfff)&~0xfffu;blocks[at].resize(n,0xa5);++allocated;return at;}};
struct Sockets{std::vector<Socket> owned;~Sockets(){for(auto s:owned){
#ifdef _WIN32
closesocket(s);
#else
close(s);
#endif
}}};
}
int main(int argc,char** argv){using namespace dfre;Memory memory;for(auto [at,n]:std::vector<std::pair<U,U>>{{0x10000000,0x1b000},{0x1200000,0x10000},{0x1300000,0x20000}})memory.blocks[at].resize(n,0x5a);MissionNetsockPacketMemoryBindings foreign{[&](U at,U n){return memory.read(at,n);},[&](U at,U n,U value){memory.write(at,n,value);},[&](U at,std::size_t n){return memory.pointer(at,n);}};std::unique_ptr<MissionNetsockPacketMemory> dll;
 if(argc==2){std::ifstream file(std::filesystem::path(argv[1])/"Netsocknl.dll",std::ios::binary);AssetBytes raw((std::istreambuf_iterator<char>(file)),{});dll=std::make_unique<MissionNetsockPacketMemory>(raw,foreign);std::ifstream codec(std::filesystem::path(argv[1])/"Cmprssh0.dll",std::ios::binary);AssetBytes codec_bytes((std::istreambuf_iterator<char>(codec)),{});assert(codec_bytes.size()>0x1000);for(const std::string symbol:{"init_compression","compress_buffer","expand_buffer"})assert(std::search(codec_bytes.begin(),codec_bytes.end(),symbol.begin(),symbol.end())!=codec_bytes.end());}
 const auto get=[&](U at,U n=4){return dll?dll->read(at,n):foreign.read(at,n);};const auto put=[&](U at,U value,U n=4){if(dll)dll->write(at,n,value);else foreign.write(at,n,value);};const auto pointer=[&](U at,std::size_t n){return dll?dll->resolve(at,n):foreign.resolve(at,n);};const auto text=[&](U at){std::string s;for(U i=0;i<260;++i){const auto c=get(at+i,1);if(!c)return s;s.push_back(char(c));}throw std::runtime_error("Startup string exceeds its owner");};
 if(!dll){for(auto [at,s]:std::vector<std::pair<U,std::string>>{{0x10007644,"255.255.255.255"},{0x10007654,"expand_buffer"},{0x10007664,"compress_buffer"},{0x10007674,"init_compression"},{0x10007688,"CMPRSS??.DLL"}}){std::copy(s.begin(),s.end(),pointer(at,s.size()+1));put(at+U(s.size()),0,1);}for(U i=0;i<256;++i){U v=i<<24;for(U k=0;k<8;++k)v=(v<<1)^((v&0x80000000u)?0x04c11db7u:0);put(0x10007234+i*4,v);}}
 Sockets sockets;std::map<U,Socket> socket_ids;U next_socket=1,serial=0,null_allocation=0,alloc_calls=0,export_calls=0,codec_calls=0,send_capacity=0,captured_socket=0;bool mutate_malloc=false,mutate_exports=false,find_failure=false,load_failure=false;AssetBytes wire;struct Frame{U site,at;};std::vector<Frame> frames;MissionNetsockStartupBindings b;b.read=get;b.write=[&](U at,U n,U value){put(at,value,n);};b.resolve=pointer;
 b.frame=[&](U site,U n,const auto& args){const U at=0x1300000+serial++*0x400;assert(at+n<0x1320000);std::fill_n(pointer(at,n),n,0x5a);frames.push_back({site,at});U offset=site==0x10004c80?0x20:site==0x10004b30?8:site==0x10005660?0x228:site==0x10005020?0x24:site==0x10005120?0x30:4;for(U value:args){put(at+offset,value);offset+=4;}return at;};b.release=[&](U site,U at,U){assert(!frames.empty()&&frames.back().site==site&&frames.back().at==at);frames.pop_back();};
 b.call=[&](U target,const auto& args)->U{
  if(target==0x1100100||target==0x1100b00){++alloc_calls;if(mutate_malloc){assert(target==0x1100100);put(0x1000605c,0x1100b00);}return alloc_calls==null_allocation?0:memory.allocate(args.at(0));}
  if(target==0x10005916){
#ifdef _WIN32
return U(WSAGetLastError());
#else
throw std::runtime_error("Loopback OS failure: "+std::to_string(errno)+" "+std::strerror(errno));
#endif
}
  if(target==0x10005922){assert(args[0]==0x101);
#ifdef _WIN32
   WSADATA data;const int result=WSAStartup(0x101,&data);if(result)return U(result);
#endif
   put(args[1],0x101,2);return 0;}
  if(target==0x1000593a){assert(args==std::vector<U>({2,2,0}));const Socket s=::socket(AF_INET,SOCK_DGRAM,0);assert(s!=Socket(-1));const U id=next_socket++;socket_ids[id]=s;sockets.owned.push_back(s);return id;}
  if(target==0x10005934){assert(args[1]==0x8004667e&&get(args[2])==1);const Socket s=socket_ids.at(args[0]);
#ifdef _WIN32
   u_long nonblock=1;return ioctlsocket(s,FIONBIO,&nonblock)==0?0:0xffffffffu;
#else
   const int flags=fcntl(s,F_GETFL,0);assert(flags!=-1);return fcntl(s,F_SETFL,flags|O_NONBLOCK)==0?0:0xffffffffu;
#endif
  }
  if(target==0x1000592e)return htons(std::uint16_t(args[0]));
  if(target==0x10005928){assert(args[2]==16&&get(args[1],2)==2&&get(args[1]+4)==0);sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=std::uint16_t(get(args[1]+2,2));address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);return ::bind(socket_ids.at(args[0]),reinterpret_cast<sockaddr*>(&address),sizeof(address))==0?0:0xffffffffu;}
  if(target==0x1000591c){assert(args[1]==0xffff&&args[4]==4);const int option=args[2]==0x1001?SO_SNDBUF:args[2]==0x1002?SO_RCVBUF:args[2]==0x20?SO_BROADCAST:0;assert(option);const int value=int(get(args[3]));return setsockopt(socket_ids.at(args[0]),SOL_SOCKET,option,reinterpret_cast<const char*>(&value),sizeof(value))==0?0:0xffffffffu;}
  if(target==0x10005910){assert(args[1]==0xffff&&get(args[4])==4);int value=0;
#ifdef _WIN32
   int n=sizeof(value);
#else
   socklen_t n=sizeof(value);
#endif
   const int result=getsockopt(socket_ids.at(args[0]),SOL_SOCKET,args[2]==0x1001?SO_SNDBUF:SO_RCVBUF,reinterpret_cast<char*>(&value),&n);if(result==0){put(args[3],U(value));put(args[4],U(n));if(args[2]==0x1001)send_capacity=U(value);}return result==0?0:0xffffffffu;}
  if(target==0x10005946){assert(text(args[0])=="127.0.0.1");return inet_addr("127.0.0.1");}
  if(target==0x1100200){assert(text(args[0])=="CMPRSS??.DLL");const std::string name="Cmprssh0.dll";std::copy(name.begin(),name.end(),pointer(args[1]+0x14,name.size()+1));put(args[1]+0x14+U(name.size()),0,1);return find_failure?0xffffffffu:7;}
  if(target==0x1100300){assert(args[0]==7||args[0]==0xffffffffu);return 1;}
  if(target==0x1100400){assert(text(args[0])=="Cmprssh0.dll");return load_failure?0:0x4455000;}
  if(target==0x1100500){++export_calls;if(mutate_exports&&export_calls==1){put(0x10006004,0x1100a00);put(0x100096b0,0x4455111);}if(mutate_exports)assert(args[0]==(export_calls==1?0x4455000:0x4455111));const auto name=text(args[1]);return name=="init_compression"?0x1100700:name=="compress_buffer"?0x1100800:name=="expand_buffer"?0x1100900:0;}
  if(target==0x1100700){++codec_calls;init_compression();return 0;}
  if(target==0x1100600)return 1;
  if(target==0x10005952){assert(args.size()==6&&args[3]==0&&args[5]==16);captured_socket=args[0];wire.assign(pointer(args[1],args[2]),pointer(args[1],args[2])+args[2]);return U(wire.size());}
  throw std::runtime_error("Startup provider is not bound: "+std::to_string(target));
 };
 const auto iat=[&]{for(auto [at,value]:std::vector<std::pair<U,U>>{{0x1000605c,0x1100100},{0x1000602c,0x1100200},{0x10006030,0x1100300},{0x10006008,0x1100400},{0x10006004,0x1100500},{0x10006000,0x1100600}})put(at,value);};iat();mutate_malloc=true;assert(mission_netsock_startup_init(b)==0&&alloc_calls==3&&get(0x10019880)==1&&get(0x10019c64)==0x10000&&get(0x10019c70,2)==0xf22&&get(0x10019c72,2)==0xf22);const U cleared=get(0x100199dc);assert(std::all_of(pointer(cleared,0x21c0),pointer(cleared,0x21c0)+0x21c0,[](auto n){return n==0;}));assert(get(get(0x10019c5c))==0xa5a5a5a5&&get(get(0x10019c60))==0xa5a5a5a5);mutate_malloc=false;iat();
 // The test backend binds only loopback and sends no OS packets. It substitutes
 // loopback for source INADDR_ANY at the explicit bind boundary.
 put(0x10019c72,0,2);put(0x10019c70,0x1234,2);put(0x10019c74,0);const std::string loop="127.0.0.1";std::copy(loop.begin(),loop.end(),pointer(0x1200000,loop.size()+1));put(0x1200000+U(loop.size()),0,1);put(0x130f004,0x1200000);mutate_exports=true;assert(mission_netsock_startup_tcpip(b,0x130f000)==0&&get(0x10019884)==1&&get(0x100198a8)==inet_addr("127.0.0.1")&&get(0x10019860)==1&&codec_calls==1&&frames.empty());assert(get(0x1001985c)==send_capacity+10&&get(0x10019854)>=0x22000000&&get(0x100096b8)==0&&get(0x10019868,2)==0x1234);const U produced_socket=get(0x100096b4),produced_buffer=get(0x10019854);assert(socket_ids.count(produced_socket));
 // The produced current socket, port and buffer feed the real raw send leaf.
 const AssetBytes payload{1,2,3,4,5};std::copy(payload.begin(),payload.end(),pointer(0x1201000,payload.size()));const U direct=b.frame(0x10005020,0x30,{0x1201000,U(payload.size()),0x0100007f});assert(mission_netsock_encode_direct(b,direct)==0&&wire==encode_direct_datagram(payload)&&captured_socket==produced_socket&&get(0x10019854)==produced_buffer);b.release(0x10005020,direct,0x30);
 auto missing=b;missing.call={};put(0x10019880,0);assert(mission_netsock_startup_tcpip(missing,0)==1);put(0x10019880,1);put(0x100199b8,1);assert(fails([&]{mission_netsock_startup_tcpip(missing,0x130f000);}));put(0x100199b8,0);assert(fails([&]{mission_netsock_startup_codec(missing,0x130f000);}));assert(get(0x100096a8)==0&&get(0x100096ac)==0);
 mutate_exports=false;iat();find_failure=true;put(0x10019860,9);const U codec=b.frame(0x10005660,0x22c,{0});assert(mission_netsock_startup_codec(b,codec)==0x67&&get(0x10019860)==9);b.release(0x10005660,codec,0x22c);find_failure=false;load_failure=true;const U codec2=b.frame(0x10005660,0x22c,{0});assert(mission_netsock_startup_codec(b,codec2)==0x68&&get(0x100096b0)==0&&get(0x10019860)==9);b.release(0x10005660,codec2,0x22c);
 iat();alloc_calls=0;null_allocation=3;assert(fails([&]{mission_netsock_startup_init(b);}));assert(get(0x100199dc)==0&&get(0x10019880)==1&&get(0x10019c64)==0x10000&&get(0x10019c70,2)==0&&frames.empty());
#ifdef _WIN32
 WSACleanup();
#endif
}
