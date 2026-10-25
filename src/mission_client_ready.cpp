#include "dfre/mission_client_ready.hpp"
namespace dfre {namespace {using U=std::uint32_t;
U r(const MissionClientAddedBindings&b,U at,U n=4){if(!b.read)throw MissionClientAddedError("Client ready current read is not bound");return b.read(at,n);}
void w(const MissionClientAddedBindings&b,U at,U value,U n=4){if(!b.write)throw MissionClientAddedError("Client ready current store is not bound");b.write(at,n,n==1?value&255u:n==2?value&65535u:value);}
U call(const MissionClientAddedBindings&b,U site,const std::vector<U>&args){if(!b.call)throw MissionClientAddedError("Client ready required body is not bound");return b.call(site,args);}
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::int32_t sh(U n){return (n&0x8000u)?std::int32_t(n&65535)-65536:std::int32_t(n&65535);}
void frame(U f,U bytes,U need){if(!f||bytes<need||std::uint64_t(f)+bytes>0x100000000ull)throw MissionClientAddedError("Client ready current frame is incomplete");}
void copy(const MissionClientAddedBindings&b,U from,U to){for(;;){const auto first=r(b,from,1);w(b,to,first,1);if(!first)return;const auto second=r(b,from+1,1);from+=2;w(b,to+1,second,1);to+=2;if(!second)return;}}
U scan(const MissionClientAddedBindings&b,U at){U remain=0xffffffff;while(remain){const auto value=r(b,at++,1);--remain;if(!value)break;}return ~remain-1;}
}
void mission_client_ready(const MissionClientAddedBindings&b,U f,U bytes){
    frame(f,bytes,0x128);const U offset=r(b,f+0x120)*0x9b84u,client=0x6e9a90+offset;
    const auto actor=r(b,client+8);w(b,client+0x1c4,5,2);const auto packet=r(b,f+0x124);const auto flags=r(b,actor+0x6c,1);w(b,actor+0x6c,flags&0xfdu,1);
    copy(b,packet,client+0x4c);copy(b,r(b,f+0x124)+0x20,client+0x2c);
    call(b,0x49a620,{f,0,0xe2});const auto current=r(b,client+8);
    w(b,f+0x40,U(si(r(b,current+0x24))>>15),2);w(b,f+0x42,U(si(r(b,current+0x28))>>15),2);w(b,f+0x44,U(si(r(b,current+0x2c))>>15),2);w(b,f+0x46,r(b,current+0x30)>>16,2);
    const auto team=r(b,current+0x70,2);
    if(team==1){
        U list=0,delta=0;auto marker_flags=r(b,0x78f280,1);
        while(!(marker_flags&2)){delta+=0x88;++list;if(si(delta)>=0x4400)break;marker_flags=r(b,0x78f280+delta,1);}
        if(si(delta)<0x4400){
            const auto person=r(b,current+0xc);w(b,person+0xac,list);const auto count=r(b,0x78f284+delta,2);w(b,f+0x48,count,2);if(sh(count)>8)w(b,f+0x48,8,2);
            w(b,f+0x104,0);U index=0,name=f+0x5a,list_word=list*0x88;
            for(;;){
                w(b,f+0x108,list_word);const auto limit=r(b,f+0x46);if(si(index)>=si(limit)>>16)break;
                const auto cursor=r(b,f+0x108),position=r(b,f+0x104);const auto marker=r(b,0x78f288+cursor,2);w(b,f+0x4a+position,marker,2);
                const auto id=r(b,0x8cf9b4+index*4);call(b,0x493a80,{f+0xe4,0x4cc67c,id});const auto text=call(b,0x404d80,{0x4cc684,f+0xe4});
                const auto first_length=scan(b,text);if(!first_length||scan(b,text)+1>=0xc)call(b,0x493a80,{name,0x4cc570,index});else copy(b,text,name);
                const auto next_position=r(b,f+0x104),next_list=r(b,f+0x108);name+=0xc;++index;w(b,f+0x104,next_position+2);list_word=next_list+4;
            }
        }
    }else if(team!=2)w(b,f+0x48,0,2);
    const U response_client=0x6e9a90+r(b,f+0x120)*0x9b84u;call(b,0x441750,{response_client,0xb,f,0xe2});call(b,0x447e80,{response_client});
    bool match=true;for(U i=0;i<9;++i){const auto input=r(b,f+0x124),expected=r(b,0xbd67d4+i*4),actual=r(b,input+0xba+i*4);if(actual!=expected)match=false;}
    if(!match){const auto slot=r(b,f+0x120);if(slot)call(b,0x447fb0,{slot});}
}
void mission_client_resupply(const MissionClientAddedBindings&b,U client){
    U cursor=client;const U end=client+0x28;
    do{
        const auto index=r(b,cursor+0xac),base=r(b,0xad3ed0);const U weapon=base+index*0xa8;const auto kind=r(b,weapon+0x74),ammo=r(b,0xad3ee4);const U row=ammo+kind*0x34;
        const auto first=r(b,row+4),second=r(b,row+8),target=r(b,weapon+0x74);w(b,client+0xd4+target*4,first*second);
        if(r(b,weapon+0x6c)==1){const auto current=r(b,weapon+0x74);const U at=client+0xd4+current*4;const auto value=r(b,at);w(b,at,value+1);}
        cursor+=4;
    }while(cursor!=end);
    if(r(b,client+0x168)==1){
        cursor=client+0xc;const U secondary_end=client+0x18;
        do{
            const auto index=r(b,cursor+0xac),base=r(b,0xad3ed0);const U weapon=base+index*0xa8;const auto kind=r(b,weapon+0x74),ammo=r(b,0xad3ee4);const U row=ammo+kind*0x34;
            const auto first=r(b,row+4),second=r(b,row+8),target=r(b,weapon+0x74);w(b,client+0xd4+target*4,first*second);
            const auto current=r(b,weapon+0x74);const U at=client+0xd4+current*4;const auto value=r(b,at);w(b,at,value+value);
            if(r(b,weapon+0xa4,1)&1){const auto last=r(b,weapon+0x74);const U cell=client+0xd4+last*4;const auto previous=r(b,cell);w(b,cell,previous+1);}
            cursor+=4;
        }while(cursor!=secondary_end);
    }
    w(b,client+0x154,1000);
}
void mission_client_reject(const MissionClientAddedBindings&b,U f,U bytes){
    frame(f,bytes,0x4c);const auto slot=r(b,f+0x48);if(!r(b,0x67f520)||!r(b,0x67f528)||si(slot)<1||si(slot)>=17)return;
    const U client=0x6e9a90+slot*0x9b84u;if(!r(b,client+0x18c,1))return;call(b,0x441750,{client,0x44,0,0});
    const auto text=call(b,0x404cf0,{0x4cc178,0x4cc6bc});call(b,0x493a80,{f,0x4cc278,text,client+0xc});const auto current=r(b,0x67db40);call(b,0x479f00,{f,current});
}
} // namespace dfre
