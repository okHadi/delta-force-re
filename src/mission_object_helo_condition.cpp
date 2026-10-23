#include "dfre/mission_object_helo_condition.hpp"
#include <cstring>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
U absolute(U n){return si(n)<0?0u-n:n;}
struct Access {
    const MissionObjectHeloBindings& b;
    U r(U at,U width=4)const{if(!b.read)throw MissionTickError("Helicopter condition reader is not bound");return b.read(at,width);}
    void w(U at,U value)const{if(!b.write)throw MissionTickError("Helicopter condition writer is not bound");b.write(at,4,value);}
    U call(U site,std::initializer_list<U> args)const{if(!b.call)throw MissionTickError("Helicopter condition body is not bound");return b.call(site,args);}
};
}
U mission_object_helo_marker(const MissionObjectHeloBindings& b,U actor,U angle,U distance,U radius,U yaw){
    const Access m{b};const U person=m.r(actor+0xc),kind=m.r(person+0xa4);
    if(kind==1){
        const U list=m.r(person+0xac);if(!list)return 0;const U record=0x78f280+list*0x88;if(!m.r(record+4))return 0;
        const U entry=m.r(person+0xb0),marker=0x8ace80+m.r(record+8+entry*4)*0x114;
        const U actor_x=m.r(actor+0x24),actor_y=m.r(actor+0x28),x=m.r(marker+0x24),y=m.r(marker+0x28);
        const U dx=x-actor_x,dy=y-actor_y,ax=absolute(dx),ay=absolute(dy);
        const U small=si(ax)<=si(ay)?ax:ay,large=si(ax)<=si(ay)?ay:ax;
        m.w(distance,large+U(si(small)>>1));const U heading=m.call(0x477770,{dx,dy});m.w(angle,heading);
        m.w(radius,m.r(marker+0x7c));m.w(yaw,m.r(marker+0x30));return 1;
    }
    if(kind!=3)return 0;const U target=m.r(person+0xac);if(!target)return 0;
    const U identity=m.r(target+8);if(identity!=m.r(person+0xb0))return 0;
    const U x=m.r(target+0x24),y=m.r(target+0x28),actor_y=m.r(actor+0x28),actor_x=m.r(actor+0x24);
    const U dy=y-actor_y,dx=x-actor_x,actor_z=m.r(actor+0x2c),z=m.r(target+0x2c),dz=z-actor_z;
    const U length=m.call(0x477670,{absolute(dx),absolute(dy),absolute(dz)});m.w(distance,length);
    const U heading=m.call(0x477770,{dx,dy});m.w(angle,heading);m.w(radius,0x100000);return 1;
}
U mission_object_helo_condition(const MissionObjectHeloBindings& b,U actor,U frame,std::size_t bytes){
    if(frame%4||bytes<16||std::uint64_t(frame)+16>0x100000000ull)throw MissionTickError("Helicopter condition needs sixteen aligned frame bytes");
    const Access m{b};const U person=m.r(actor+0xc);
    if(m.r(person+0xa4)){
        if(!m.call(0x45c6a0,{actor,frame+8,frame+12,frame,frame+4})){m.w(person+0xa4,0);return 0;}
    }
    if(!m.r(person+0xa4))return 0;
    const U length=m.r(frame+12),radius=m.r(frame);
    bool advance=si(length)<=si(radius);
    if(!advance){const U speed=m.r(actor+0x7c),saved=m.r(frame+12);advance=si(absolute(speed)*2)>si(saved);}
    if(advance){
        const U entry=m.r(person+0xb0),list=m.r(person+0xac),side=m.r(actor+0x70);m.call(0x42ee30,{U(si(side)>>16),list,entry});
        const U current_list=m.r(person+0xac),current=m.r(person+0xb0),next=current+1;m.w(person+0xb0,next);
        const U count=m.r(0x78f284+current_list*0x88);if(si(next)>=si(count)){const U current_person=m.r(actor+0xc);m.w(current_person+0xb0,0);}
        const U after=m.r(person+0xb0);
        if(!after){const U list_now=m.r(person+0xac),flags=m.r(0x78f280+list_now*0x88,1);if(flags&1){
            m.w(m.r(actor+0xc)+0xa4,after);m.w(m.r(actor+0xc)+0xac,after);
            m.w(person+0x54,m.r(actor+0x24));m.w(person+0x58,m.r(actor+0x28));m.w(person+0x5c,m.r(actor+0x2c));return 0;
        }}
        m.call(0x45c6a0,{actor,frame+8,frame+12,frame,frame+4});
    }
    m.w(actor+0x8c,m.r(frame+8));const U side=m.r(actor+0x70);m.w(actor+0x88,m.r(0x7936bc+(U(si(side)>>16)<<6)));return 1;
}
}
