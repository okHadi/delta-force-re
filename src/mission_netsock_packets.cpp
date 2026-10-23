#include "dfre/mission_netsock_packets.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
namespace dfre {namespace {
using U=std::uint32_t;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
U word(const AssetBytes& bytes,std::size_t at,unsigned width=4){U value=0;for(unsigned i=0;i<width;++i)value|=U(bytes.at(at+i))<<(8*i);return value;}
U rotate(U n,unsigned bits){return(n>>bits)|(n<<(32-bits));}
bool exact_dll(const AssetBytes& bytes){
    static constexpr U k[]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<U,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    check(bytes.size()<0x2000000,"Netsock DLL exceeds supported image limit");
    AssetBytes padded=bytes;padded.push_back(0x80);while(padded.size()%64!=56)padded.push_back(0);
    const auto bits=std::uint64_t(bytes.size())*8;for(unsigned i=8;i;--i)padded.push_back(std::uint8_t(bits>>((i-1)*8)));
    for(std::size_t at=0;at<padded.size();at+=64){
        std::array<U,64>w{};for(unsigned i=0;i<16;++i)for(unsigned j=0;j<4;++j)w[i]=(w[i]<<8)|padded[at+i*4+j];
        for(unsigned i=16;i<64;++i){auto a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));}
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(unsigned i=0;i<64;++i){const U t=v+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i];const U u=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));v=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    return h==std::array<U,8>{0xfe36bd01,0x81d27ab4,0x8b39d55f,0xe0e1899a,0x63ef5cfd,0xc07a903a,0xcce0d0cf,0x2aafcfb2};
}
} // namespace
struct MissionNetsockPacketMemory::Impl {
    MissionNetsockPacketMemoryBindings& foreign;AssetBytes image;
    Impl(const AssetBytes& raw,MissionNetsockPacketMemoryBindings& host):foreign(host){
        check(exact_dll(raw),"Netsock packet image needs the supported DLL hash");
        check(word(raw,0,2)==0x5a4d,"Netsock DLL needs a DOS header");const auto pe=word(raw,0x3c),opt=pe+24;
        check(word(raw,pe)==0x4550&&word(raw,pe+4,2)==0x14c&&word(raw,opt,2)==0x10b,"Netsock DLL needs PE x86");
        check(word(raw,opt+28)==0x10000000&&word(raw,opt+56)==0x1b000,"Netsock DLL image layout differs");image.resize(0x1b000);
        const auto headers=word(raw,opt+60);check(headers<=raw.size()&&headers<=image.size(),"Netsock DLL headers exceed the image");std::copy_n(raw.begin(),headers,image.begin());
        const auto table=opt+word(raw,pe+20,2),count=word(raw,pe+6,2);
        for(U i=0;i<count;++i){const auto at=table+i*40,offset=word(raw,at+20),length=word(raw,at+16),rva=word(raw,at+12);check(offset<=raw.size()&&length<=raw.size()-offset&&rva<=image.size()&&length<=image.size()-rva,"Netsock DLL section exceeds its image");std::copy_n(raw.begin()+offset,length,image.begin()+rva);}
    }
    bool owns(U at,std::size_t size)const{const auto end=std::uint64_t(at)+size;check(size&&end<=0x100000000ull,"Netsock memory range is invalid");if(at>=0x10000000&&end<=0x1001b000)return true;check(!(at<0x1001b000&&end>0x10000000),"Netsock access crosses the DLL image");return false;}
};
MissionNetsockPacketMemory::MissionNetsockPacketMemory(const AssetBytes& raw,MissionNetsockPacketMemoryBindings& foreign):impl_(std::make_unique<Impl>(raw,foreign)){}
MissionNetsockPacketMemory::~MissionNetsockPacketMemory()=default;
bool MissionNetsockPacketMemory::claims(U at,U width)const{check(width==1||width==2||width==4,"Netsock memory width is invalid");return impl_->owns(at,width);}
U MissionNetsockPacketMemory::read(U at,U width)const{if(claims(at,width))return word(impl_->image,at-0x10000000,width);check(bool(impl_->foreign.read),"Netsock foreign memory read is not bound");return impl_->foreign.read(at,width);}
void MissionNetsockPacketMemory::write(U at,U width,U value){if(claims(at,width)){for(U i=0;i<width;++i)impl_->image[at-0x10000000+i]=std::uint8_t(value>>(i*8));return;}check(bool(impl_->foreign.write),"Netsock foreign memory store is not bound");impl_->foreign.write(at,width,value);}
std::uint8_t* MissionNetsockPacketMemory::resolve(U at,std::size_t size){if(impl_->owns(at,size))return impl_->image.data()+(at-0x10000000);check(bool(impl_->foreign.resolve),"Netsock foreign memory resolve is not bound");auto* result=impl_->foreign.resolve(at,size);check(result!=nullptr,"Netsock address has no canonical storage");return result;}
namespace {
U read(const MissionNetsockPacketBindings& b,U at,U width=4){check(bool(b.read),"Netsock current memory read is not bound");return b.read(at,width);}
void write(const MissionNetsockPacketBindings& b,U at,U value,U width=4){check(bool(b.write),"Netsock current memory store is not bound");b.write(at,width,value);}
struct Frame {
    const MissionNetsockPacketBindings& b;U site,size,at;
    Frame(const MissionNetsockPacketBindings& source,U target,U bytes,const std::vector<U>& args):b(source),site(target),size(bytes),at(0){check(bool(b.frame)&&bool(b.release),"Netsock current caller frame is not bound");at=b.frame(site,size,args);}
    ~Frame(){b.release(site,at,size);}
};
U call(const MissionNetsockPacketBindings&,U,const std::vector<U>&);
U lookup(const MissionNetsockPacketBindings& b,U peer){const U index=peer-0x10000u;const auto count=read(b,0x100198b8);if(index>=count)return 0;const auto base=read(b,0x10019afc),record=base+index*0xbcu;return read(b,record+0x9c)?record:0;}
U name_host(const MissionNetsockPacketBindings& b,U frame){const auto peer=read(b,frame+4);const auto source=call(b,0x100019e0,{peer});if(!source)return 0xb;const auto destination=read(b,frame+8);for(U at=0;at<0x88;at+=4){const auto value=read(b,source+at);write(b,destination+at,value);}return 0;}
U receive_queue(const MissionNetsockPacketBindings& b,U frame,U queue){
    const auto destination=read(b,frame+8);
    const auto found=call(b,0x10003c00,{destination,frame,queue,frame+8,0});if(!found)return 0xc;
    const auto sender=read(b,frame+0x10),peer=read(b,frame+8),length=read(b,frame+0xc);
    write(b,sender,peer);write(b,length,read(b,frame));return 0;
}
U call(const MissionNetsockPacketBindings& b,U site,const std::vector<U>& args){
    if(b.before_call)b.before_call(site,args);
    if(site==0x100019e0)return lookup(b,args.at(0));
    if(site==0x10001a50)return 0;
    if(site==0x10001be0||site==0x10001c30){Frame f(b,site,0x14,args);return receive_queue(b,f.at,site==0x10001be0?0x10019b0c:0x10019c24);}
    if(site==0x10003c00){Frame f(b,site,0x18,args);return mission_netsock_queue_pop(b,f.at);}
    if(site==0x10001a20){Frame f(b,site,0xc,args);return name_host(b,f.at);}
    check(bool(b.call),"Netsock required import or body is not bound");return b.call(site,args);
}
}
U mission_netsock_get_message(const MissionNetsockPacketBindings& b,U frame){
    if(!read(b,0x10019880))return 1;if(!read(b,0x10019884))return 0x200;if(!read(b,0x100198ac))return 4;
    const auto output=read(b,frame+4);if(!output)return 2;const auto length=read(b,frame+8);if(!length)return 2;
    const auto role=read(b,0x100199b0),sender=read(b,frame+0xc);return call(b,role==1?0x10001be0:0x10001c30,{output,length,sender});
}
U mission_netsock_queue_pop(const MissionNetsockPacketBindings& b,U frame){
    const auto queue=read(b,frame+0xc);auto lock=read(b,queue+0x10);
    if(lock){const auto sleep=read(b,0x10006010);do{call(b,sleep,{0});lock=read(b,queue+0x10);}while(lock);}
    const auto head=read(b,queue+4),tail=read(b,queue+8);write(b,queue+0x10,1);if(head==tail){write(b,queue+0x10,0);return 0;}
    const auto base=read(b,queue);const U next=head+0x10;write(b,frame+0xc,next);if(next>=base+0x10000u)write(b,frame+0xc,base);
    const auto address=read(b,frame+0x10);if(address)write(b,address,read(b,head+8));const auto port=read(b,frame+0x14);if(port)write(b,port,read(b,head+0xc));
    const auto length_pointer=read(b,frame+8),length=read(b,head+4),destination=read(b,frame+4);write(b,length_pointer,length);
    const auto count=read(b,head+4),source=read(b,head);U offset=0;
    for(U i=0;i<(count>>2);++i,offset+=4){const auto value=read(b,source+offset);write(b,destination+offset,value);}
    for(U i=0;i<(count&3);++i,++offset){const auto value=read(b,source+offset,1);write(b,destination+offset,value,1);}
    const auto payload=read(b,head),free_target=read(b,0x10006068);call(b,free_target,{payload});
    const auto current_next=read(b,frame+0xc);write(b,head,0);const U remaining=read(b,queue+0xc)-1u;
    write(b,queue+4,current_next);write(b,queue+0xc,remaining);if(remaining&0x80000000u)call(b,0x10003aa0,{0x10007128,remaining});write(b,queue+0x10,0);return 1;
}
U mission_netsock_player_name(const MissionNetsockPacketBindings& b,U frame){
    if(!read(b,0x10019880))return 1;if(!read(b,0x10019884))return 0x200;if(!read(b,0x100198ac))return 4;
    const auto output=read(b,frame+8);if(!output)return 2;const auto role=read(b,0x100199b0);const auto peer=read(b,frame+4);return call(b,role==1?0x10001a20:0x10001a50,{peer,output});
}
U mission_df_netsock_player_name(const MissionNetsockPacketBindings& b,U frame){
    if(!read(b,0x501320))return 0;const auto peer=read(b,frame+0x94);if(call(b,0x4c5520,{peer,frame}))return 0;
    U source=frame,destination=0xd0a500;
    for(;;){const auto first=read(b,source,1);write(b,destination,first,1);if(!first)return 0xd0a500;const auto second=read(b,source+1,1);source+=2;write(b,destination+1,second,1);destination+=2;if(!second)return 0xd0a500;}
}
} // namespace dfre
