#include "dfre/mission_weapon_startup.hpp"
namespace dfre {namespace {using U=std::uint32_t;
U r(const MissionClientAddedBindings&b,U at,U n=4){if(!b.read)throw MissionClientAddedError("Weapon startup current read is not bound");return b.read(at,n);}
void w(const MissionClientAddedBindings&b,U at,U value,U n=4){if(!b.write)throw MissionClientAddedError("Weapon startup current store is not bound");b.write(at,n,n==1?value&255u:value);}
U call(const MissionClientAddedBindings&b,U site,const std::vector<U>&args){if(!b.call)throw MissionClientAddedError("Weapon startup reached service is not bound");return b.call(site,args);}
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
void copy(const MissionClientAddedBindings&b,U from,U to){for(;;){const auto first=r(b,from,1);w(b,to,first,1);if(!first)return;const auto second=r(b,from+1,1);from+=2;w(b,to+1,second,1);to+=2;if(!second)return;}}
}
void mission_weapon_startup(const MissionClientAddedBindings&b,U frame,U bytes){
    if(!frame||bytes<0x20||std::uint64_t(frame)+bytes>0x100000000ull)throw MissionClientAddedError("Weapon startup current frame is incomplete");
    w(b,0xad3ee4,0x4f9154);w(b,0xad3ee0,0x1c);w(b,0xad3ed4,0x4f9704);w(b,0xad3ed8,0x1c);w(b,0xad3ed0,0x4f9934);w(b,0xad3edc,0x27);
    U index=0,offset=0;
    do{
        call(b,0x493a80,{frame,0x4ccc60,index});const auto primary=call(b,0x404cf0,{0x4ccc70,frame});const auto first_base=r(b,0xad3ed0);copy(b,primary,first_base+offset+4);
        call(b,0x493a80,{frame,0x4ccc80,index});const auto secondary=call(b,0x404cf0,{0x4ccc70,frame});const auto second_base=r(b,0xad3ed0);copy(b,secondary,second_base+offset+0x44);
        const auto third_base=r(b,0xad3ed0),primary_id=r(b,third_base+offset+0x64);const auto graphic=call(b,0x4828c0,{primary_id});const auto primary_destination=r(b,0xad3ed0);w(b,primary_destination+offset+0x94,graphic);
        const auto fourth_base=r(b,0xad3ed0),secondary_id=r(b,fourth_base+offset+0x68);const auto alternate=call(b,0x4828c0,{secondary_id});const auto secondary_destination=r(b,0xad3ed0);++index;w(b,secondary_destination+offset+0x98,alternate);offset+=0xa8;
    }while(si(index)<si(r(b,0xad3edc)));
    U cursor=0;do{cursor+=4;w(b,0xad3dcc+cursor,600);}while(cursor!=0xfc);
    if(si(r(b,0xad3edc))<=0)return;
    const U end=r(b,0xad3edc)*0xa8u;cursor=0;
    do{
        const auto kind_base=r(b,0xad3ed0);auto kind=r(b,kind_base+cursor+0x74);if(kind==5||kind==6)kind=4;else if(kind==8||kind==9)kind=7;
        const auto rate_base=r(b,0xad3ed0);const U at=0xad3dd0+kind*4;const auto prior=r(b,at),rate=r(b,rate_base+cursor+0x9c);if(si(rate)<si(prior))w(b,at,rate);
        cursor+=0xa8;
    }while(si(cursor)<si(end));
}
} // namespace dfre
