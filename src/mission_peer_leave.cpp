#include "dfre/mission_peer_leave.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
template<class F> const F& need(const F& f){if(!f)throw std::runtime_error("Peer leave reached a missing provider");return f;}
U read(const MissionPeriodicBindings& b,U at,U width=4){return need(b.read)(at,width);}
void write(const MissionPeriodicBindings& b,U at,U width,U value){need(b.write)(at,width,value);}
U call(const MissionPeriodicBindings& b,U site,const std::vector<U>& args={}){return need(b.call)(site,args);}
}
void mission_peer_reset(const MissionPeriodicBindings& b,U frame){
    call(b,0x49a620,{frame,0,0xce});
    U mode=0;if(read(b,0x67f534)&&!read(b,0x6e558c))mode=1;
    write(b,frame+2,1,mode);
    for(U offset=0;offset!=0xa53c4;offset+=0x9b84){
        const U row=0x6e9a90+offset;
        if(!read(b,row+0x18c,1))call(b,0x49a620,{row,0,0x9b84});
        else{
            const auto state=read(b,row+0x1c4,2);
            if(state==3||state==4){call(b,0x4403c0,{frame,row});call(b,0x441750,{row,0x1e,frame,0xce});}
            if(read(b,row+0x1c4,2)!=1)write(b,row+0x1c4,2,0);
            for(U at:{0x8u,0x8cu,0x90u,0x154u,0x158u,0x15cu,0x194u,0x198u,0x19cu,0x1a0u,0x1a4u,0x1a8u,0x1acu,0x1b0u,0x1b4u,0x1bcu})write(b,row+at,4,0);
            for(U at:{0x1c6u,0x1c8u,0x1cau,0x1ccu})write(b,row+at,2,0);
        }
    }
    for(U offset=0;offset!=0x1a00;offset+=0x68){
        const U row=0x6e8090+offset;
        if(!read(b,row+0x40))call(b,0x49a620,{row,0,0x68});
        else for(U at=0x44;at!=0x68;at+=4)write(b,row+at,4,0);
    }
}
U mission_peer_leave_drain(const MissionPeriodicBindings& b){
    U count=0;if(!read(b,0x67f528)||!read(b,0x67f520))return count;
    while(call(b,0x460e40)){
        if(read(b,0x6e5588))call(b,0x442ae0);
        ++count;
    }
    call(b,0x4421d0);call(b,0x490880);return count;
}
}
