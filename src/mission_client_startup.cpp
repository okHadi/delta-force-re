#include "dfre/mission_client_startup.hpp"
#include <cstring>
#include <string>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t v;std::memcpy(&v,&n,4);return v;}
U read(const MissionClientStartupBindings& b,U at,U width=4){if(!b.read)throw MissionClientStartupError("Client startup needs current memory reads");return b.read(at,width);}
void write(const MissionClientStartupBindings& b,U at,U value,U width=4){if(!b.write)throw MissionClientStartupError("Client startup needs current memory writes");b.write(at,width,value);}
U invoke(const MissionClientStartupBindings& b,U site,std::vector<U> args){
    if(b.before_call)b.before_call(site,args);
    if(site==0x4406e0){mission_client_assign_team(b,args.at(0),args.at(1));return 0;}
    if(site==0x4715b0){mission_client_register_input(b,args.at(0));return 0;}
    if(site==0x4974c0||site==0x4974d0||site==0x4974e0||site==0x497500){mission_client_register_callback(b,site,args.at(0));return 0;}
    if(site==0x4487e0){mission_client_reset_banned(b);return 0;}
    if(!b.call)throw MissionClientStartupError("Client startup needs body "+std::to_string(site));return b.call(site,args);
}
void copy(const MissionClientStartupBindings& b,U source,U destination){for(;;){const auto first=read(b,source,1);write(b,destination,first,1);if(!first)return;const auto second=read(b,source+1,1);source+=2;write(b,destination+1,second,1);destination+=2;if(!second)return;}}
U length(const MissionClientStartupBindings& b,U at){U count=0;while(read(b,at+count,1))++count;return count;}
}
void mission_client_register_callback(const MissionClientStartupBindings& b,U site,U callback){U target=0;
    if(site==0x4974c0)target=0x501374;else if(site==0x4974d0)target=0x50137c;else if(site==0x4974e0)target=0x501380;else if(site==0x497500)target=0x501378;else throw MissionClientStartupError("Client callback setter is not owned");write(b,target,callback);
}
void mission_client_register_input(const MissionClientStartupBindings& b,U source){
    U row=0,offset=0;auto occupied=read(b,0xbc5c71,1);
    while(occupied){const auto id=read(b,offset+0xbc5c70,1);if(id==read(b,source,1))break;offset+=10;++row;if(si(offset)>=160)return;occupied=read(b,offset+0xbc5c71,1);}
    const U destination=0xbc5c70+row*10;write(b,destination,read(b,source));write(b,destination+4,read(b,source+4));write(b,destination+8,read(b,source+8,2),2);
}
void mission_client_assign_team(const MissionClientStartupBindings& b,U client,U requested){
    U first=0,second=0,team=1;const auto multiplayer=read(b,0x67f520);
    bool unique=false;
    if(multiplayer){const auto mode=read(b,0x67f53c);if(mode){
        if(mode==1||mode==3){for(U slot=0,at=0x6e9a90;slot<17;++slot,at+=0x9b84)if(client==at){team=slot+3;unique=true;break;}}
        if(!unique){bool selected=false;if(read(b,0x67f548,1)&0x20){if(requested==2||requested==1){team=requested;selected=true;}}
            if(!selected){for(U offset=0;offset!=0xa53c4;offset+=0x9b84){if(!read(b,offset+0x6e9c1c,1))continue;const auto side=read(b,offset+0x6e9b2c);if(side==1)first+=side;if(read(b,offset+0x6e9b2c)==2)++second;}team=si(first)>si(second)?2u:1u;}
        }
    }}
    const auto actor=read(b,client+8);write(b,client+0x9c,team);if(!actor)return;write(b,actor+0x70,team&0xffff,2);const auto current=read(b,client+8);write(b,current+0x72,unique?0:team,2);
}
void mission_client_setup_local(const MissionClientStartupBindings& b,U name,U face,U side){
    const auto peer=read(b,0x50133c);write(b,0x6e9a90,peer);copy(b,name,0x6e9a9c);write(b,0x6e9b30,face);write(b,0x6e9b2c,1);write(b,0x6e9b34,side);
}
void mission_client_reset_banned(const MissionClientStartupBindings& b){write(b,0x6e5888,0);write(b,0x6e588c,0);invoke(b,0x4568e0,{0x4cc6f0,0x448690});}
void mission_client_startup(const MissionClientStartupBindings& b){
    invoke(b,0x49a620,{0x6e9a90,0,0xa53c4});
    const auto name=invoke(b,0x404cf0,{0x4cc178,0x4cc1dc});copy(b,name,0x6e9a9c);
    write(b,0x6e9a98,0);write(b,0x6e9c54,0,2);write(b,0x6e9c48,1);write(b,0x6e9c1c,1,1);write(b,0x6e9c1d,1,1);write(b,0x6e9c1e,0,1);write(b,0x6e9c20,3,1);
    copy(b,0x4cc1e5,0x6e56fc);copy(b,0x4cc1f8,0x6e571c);copy(b,0x4cc1f8,0x6e573c);
    const auto first_length=length(b,0x6e573c);write(b,first_length+0x6e573d,0,1);const auto second_length=length(b,0x6e573c);write(b,second_length+0x6e573c,0x50,1);
    const auto address=invoke(b,0x454df0,{0x6e56fc});write(b,0x6e575c,address);write(b,0x6e5760,0xf23);
    for(const auto at:{0x6e558cu,0x6e5590u,0x6e5594u,0x6e5598u,0x6e559cu})write(b,at,0);
    write(b,0x501340,0);write(b,0x6e5574,0xd7);invoke(b,0x4406e0,{0x6e9a90,0});
    invoke(b,0x4974c0,{0x43f830});invoke(b,0x497500,{0x43fd60});invoke(b,0x4974d0,{0x43fef0});invoke(b,0x4974e0,{0x43ff20});write(b,0x67db68,0);
    for(U at=0x4f81b0;at!=0x4f8210;at+=0x10)invoke(b,0x4715b0,{at});
    invoke(b,0x454bb0,{2,0,0xf28});invoke(b,0x4487e0,{});
}
}
