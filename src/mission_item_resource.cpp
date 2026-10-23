#include "dfre/mission_item_resource.hpp"
#include "dfre/resource_plan.hpp"
#include <cstring>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
struct Access {
    const MissionItemResourceBindings& b;
    U r(U at,U width=4)const{if(!b.read)throw MissionItemResourceError("Item resource reader is not bound");return b.read(at,width);}
    void w(U at,U value)const{if(!b.write)throw MissionItemResourceError("Item resource writer is not bound");b.write(at,4,value);}
    U call(U site,std::initializer_list<U> args)const{if(!b.call)throw MissionItemResourceError("Item resource body is not bound");return b.call(site,args);}
};
void callbacks(const Access& m,U model,bool husk){
    const U first=m.call(0x4818f0,{m.r(model+0x28)});
    if(husk)m.w(model+0x3c,first);
    const U second_tag=m.r(model+0x2c);
    if(!husk)m.w(model+0x3c,first);
    const U second=m.call(0x4818f0,{second_tag});
    if(!husk)m.w(model+0x40,second);
    const U third_tag=m.r(model+0x30);
    if(husk)m.w(model+0x40,second);
    const U third=m.call(0x4818f0,{third_tag}),fourth_tag=m.r(model+0x34);
    m.w(model+0x44,third);
    const U fourth=m.call(0x4818f0,{fourth_tag}),matrix_tag=m.r(model+0x28);
    m.w(model+0x48,fourth);
    const U matrix=m.call(0x481a00,{matrix_tag});m.w(model+0x4c,matrix);
}
}
void mission_item_resource_load(const MissionItemResourceBindings& b,U index,U flag){
    if((flag&3u)||std::uint64_t(flag)+4>0x100000000ull)throw MissionItemResourceError("Item resource flag needs one aligned current word");
    const Access m{b};
    if(si(index)<0||si(index)>=si(m.r(0xc23814)))m.call(0x4564b0,{0x4ccb04});
    const U offset=index*0x13cu,initial=m.r(0xc23810)+offset;
    if(m.r(initial+0x6c))return;
    U bank=0;if(m.r(0x503138))bank=original_item_bank(m.r(initial+0x28),true);
    U item=m.r(0xc23810)+offset;
    if(m.r(item+0x2c,1)){
        const U graphic=m.call(0x486530,{item+0x2c,bank,flag});
        m.w(m.r(0xc23810)+offset+0x6c,graphic);
        item=m.r(0xc23810)+offset;
        if(m.r(item+0x28)==3){
            const U character=m.call(0x42cbb0,{item+0x4c});m.w(m.r(0xc23810)+offset+0x74,character);
            const U key=m.r(0x503228),animation_item=m.r(0xc23810)+offset;
            const U animation=m.call(0x42d200,{animation_item+0x5c,key});m.w(m.r(0xc23810)+offset+0x78,animation);
        }
        if(m.r(flag))callbacks(m,m.r(m.r(0xc23810)+offset+0x6c),false);
    }
    item=m.r(0xc23810)+offset;
    if(m.r(item+0x3c,1)){
        const U husk=m.call(0x486530,{item+0x3c,bank,flag});m.w(m.r(0xc23810)+offset+0x70,husk);
        if(m.r(flag))callbacks(m,m.r(m.r(0xc23810)+offset+0x70),true);
    }
    item=m.r(0xc23810)+offset;
    if(m.r(item+0x28)!=8)return;
    const U graphic=m.r(item+0x6c);if(!graphic)return;
    const U lod=m.r(graphic+0x50);if(!lod)return;
    const U width=U(si(m.r(lod+0x40))>>16),height=U(si(m.r(lod+0x58))>>16);
    m.w(item+0x9c,width*height);
}
}
