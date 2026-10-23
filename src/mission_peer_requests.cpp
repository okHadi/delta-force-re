#include "dfre/mission_peer_requests.hpp"
#include <stdexcept>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U v){return v<=0x7fffffffu?std::int32_t(v):std::int32_t(std::int64_t(v)-0x100000000LL);}
template<class F>const F& need(const F& f,const char* s){if(!f)throw std::runtime_error(s);return f;}
U read(const MissionPeerRequestBindings& b,U a,U width=4){return need(b.read,"Peer request memory reader is not bound")(a,width);}
void write(const MissionPeerRequestBindings& b,U a,U value,U width=4){need(b.write,"Peer request memory writer is not bound")(a,width,value);}
U word(const std::uint8_t* p,U at,U width=4){U value=0;for(U i=0;i<width;++i)value|=U(p[at+i])<<(8*i);return value;}
U signed_slot(const std::uint8_t* p){const auto v=word(p,6);return U(si(v)>>16);}
}
void mission_peer_trigger_charges(const MissionPeerRequestBindings& b,U owner){
    U index=0,count=read(b,0x8cfa1c);
    while(si(index)<si(count)){
        const U actor=0x7dde80+index*0x114;
        if(read(b,actor)){
            const auto flags=read(b,actor+0x6c,1);
            if(!(flags&6)){
                const auto definition=read(b,actor+4);const auto id=read(b,definition+0x20);
                if((id==0xfc8||id==0xfb4)&&owner==read(b,actor+0xf8)){
                    write(b,actor+0x74,0,2);const auto target=read(b,actor+0x110);
                    if(!target)throw std::runtime_error("Peer charge behavior callback is null");
                    need(b.call,"Peer charge behavior body is not bound")(target,{actor,1});
                }
            }
        }
        ++index;count=read(b,0x8cfa1c);
    }
}
void mission_peer_request_strike(const MissionPeerRequestBindings& b,MissionClientOutbox& outbox,std::size_t slot){
    if(slot>=17)throw std::runtime_error("Peer strike client slot exceeds17 records");
    if(si(read(b,0x6e5884))>0){
        U offset=0;while(si(read(b,0x6e5784+offset))>0){offset+=16;if(offset==0x100){outbox.enqueue(slot,0x2e,nullptr,0);return;}}
        write(b,0x6e5784+offset,0x500);
        const U client=0x6e9a90+U(slot)*0x9b84;const auto owner=read(b,client+8);write(b,0x6e5778+offset,owner);
        const auto x=read(b,owner+0x24);write(b,0x6e577c+offset,x);
        const auto current_owner=read(b,client+8);const auto y=read(b,current_owner+0x28);write(b,0x6e5780+offset,y);
        write(b,0x6e5884,read(b,0x6e5884)-1);outbox.enqueue(slot,0x2d,nullptr,0);return;
    }
    outbox.enqueue(slot,0x2e,nullptr,0);
}
void mission_peer_record_shot(const MissionPeerRequestBindings& b,std::uint8_t* packet,std::size_t length){
    const auto count=read(b,0x6e5570);if(si(count)>=100)return;
    if(si(count)<0)throw std::runtime_error("Peer shot ledger counter is negative");
    if(!packet||length!=15)throw std::runtime_error("Peer shot ledger needs fifteen mutable bytes");
    const U record=0x6e4760+count*36;
    U height=(word(packet,4)&255)*256+(word(packet,0)&255)*65536;if(height&0x800000)height|=0xff000000;
    write(b,record,signed_slot(packet));write(b,record+0x18,word(packet,0));write(b,record+0x1c,word(packet,4));
    write(b,record+4,word(packet,0)&0xffffff00u);
    const auto y=word(packet,4)&0xffffff00u,x=read(b,record+4);write(b,record+8,y);
    const auto ground=need(b.call,"Peer shot terrain height is not bound")(0x47df40,{x,y});write(b,record+0xc,ground+height);
    write(b,record+0x10,word(packet,10,2)<<16);write(b,record+0x14,word(packet,12,2)<<16);write(b,record+0x20,word(packet,14,1));
    write(b,0x6e5570,read(b,0x6e5570)+1);
    for(U slot=0;slot<17;++slot){
        const U owner=0x7cca80+signed_slot(packet)*0x114,client=0x6e9a90+slot*0x9b84;
        if(owner!=read(b,client+8))continue;
        write(b,client+0x1a4,read(b,client+0x1a4)+1);const auto current=read(b,client+8);write(b,current+0x78,0);return;
    }
}
}
