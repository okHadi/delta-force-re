#include "dfre/mission_netsock_receiver.hpp"
#include "dfre/mission_netsock_queue.hpp"
#include "dfre/network.hpp"
#include "dfre/network_send.hpp"
#include "dfre/compression.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
namespace {using U=std::uint32_t;
template<class F>bool fails(F f){try{f();}catch(const std::exception&){return true;}return false;}
struct Memory {
 std::map<U,dfre::AssetBytes> blocks;
 std::uint8_t* pointer(U at,std::size_t n){auto i=blocks.upper_bound(at);if(i==blocks.begin())throw std::runtime_error("Unmapped receiver memory");--i;if(std::uint64_t(at-i->first)+n>i->second.size())throw std::runtime_error("Receiver memory exceeds its owner");return i->second.data()+at-i->first;}
 U read(U at,U n){U value=0;for(U i=0;i<n;++i)value|=U(*pointer(at+i,1))<<(8*i);return value;}
 void write(U at,U n,U value){for(U i=0;i<n;++i)*pointer(at+i,1)=std::uint8_t(value>>(8*i));}
};}
int main(int argc,char** argv){using namespace dfre;Memory memory;for(auto [at,n]:std::vector<std::pair<U,U>>{{0x10000000,0x1b000},{0x1200000,0x10000},{0x1300000,0x10000}})memory.blocks[at].resize(n,0xa5);
 MissionNetsockPacketMemoryBindings foreign{[&](U at,U n){return memory.read(at,n);},[&](U at,U n,U value){memory.write(at,n,value);},[&](U at,std::size_t n){return memory.pointer(at,n);}};std::unique_ptr<MissionNetsockPacketMemory> dll;if(argc==2){std::ifstream file(std::filesystem::path(argv[1])/"Netsocknl.dll",std::ios::binary);AssetBytes raw((std::istreambuf_iterator<char>(file)),{});dll=std::make_unique<MissionNetsockPacketMemory>(raw,foreign);}
 auto get=[&](U at,U n=4){return dll?dll->read(at,n):foreign.read(at,n);};auto put=[&](U at,U value,U n=4){if(dll)dll->write(at,n,value);else foreign.write(at,n,value);};auto pointer=[&](U at,std::size_t n){return dll?dll->resolve(at,n):foreign.resolve(at,n);};
 if(!dll)for(U i=0;i<256;++i){U value=i<<24;for(U bit=0;bit<8;++bit)value=(value<<1)^((value&0x80000000u)?0x04c11db7u:0);put(0x10007234+i*4,value);}
 const U frame=0x1300000,destination=0x1204000,outputs=0x1208000;U depth=0,calls=0,recv_result=0,error=0,ioctl_result=0,poll_count=0,mutation=0;bool expanded=false;AssetBytes wire;std::vector<U> targets;
 const auto setup=[&](const AssetBytes& packet){wire=packet;recv_result=U(wire.size());error=mutation=0;expanded=false;calls=0;targets.clear();std::fill_n(pointer(destination,0x1000),0x1000,0xcc);std::fill_n(pointer(outputs,0x40),0x40,0xa5);put(frame+0x34,destination);put(frame+0x38,0x1000);put(frame+0x3c,outputs);put(frame+0x40,outputs+4);put(frame+0x44,outputs+8);put(0x10019854,0x1200000);put(0x1001985c,0x2000);put(0x100096b4,77);put(0x100096b8,0);put(0x10019860,1);put(0x100096ac,0x1100100);};
 MissionNetsockReceiverBindings b;b.read=get;b.write=[&](U at,U n,U value){if((mutation&2)&&expanded&&at==destination){put(frame+0x24,0xabcdef01);put(frame+0x22,0x9876,2);}put(at,value,n);};b.frame=[&](U site,U,const auto& args){const U at=0x1301000+depth++*0x100;U offset=(site==0x10001be0||site==0x10001c30)?8:4;for(auto n:args){put(at+offset,n);offset+=4;}return at;};b.release=[&](U,U,U){--depth;};b.before_call=[&](U target,const auto&){targets.push_back(target);++calls;};
 b.call=[&](U target,const auto& args)->U{if(target==0x1000595e){assert(args.size()==6&&args[0]==77&&args[2]==0x2000&&args[3]==0&&get(args[5])==16);if(recv_result!=0xffffffffu){std::copy(wire.begin(),wire.end(),pointer(args[1],wire.size()));put(args[4],2,2);put(args[4]+2,0x3412,2);put(args[4]+4,0x04030201);put(args[5],16);}return recv_result;}if(target==0x10005916)return error;if(target==0x10005958){const auto n=args[0];return(n>>24)|((n>>8)&0xff00)|((n<<8)&0xff0000)|(n<<24);}if(target==0x1000592e){if(mutation&1){put(frame+0x40,outputs+0x14);put(frame+0x44,outputs+0x18);}return((args[0]&255)<<8)|((args[0]>>8)&255);}if(target==0x10005934){assert(args[0]==77&&args[1]==0x4004667f);put(args[2],poll_count);return ioctl_result;}if(target==0x1100100){assert(args[1]==0x10009854&&args[2]==0x10000);const auto count=expand_buffer(pointer(args[0],wire.size()-10),wire.size()-10,pointer(args[1],args[2]),args[2]);if(count<0)throw std::runtime_error("Native compressed fixture is invalid");expanded=true;return U(count);}throw std::runtime_error("Required receiver body is not bound");};
 const AssetBytes payload{0x10,0x20,0x30,0x40,0x50,0x60,0x70};setup(encode_direct_datagram(payload));assert(mission_netsock_receiver(b,frame)==0&&depth==0&&get(outputs)==0x04030201&&get(outputs+4,2)==0x1234&&get(outputs+6,2)==0xa5a5&&get(outputs+8)==payload.size());assert(std::equal(payload.begin(),payload.end(),pointer(destination,payload.size())));
 AssetBytes long_payload(121);for(U i=0;i<long_payload.size();++i)long_payload[i]=std::uint8_t(i*17);setup(encode_direct_datagram(long_payload));mutation=1;assert(mission_netsock_receiver(b,frame)==0&&get(outputs+0x14,2)==0x1234&&get(outputs+0x18)==long_payload.size()&&get(outputs+4)==0xa5a5a5a5);assert(std::equal(long_payload.begin(),long_payload.end(),pointer(destination,long_payload.size())));
 AssetBytes compressed(compress_bound(long_payload.size()));const auto bits=compress_buffer(long_payload.data(),int(long_payload.size()),compressed.data());compressed.resize((bits+7)/8);setup(encode_extended_datagram(compressed,{},true,0x2000).datagram);mutation=2;assert(mission_netsock_receiver(b,frame)==0&&expanded&&get(outputs)==0x04030201&&get(outputs+4,2)==0x7698&&get(outputs+8)==long_payload.size());assert(std::equal(long_payload.begin(),long_payload.end(),pointer(destination,long_payload.size())));
 setup(encode_extended_datagram(compressed,{},true,0x2000).datagram);put(0x10019860,0);assert(mission_netsock_receiver(b,frame)==0x6a&&get(outputs)==0&&get(outputs+4,2)==0&&get(outputs+8)==0);
 setup(encode_direct_datagram(payload));put(0x100096b8,1);put(0x100076a8,0x01020304);put(0x100076ac,0);assert(mission_netsock_receiver(b,frame)==0x66&&get(outputs)==0&&get(outputs+8)==0);
 setup(encode_direct_datagram(payload));put(frame+0x38,1);assert(mission_netsock_receiver(b,frame)==0x2738&&get(outputs)==0&&get(destination,1)==0xcc);
 for(U code:{0x2733u,0x2736u}){setup({});recv_result=0xffffffffu;error=code;assert(mission_netsock_receiver(b,frame)==(code==0x2733?0:code)&&get(outputs)==0&&get(outputs+8)==0&&targets.size()==2);}
 setup(encode_direct_datagram(payload));wire.back()^=0x80;assert(mission_netsock_receiver(b,frame)==0x271e&&get(outputs)==0&&get(outputs+8)==0&&get(destination,1)==payload[0]);
 setup(encode_direct_datagram(payload));auto missing=b;missing.call={};assert(fails([&]{mission_netsock_receiver(missing,frame);}));assert(get(frame+0x1c)==16&&get(destination,1)==0xcc);
 setup(encode_direct_datagram(payload));missing=b;missing.frame={};assert(fails([&]{mission_netsock_receiver(missing,frame);}));assert(depth==0&&get(frame+0x14)==wire.size());
 setup(encode_direct_datagram(payload));missing=b;missing.read=[&](U at,U n){if(at==0x10007234+((((0xffffffffu>>24)^payload[0])&255u)*4))throw std::runtime_error("Current table read failed");return get(at,n);};assert(fails([&]{mission_netsock_receiver(missing,frame);}));assert(get(destination,1)==payload[0]&&get(destination+1,1)==0xcc&&get(outputs)==0xa5a5a5a5&&depth==0);
 for(U result:{0u,1u,0xffffffffu})for(U count:{0u,1u,0xffffffffu}){ioctl_result=result;poll_count=count;assert(mission_netsock_receive_poll(b,frame)==(result==0xffffffffu?0:count));}
 // Actual receive -> queue producer -> GetMessage uses the same buffer bytes.
 setup(encode_direct_datagram(long_payload));assert(mission_netsock_receiver(b,frame)==0);
 U next=0x22000000,allocations=0,frees=0;MissionNetsockQueueBindings queue_bindings;queue_bindings.read=b.read;queue_bindings.write=b.write;const auto receiver_call=b.call;
 queue_bindings.call=[&](U target,const auto& args)->U{if(target==0x1100200){const auto at=next;next+=(args[0]+0xfff)&~0xfffu;memory.blocks[at].resize(args[0]);++allocations;return at;}if(target==0x1100300){assert(memory.blocks.erase(args[0])==1);++frees;return 0;}return receiver_call(target,args);};
 const U queue=0x10019b0c,entry=0x1308000;put(0x1000605c,0x1100200);put(0x10006068,0x1100300);put(entry+4,queue);mission_netsock_queue_initialize(queue_bindings,entry);
 put(entry+4,destination);put(entry+8,get(outputs+8));put(entry+0xc,queue);put(entry+0x10,0x10001);put(entry+0x14,0x1234);put(entry+0x18,0);mission_netsock_queue_enqueue(queue_bindings,entry);assert(get(queue+0xc)==1);
 MissionNetsockPacketBindings packet_bindings=b;packet_bindings.call=queue_bindings.call;for(U at:{0x10019880u,0x10019884u,0x100198acu,0x100199b0u})put(at,1);put(entry+4,0x120a000);put(entry+8,outputs+0x20);put(entry+0xc,outputs+0x24);put(outputs+0x20,1);
 assert(mission_netsock_get_message(packet_bindings,entry)==0&&depth==0&&get(outputs+0x20)==long_payload.size()&&get(outputs+0x24)==0x10001&&get(queue+0xc)==0);assert(std::equal(long_payload.begin(),long_payload.end(),pointer(0x120a000,long_payload.size())));
 put(entry+4,queue);mission_netsock_queue_destroy(queue_bindings,entry);assert(allocations==frees);
 // The zero-count source gate has no required ntohl callback or argument read.
 put(0x100096b8,0);missing={};missing.read=get;assert(mission_netsock_ip_filter(missing,frame)==0);put(0x100096b8,1);assert(fails([&]{mission_netsock_ip_filter(missing,frame);}));
}
