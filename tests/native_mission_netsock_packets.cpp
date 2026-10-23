#include "dfre/mission_netsock_packets.hpp"
#include "dfre/mission_game_transport.hpp"
#include "dfre/mission_peer_frame.hpp"
#include "dfre/game_messages.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>

namespace {using U=std::uint32_t;
struct Memory {
 std::map<U,dfre::AssetBytes> blocks;
 std::uint8_t* resolve(U at,std::size_t count){auto p=blocks.upper_bound(at);if(p==blocks.begin())throw std::runtime_error("Memory is not mapped");--p;if(std::uint64_t(at-p->first)+count>p->second.size())throw std::runtime_error("Memory exceeds its owner");return p->second.data()+at-p->first;}
 U read(U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(*resolve(at+i,1))<<(i*8);return value;}
 void write(U at,U n,U value){for(U i=0;i<n;++i)*resolve(at+i,1)=std::uint8_t(value>>(i*8));}
};
template<class F>bool fails(F f){try{f();}catch(const std::exception&){return true;}return false;}
}
int main(int argc,char** argv){using namespace dfre;Memory memory;for(auto [at,size]:std::array<std::pair<U,U>,6>{{{0x10000000,0x1b000},{0x1200000,0x10000},{0x1210000,0x10000},{0x1300000,0x10000},{0xd0a500,0x100},{0x501300,0x1000}}})memory.blocks[at].resize(size);
 MissionNetsockPacketMemoryBindings host{[&](U at,U n){return memory.read(at,n);},[&](U at,U n,U value){memory.write(at,n,value);},[&](U at,std::size_t n){return memory.resolve(at,n);}};
 assert(fails([&]{MissionNetsockPacketMemory invalid({},host);}));std::unique_ptr<MissionNetsockPacketMemory> image;
 if(argc==2){std::ifstream input(std::filesystem::path(argv[1])/"Netsocknl.dll",std::ios::binary);AssetBytes raw((std::istreambuf_iterator<char>(input)),{});image=std::make_unique<MissionNetsockPacketMemory>(raw,host);auto wrong=raw;wrong.back()^=1;assert(fails([&]{MissionNetsockPacketMemory invalid(wrong,host);}));assert(image->claims(0x10019880,4)&&!image->claims(0x1210000,4));assert(image->read(0x10019880,4)==0);assert(fails([&]{image->claims(0x1001afff,2);}));assert(fails([&]{image->resolve(0x10000000,0);}));image->write(0x10019880,4,0x12345678);assert(image->read(0x10019881,2)==0x3456);auto* shared=image->resolve(0x10019880,4);shared[0]=0xa5;assert(image->read(0x10019880,1)==0xa5);assert(image->resolve(0x1210000,9)==memory.resolve(0x1210000,9));}
 auto read=[&](U at,U n){return image?image->read(at,n):host.read(at,n);};auto write=[&](U at,U n,U value){if(image)image->write(at,n,value);else host.write(at,n,value);};auto put=[&](U at,U value){write(at,4,value);};auto get=[&](U at){return read(at,4);};
 U depth=0,frames=0,releases=0;MissionNetsockPacketBindings b;b.read=read;b.write=write;
 b.frame=[&](U site,U size,const std::vector<U>& args){const U at=0x1300000+depth++*0x100;std::fill_n(memory.resolve(at,size),size,0x99);U off=site==0x10001be0||site==0x10001c30?8:4;for(auto n:args){memory.write(at+off,4,n);off+=4;}++frames;return at;};b.release=[&](U,U,U){assert(depth);--depth;++releases;};
 for(U at:{0x10019880u,0x10019884u,0x100198acu,0x100199b0u})put(at,1);put(0x10006010,0x1100100);put(0x10006068,0x1100200);
 const U queue=0x10019b0c;auto seed=[&](U length){put(queue,0x1200000);put(queue+4,0x120fff0);put(queue+8,0x1200030);put(queue+0xc,1);put(queue+0x10,1);put(0x120fff0,0x1210000);put(0x120fff4,length);put(0x120fff8,9);put(0x120fffc,1234);for(U i=0;i<length;++i)write(0x1210000+i,1,0x40+i);};seed(7);
 U sleeps=0,frees=0;b.call=[&](U site,const auto& args)->U{if(site==0x1100100){assert(args==std::vector<U>({0}));++sleeps;put(queue+0x10,0);return 0;}if(site==0x1100200){assert(args==std::vector<U>({0x1210000}));++frees;return 0;}throw std::runtime_error("Required DLL body is not bound");};
 // Real DF bridge -> DLL GetMessage -> raw queue pop publishes one shared buffer.
 put(0x501320,1);put(0x501398,0x501ab8);put(0x501ab8+0x54,0x4b9530);put(0xd0a540,0x1211000);
 MissionGameTransportBindings bridge;bridge.read=read;bridge.write=write;bridge.frame=[&](U site,U size){assert(site==0x4b9530&&size==4);return b.frame(site,size,{});};bridge.release=b.release;bridge.call=[&](U site,const auto& args)->U{assert(site==0x4c551a);const auto at=b.frame(0x10001c80,0x10,args);try{const auto result=mission_netsock_get_message(b,at);b.release(0x10001c80,at,0x10);return result;}catch(...){b.release(0x10001c80,at,0x10);throw;}};
 assert(mission_game_transport_receive(bridge)==0x1211000&&get(0x50135c)==0x1211000&&get(0x501360)==7&&get(0x501344)==9&&get(queue+4)==0x1200000&&get(queue+0xc)==0&&get(queue+0x10)==0&&sleeps==1&&frees==1);for(U i=0;i<7;++i)assert(read(0x1211000+i,1)==0x40+i);assert(memory.resolve(get(0x50135c),7)==memory.resolve(0x1211000,7));
 // Queue pop ignores the incoming capacity; the canonical destination bounds apply.
 seed(7);put(queue+0x10,0);const U raw=0x1308000;for(auto [off,value]:std::array<std::pair<U,U>,5>{{{4,0x1211000},{8,0x1212000},{0xc,queue},{0x10,0x1212004},{0x14,0x1212008}}})put(raw+off,value);put(0x1212000,1);assert(mission_netsock_queue_pop(b,raw)==1&&get(0x1212000)==7&&get(0x1212004)==9&&get(0x1212008)==1234);
 seed(7);put(queue+0x10,0);put(raw+0xc,queue);auto missing=b;missing.call={};assert(fails([&]{mission_netsock_queue_pop(missing,raw);}));assert(get(queue+0x10)==1&&get(0x120fff0)==0x1210000);put(queue+0x10,0);
 // Exact host player name reaches one canonical DF D0A500 string buffer.
 put(0x100198b8,1);put(0x10019afc,0x1214000);put(0x121409c,1);const char name[]="ALPHA";for(U i=0;i<sizeof(name);++i)write(0x1214000+i,1,U(name[i]));const U df_frame=0x1309000;put(df_frame+0x94,0x10000);
 auto name_binding=b;name_binding.call=[&](U site,const auto& args)->U{assert(site==0x4c5520);const auto at=b.frame(0x10001aa0,0xc,args);try{const auto result=mission_netsock_player_name(name_binding,at);b.release(0x10001aa0,at,0xc);return result;}catch(...){b.release(0x10001aa0,at,0xc);throw;}};
 assert(mission_df_netsock_player_name(name_binding,df_frame)==0xd0a500);for(U i=0;i<sizeof(name);++i)assert(read(0xd0a500+i,1)==U(name[i]));assert((image?image->resolve(0xd0a500,sizeof(name)):host.resolve(0xd0a500,sizeof(name)))==memory.resolve(0xd0a500,sizeof(name)));
 put(0x100199b0,2);put(raw+4,0x10000);put(raw+8,0x1212000);put(0x1212000,0xa5a5a5a5);assert(mission_netsock_player_name(b,raw)==0&&get(0x1212000)==0xa5a5a5a5);
 assert(depth==0&&frames==releases);
}
