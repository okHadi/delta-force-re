#include "dfre/mission_client_added.hpp"
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
class Added {
public:
    Added(const MissionClientAddedBindings& bindings,U at,U bytes):b(bindings),scratch(at),length(bytes){
        if(!b.read||!b.write)throw MissionClientAddedError("Client-added memory is not bound");
        if(!scratch||length<0x90||std::uint64_t(scratch)+length>0x100000000ull)
            throw MissionClientAddedError("Client-added scratch is incomplete");
    }
    U r(U at,U width=4){return b.read(at,width);}
    void w(U at,U value,U width=4){b.write(at,width,value);}
    U local(U offset,U width=4){check(offset,width);return r(scratch+offset,width);}
    void local(U offset,U width,U value){check(offset,width);w(scratch+offset,value,width);}
    U call(U site,std::vector<U> args){if(!b.call)throw MissionClientAddedError("Client-added body is not bound");return b.call(site,args);}
    void copy(U source,U destination,bool frame=false){
        for(;;){const auto first=r(source,1);store(destination,first,frame);if(!first)return;
            const auto second=r(source+1,1);source+=2;store(destination+1,second,frame);destination+=2;if(!second)return;}
    }
    void run(){
        const U peer=r(0x501344);local(0x6c,4,peer);const U payload=r(0x50136c);
        call(0x493a80,{scratch,0x4cc0f4,peer});call(0x4564b0,{scratch});
        if(!r(0x67f528))return;
        if(!payload){for(const U at:{0x64u,0x68u,0x70u,0x78u,0x74u,0x7cu})local(at,4,0);}
        else{
            local(0x64,4,r(payload,1)&0x7f);local(0x68,4,(r(payload,1)&0x80)?1:0);
            local(0x70,4,r(payload+4,1));local(0x78,4,r(payload+1,1));local(0x74,4,r(payload+2,1));
            const auto last=r(payload+3,1),primary=local(0x78);local(0x7c,4,last);
            if(r(r(0xad3ed4)+primary*20+0x10)!=3)local(0x78,4,0);
            const auto secondary=local(0x74);if(r(r(0xad3ed4)+secondary*20+0x10)!=4)local(0x74,4,0);
            const auto grenade=local(0x7c);if(r(r(0xad3ed4)+grenade*20+0x10)!=2)local(0x7c,4,0);
        }
        const auto initial_count=r(0x67f530);local(0x84,4,0xffffffff);local(0x80,4,0xffffffff);
        if(si(initial_count)>1){
            local(0x88,4,0xa53c5);U slot=1,client=0x6e9a90+0x9b84,offset=0x9b84;
            do{
                const auto current_peer=local(0x6c);if(current_peer==r(0x6e9a90+offset)){
                    call(0x4564b0,{0x4cc108});const auto actor=r(0x6e9a98+offset);
                    if(actor){const auto current_actor=r(0x6e9a98+offset);local(0x8c,4,(actor-0x7cca80)/0x114);call(0x45da30,{current_actor});call(0x4423f0,{0x10,scratch+0x8c,2});}
                    call(0x49a620,{client,0,0x9b84});call(0x442450,{client});
                    U cursor=slot;const auto limit=local(0x88);
                    do{cursor+=0x9b84;w(cursor+0x6e00ed,0,1);}while(cursor!=limit);
                }
                if(!r(0x6e9c1c+offset,1)){
                    const auto best=local(0x84),candidate=r(0x6e9b24+offset);
                    if(si(candidate)>si(best)){local(0x80,4,slot);local(0x84,4,candidate);}
                }
                const auto limit=local(0x88),count=r(0x67f530);client+=0x9b84;offset+=0x9b84;++slot;local(0x88,4,limit+1);
                if(si(slot)>=si(count))break;
            }while(true);
        }
        const auto selected=local(0x80);if(selected==0xffffffff){call(0x4564b0,{0x4cc130});return;}
        const U client=0x6e9a90+selected*0x9b84;call(0x49a620,{client,0,0x9b84});
        const auto current_peer=local(0x6c);w(client+0x18c,1,1);const auto name=call(0x497870,{current_peer});copy(name?name:0x4cc160,client+0x0c);
        if(r(0x6e5588)){
            const auto side=local(0x68),face=local(0x64);const auto actor=call(0x45d520,{face,side});w(client+8,actor);
            if(actor){w(actor+8,local(0x6c));const auto person=r(r(client+8)+0x0c);copy(client+0x0c,person+4);}
        }
        w(client+0x18c,1,2);w(client+0x18e,0,1);w(client+0x1c4,1,2);const auto stored_peer=local(0x6c);w(client+0x1b8,1);w(client,stored_peer);
        for(const auto field:std::vector<std::pair<U,U>>{{0xa0,0x64},{0xa4,0x68},{0x9c,0x70},{0x164,0x78},{0x168,0x74},{0x160,0x7c}})w(client+field.first,local(field.second));
        const auto base_value=r(r(0xad3ed4)+0x2c),grenade=local(0x7c);w(client+0xb0,base_value);
        const auto grenade_value=r(r(0xad3ed4)+grenade*20+4),primary=local(0x78);w(client+0xb4,grenade_value);
        w(client+0xb8,r(r(0xad3ed4)+primary*20+4));w(client+0xbc,r(r(0xad3ed4)+primary*20+8));
        const auto primary_last=r(r(0xad3ed4)+primary*20+12),secondary=local(0x74);w(client+0xc0,primary_last);
        for(U i=0;i<3;++i)w(client+0xc4+i*4,r(r(0xad3ed4)+secondary*20+4+i*4));
        w(client+0xd0,r(r(0xad3ed4)+0x20c));
        const auto amount=r(r(0xad3ed4)+0x180);w(client+0x16c,0);w(client+0xac,amount);
        for(U i=0;i<10;++i){const auto value=r(client+0xac+i*4),sum=r(client+0x16c);w(client+0x16c,sum+value);}
        call(0x4406e0,{client,local(0x70)});call(0x43f610,{client});
        const auto format=call(0x404cf0,{0x4cc178,0x4cc16c});const auto announcement=call(0x43f770,{format,client+0x0c});copy(announcement,scratch+3,true);
        U size=0;while(local(3+size,1))++size;++size;
        if(si(size)>1&&si(size)<0xc8){local(2,1,0);local(0,2,size+3);call(0x4423f0,{0x12,scratch,size+3});}
    }
private:
    void check(U offset,U width){if(std::uint64_t(offset)+width>length)throw MissionClientAddedError("Client-added access exceeds scratch memory");}
    void store(U address,U value,bool frame){if(frame)check(address-scratch,1);w(address,value,1);}
    const MissionClientAddedBindings& b;U scratch,length;
};
}
void mission_client_added(const MissionClientAddedBindings& b,U scratch,U bytes){Added(b,scratch,bytes).run();}
} // namespace dfre
