#include "dfre/mission_netsock_queue.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
namespace {using U=std::uint32_t;
struct Backend {
 std::map<U,dfre::AssetBytes> blocks;U next=0x22000000,allocations=0,frees=0;
 std::uint8_t* pointer(U at,std::size_t size){auto p=blocks.upper_bound(at);if(p==blocks.begin())throw std::runtime_error("Unmapped memory");--p;if(std::uint64_t(at-p->first)+size>p->second.size())throw std::runtime_error("Memory exceeds its owner");return p->second.data()+at-p->first;}
 U read(U at,U width){U value=0;for(U i=0;i<width;++i)value|=U(*pointer(at+i,1))<<(8*i);return value;}
 void write(U at,U width,U value){for(U i=0;i<width;++i)*pointer(at+i,1)=std::uint8_t(value>>(8*i));}
 U allocate(U size){assert(size&&size<=0x10000);const U at=next;next+=(size+0xfff)&~0xfffu;blocks[at].resize(size,0xa5);++allocations;return at;}
 void release(U at){const auto n=blocks.erase(at);assert(n==1);++frees;}
};
template<class F>bool fails(F f){try{f();}catch(const std::exception&){return true;}return false;}
}
int main(int argc,char** argv){using namespace dfre;Backend memory;memory.blocks[0x10000000].resize(0x1b000);memory.blocks[0x1300000].resize(0x10000);memory.blocks[0x1210000].resize(0x10000);
 MissionNetsockPacketMemoryBindings foreign{[&](U at,U n){return memory.read(at,n);},[&](U at,U n,U value){memory.write(at,n,value);},[&](U at,std::size_t n){return memory.pointer(at,n);}};std::unique_ptr<MissionNetsockPacketMemory> dll;
 if(argc==2){std::ifstream input(std::filesystem::path(argv[1])/"Netsocknl.dll",std::ios::binary);AssetBytes raw((std::istreambuf_iterator<char>(input)),{});dll=std::make_unique<MissionNetsockPacketMemory>(raw,foreign);}
 auto read=[&](U at,U n){return dll?dll->read(at,n):foreign.read(at,n);};auto write=[&](U at,U n,U value){if(dll)dll->write(at,n,value);else foreign.write(at,n,value);};auto get=[&](U at){return read(at,4);};auto put=[&](U at,U value){write(at,4,value);};
 const U queue=0x10019b0c,frame=0x1300000;put(0x10006010,0x1100100);put(0x1000605c,0x1100200);put(0x10006068,0x1100300);put(frame+4,queue);
 MissionNetsockQueueBindings b;b.read=read;b.write=write;U sleeps=0;bool change_free=false;
 b.call=[&](U site,const auto& args)->U{if(site==0x1100200)return memory.allocate(args.at(0));if(site==0x1100300){memory.release(args.at(0));if(change_free)put(0x10006068,0x1100400);return 0;}if(site==0x1100100){++sleeps;put(0x10006010,0x1100400);if(sleeps==2)put(queue+0x10,0);return 0;}throw std::runtime_error("Required queue body is not bound");};
 assert(mission_netsock_queue_initialize(b,frame)==0);const auto ring=get(queue);assert(ring==0x22000000&&get(queue+4)==ring&&get(queue+8)==ring&&get(queue+0xc)==0&&get(queue+0x10)==0);for(U i=0;i<0x10000;++i)assert(read(ring+i,1)==0);
 const auto set_enqueue=[&](U length){put(frame+4,0x1210000);put(frame+8,length);put(frame+0xc,queue);put(frame+0x10,9);put(frame+0x14,1234);put(frame+0x18,0);};for(U i=0;i<7;++i)write(0x1210000+i,1,0x40+i);set_enqueue(7);put(queue+0x10,1);mission_netsock_queue_enqueue(b,frame);const auto payload=get(ring);assert(sleeps==2&&payload!=0&&get(queue+8)==ring+0x10&&get(queue+0xc)==1&&get(queue+0x10)==0);for(U i=0;i<7;++i)assert(read(payload+i,1)==0x40+i);
 // The actual GetMessage/queue-pop bodies consume the same canonical allocation.
 MissionNetsockPacketBindings receive;receive.read=read;receive.write=write;receive.call=b.call;U depth=0;receive.frame=[&](U site,U size,const auto& args){const U at=0x1301000+depth++*0x100;std::fill_n(memory.pointer(at,size),size,0x99);U offset=site==0x10001be0||site==0x10001c30?8:4;for(auto n:args){put(at+offset,n);offset+=4;}return at;};receive.release=[&](U,U,U){--depth;};
 for(U at:{0x10019880u,0x10019884u,0x100198acu,0x100199b0u})put(at,1);const U entry=0x1308000;put(entry+4,0x1211000);put(entry+8,0x1212000);put(entry+0xc,0x1212004);put(0x1212000,1);
 assert(mission_netsock_get_message(receive,entry)==0&&depth==0&&get(0x1212000)==7&&get(0x1212004)==9&&get(queue+0xc)==0&&memory.blocks.count(payload)==0);for(U i=0;i<7;++i)assert(read(0x1211000+i,1)==0x40+i);
 // Destruction scans physical slots and keeps its one cached free target.
 set_enqueue(7);mission_netsock_queue_enqueue(b,frame);const auto last=memory.allocate(3);put(ring+0xfff0,last);put(queue+0xc,2);change_free=true;put(frame+4,queue);mission_netsock_queue_destroy(b,frame);assert(get(queue)==0&&get(queue+4)==0&&get(queue+8)==0&&get(queue+0x10)==0&&get(queue+0xc)==2&&memory.allocations==memory.frees);auto empty=b;empty.call={};mission_netsock_queue_destroy(empty,frame);
 // Null allocation preserves the source's queue-base store before the bad access.
 auto missing=b;missing.call=[](U,const auto&)->U{return 0;};assert(fails([&]{mission_netsock_queue_initialize(missing,frame);}));assert(get(queue)==0&&get(queue+0xc)==2);
 put(0x10006068,0x1100300);put(0x10006010,0x1100100);change_free=false;assert(mission_netsock_queue_initialize(b,frame)==0);const auto current=get(queue);set_enqueue(7);put(queue+0xc,0xff6);const auto before=memory.allocations;assert(fails([&]{mission_netsock_queue_enqueue(b,frame);}));assert(get(queue+0x10)==1&&memory.allocations==before);put(queue+0x10,0);
 put(queue+0xc,0xffffffffu);mission_netsock_queue_enqueue(b,frame);assert(get(queue+0xc)==0&&get(current)!=0);put(frame+4,queue);mission_netsock_queue_destroy(b,frame);assert(memory.allocations==memory.frees);
}
