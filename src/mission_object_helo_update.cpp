#include "dfre/mission_object_helo_update.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <limits>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;using S=std::int32_t;
S sign(U n){S s;std::memcpy(&s,&n,4);return s;}
S short_sign(U n){return n&0x8000?S(n&0xffff)-0x10000:S(n&0xffff);}
struct Body {
    const MissionObjectHeloBindings& b;U actor,frame;
    U read(U at,U width=4)const{if(!b.read)throw MissionTickError("Helicopter update read owner is not bound");return b.read(at,width);}
    void write(U at,U n,U width=4)const{if(!b.write)throw MissionTickError("Helicopter update write owner is not bound");b.write(at,width,width==1?n&0xffu:width==2?n&0xffffu:n);}
    U r(U offset,U width=4)const{return read(actor+offset,width);}
    void w(U offset,U n,U width=4)const{write(actor+offset,n,width);}
    U local(U offset)const{return read(frame+offset);}
    void local(U offset,U n)const{write(frame+offset,n);}
    U call(U site,std::vector<U> args)const{if(!b.call)throw MissionTickError("Helicopter update required body is not bound "+std::to_string(site));return b.call(site,args);}
};
void valid(U frame,std::size_t bytes){if(!frame||(frame&3)||bytes<0x44||std::uint64_t(frame)+0x44>0x100000000ull)throw MissionTickError("Helicopter update needs a current aligned 44h-byte frame");}
}
void mission_object_helo_update(const MissionObjectHeloBindings& b,U actor,U message,U frame,std::size_t bytes){
    valid(frame,bytes);Body h{b,actor,frame};U person=h.r(0xc),value=h.read(person+0x44),active=0,scale=1;
    U flags=0,current=0,result=0,height=0,definition=0,id=0,x=0,y=0,z=0,delay=0,first=0,second=0;
    h.w(0xfc,8);h.local(0x40,0);current=h.read(0x67f528);h.local(0x3c,value);
    if(current)goto host;
    if(h.r(0x6c,1)&4){if(h.r(0x1c))h.call(0x43f020,{actor,0});h.w(0xfc,0x400);return;}
    if(sign(h.r(0xd4))>2){
        h.call(0x4505d0,{actor,frame+0x18});z=h.local(0x20);y=h.local(0x1c);x=h.local(0x18);h.call(0x42a440,{x,y,z});
    }
    if(h.r(0x98))h.local(0x40,1);
    value=U(sign(h.r(0x98))>>13)+0x5000;h.w(0x20,value);
    if(sign(value)>0x12000)h.w(0x20,0x12000);else if(sign(value)<0x6000)h.w(0x20,0x6000);
    current=h.local(0x40);
    if(current){
        if(!h.r(0x1c)){definition=h.r(4);id=h.read(definition+0x20);if(id==0x7e2)h.call(0x43ef80,{actor,0x1f6,0,0});else if(id==0x7e5)h.call(0x43ef80,{actor,0x1f7,0,0});}
    }else if(h.r(0x1c))h.call(0x43f020,{actor,0});
    h.w(0xfc,0x10);return;
host:
    if(!h.r(0xc4)){definition=h.r(4);if(h.read(definition+0x20)==0x7e5){h.w(0xcc,0xc0000000);h.w(0xc4,h.r(0xcc));}h.w(0xc4,h.r(0xc4,1)|1,1);}
    flags=h.r(0x6c,1);
    if(flags&4){
        h.w(0xfc,flags&8?0x400:0x40);h.w(0x6d,h.r(0x6d,1)|1,1);h.write(person+0x44,0);
        h.w(0xa8,0);h.w(0x98,0);h.w(0xa0,0);goto target_path;
    }
    if(short_sign(h.r(0x74,2))<=0){
        flags=h.r(0x6c,2);h.w(0x88,0);h.w(0x6c,flags|0x10c,2);h.call(0x450d50,{actor});
        h.call(0x49a620,{actor+0x3c,0,0x18});h.call(0x49a620,{actor+0x54,0,0x18});h.w(0xfc,0x10);return;
    }
    if(message==1){
        value=h.read(0xa158c0);
        if(value==3||value==4){current=h.r(0xd4);flags=h.r(0x74,2);h.w(0xd4,current+1);h.w(0x74,flags+1,2);}
        if(h.read(0xa158c0)==6){current=h.r(0xd8);flags=h.r(0x74,2);h.w(0xd8,current+1);h.w(0x74,flags+1,2);}return;
    }
    flags=h.read(person+0x24,1);if(flags&1)goto condition;
    if(h.read(person+0x44)){
        if((flags&0x40)&&sign(h.read(person+0x90))>0)h.write(person+0x44,0);
        if(h.read(person+0x24,1)&2){result=h.call(0x4509b0,{actor});if(sign(result)>0x640000){h.write(person+0x44,0);h.write(person+0x38,0x3c);h.w(0xa8,1);}}
    }
    if(h.read(person+0x44)){active=1;goto target_path;}
    value=h.read(person+0x38);if(sign(value)>0){h.write(person+0x38,value-1);goto condition;}
    result=h.call(0x450c40,{actor});h.local(0x3c,result);
    if(result){active=1;h.write(person+0x38,0);goto target_path;}
condition:
    result=h.call(0x450ae0,{actor});
    if(result){if(result==1)h.w(0xa8,2);goto mark;}
    if(h.read(person+0x24,1)&2){result=h.call(0x4509f0,{actor});if(result){h.w(0xa8,1);goto rotor_up;}}
    h.w(0x88,0);
    if(h.read(person+0x9c,1)==2){active=1;goto lift;}
    if(sign(h.r(0x7c))>=0x400)goto lift;
    value=sign(h.r(0xb4))>=0x90000;current=h.r(0xb4);h.w(0xa8,value);
    if(sign(current)>0x80000)active=1;goto mark;
lift:
    h.w(0xa8,1);
mark:
    h.w(0x6d,h.r(0x6d,1)|1,1);
target_path:
    if(h.local(0x3c)){
        if(sign(h.r(0x98))>=0x14000000)h.call(0x4503f0,{actor});
        h.call(0x450710,{actor});active=1;goto rotor_test;
    }
    value=h.r(0xd4);if(sign(value)>2){scale=value;if(sign(value)>5)scale=5;}
    result=h.r(0xa8);if(result&&sign(h.r(0x98))>=0x14000000){
        if(result==1)goto high_lift;
        h.call(0x450100,{actor,0x120000,0x40000,scale});goto rotor_test;
    }
    h.local(0x30,h.r(0x24));h.local(0x34,h.r(0x28));h.local(0x38,h.r(0x2c)-0x30000);
    result=h.call(0x486fd0,{actor,frame+0x30,0x30000});h.w(0xdc,result);
    if(sign(h.r(0x98))<0x14000000){
        height=h.r(0xdc);value=h.r(0x2c);h.w(0xb4,value-height);definition=h.r(4);id=h.read(definition+0x20);
        height+=id==0x7e5?0x37000:0x3f000;
        if(sign(height)<sign(h.r(0x2c))){h.w(0xb0,0xfffffecc);goto rotor_test;}
        h.w(0xb0,0);h.w(0xac,0);h.w(0x2c,height);goto rotor_test;
    }
    height=h.r(0xdc);value=h.r(0x2c);result=value-height;h.w(0xb4,result);
    if(sign(result)>0x90000)goto high_lift;
    definition=h.r(4);id=h.read(definition+0x20);height+=id==0x7e5?0x37000:0x3f000;
    if(sign(height)>=sign(h.r(0x2c))){h.w(0xb0,0);h.w(0xac,0);h.w(0x2c,height);goto rotor_test;}
    if(sign(h.r(0xac))<=-0x400)h.w(0xb0,0);else h.w(0xb0,0xffffff66);
    active=1;goto rotor_test;
high_lift:
    h.call(0x450100,{actor,0x80000,0x10000,scale});
rotor_test:
    if(!h.r(0xa8)){
        if(active)goto rotor_up;
        current=h.r(0x98);if(sign(current)>0x700000)h.w(0x98,current-0x400000);else if(sign(current)>0)h.w(0x98,current-0x20000);
        current=h.r(0xa0);if(sign(current)>0x700000)h.w(0xa0,current-0x400000);else if(sign(current)>0)h.w(0xa0,current-0x20000);
        goto smoke;
    }
rotor_up:
    if(sign(h.r(0x98))<0x1b000000)h.w(0x9a,h.r(0x9a,2)+0x80,2);
    if(sign(h.r(0xa0))<0x1b000000)h.w(0xa2,h.r(0xa2,2)+0x80,2);
smoke:
    if(!(h.r(0x6c,1)&4)&&sign(h.r(0xd4))>2){
        h.call(0x4505d0,{actor,frame});z=h.local(8);y=h.local(4);x=h.local(0);h.call(0x42a440,{x,y,z});
        if(sign(h.r(0xd4))<=0x10){result=h.call(0x477860,{});if(!(result&7))h.w(0xd4,h.r(0xd4)+1);}
    }
    if(h.r(0x98))h.local(0x40,1);
    value=U(sign(h.r(0x98))>>13)+0x5000;h.w(0x20,value);
    if(sign(value)>0x12000)h.w(0x20,0x12000);else if(sign(value)<0x6000)h.w(0x20,0x6000);
    current=h.local(0x40);
    if(current){
        if(!h.r(0x1c)){definition=h.r(4);id=h.read(definition+0x20);if(id==0x7e2)h.call(0x43ef80,{actor,0x1f6,0,0});else if(id==0x7e5)h.call(0x43ef80,{actor,0x1f7,0,0});}
    }else if(h.r(0x1c))h.call(0x43f020,{actor,0});
    delay=h.r(0xfc);first=h.read(person+0x40);h.write(person+0x40,first+delay);
    first=h.read(person+0x2c);second=h.r(0xfc);h.write(person+0x2c,first+second);
}
void bind_mission_object_helo_update(MissionTickState& state,MissionObjectHeloBindings b,U frame,std::size_t bytes){
    valid(frame,bytes);if(state.update_callbacks.count(0x450e40))throw MissionTickError("Helicopter update callback already has an owner");
    state.update_callbacks.emplace(0x450e40,[b=std::move(b),frame,bytes](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=0x450e40)throw MissionTickError("Helicopter update callback received another body");
        static constexpr std::size_t capacities[]{1024,1024,512,256};
        if(ref.group>=4||ref.index>=capacities[ref.group]||!current.actor_at(ref))throw MissionTickError("Helicopter update actor has no physical owner");
        mission_object_helo_update(b,mission_actor_source_address(ref.group,ref.index),0,frame,bytes);
    });
}
} // namespace dfre
