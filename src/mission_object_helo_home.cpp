#include "dfre/mission_object_helo_home.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){return n<0x80000000u?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
U absolute(U n){return (n&0x80000000u)?0u-n:n;}
U multiply(U a,U b){return U(std::uint64_t(std::int64_t(si(a))*si(b))>>22);}
struct Memory {const MissionObjectHeloBindings& b;U r(U at,U width=4)const{if(!b.read)throw MissionTickError("Helicopter home memory is not bound");return b.read(at,width);}void w(U at,U value)const{if(!b.write)throw MissionTickError("Helicopter home store is not bound");b.write(at,4,value);}U call(U site,const std::vector<U>& args)const{if(!b.call)throw MissionTickError("Helicopter home required call is not bound");return b.call(site,args);}};
}
U mission_object_helo_home(const MissionObjectHeloBindings& b,U actor){
 const Memory m{b};const U person=m.r(actor+0xc);const U y=m.r(actor+0x28);const U home_x=m.r(person+0x54);const U home_y=m.r(person+0x58);const U dy=home_y-y;const U x=m.r(actor+0x24);const U dx=home_x-x;const U angle=m.call(0x477770,{dx,dy});m.w(actor+0x8c,angle);const U distance=m.call(0x477670,{absolute(dx),absolute(dy),0});
 if(si(distance)>0x500000){m.w(actor+0x88,0xf0000);m.call(0x450100,{actor,0x120000,0x40000,2});return 1;}
 if(si(distance)>0x1e0000){m.w(actor+0x88,0x50000);m.call(0x450100,{actor,0x120000,0x40000,1});return 1;}
 if(si(distance)>0x40000){m.w(actor+0x88,0x28000);m.call(0x450100,{actor,0x80000,0x10000,0});return 1;}
 m.w(actor+0x88,0);return 0;
}
void mission_object_helo_altitude(const MissionObjectHeloBindings& b,U frame){
 const Memory m{b};const U actor=m.r(frame+0x30);U angle=m.r(actor+0x30);m.call(0x477820,{angle,frame+0xc,frame+0x10});const U person=m.r(actor+0xc);const U age=m.r(person+0x2c);
 if(si(age)>62){m.w(person+0x2c,0);U n=m.r(actor+0x24);m.w(frame,n);n=m.r(actor+0x28);m.w(frame+4,n);n=m.r(actor+0x2c);m.w(frame+8,n-0x30000u);n=m.call(0x486fd0,{actor,frame,0x30000});m.w(actor+0xdc,n);}
 U y=m.r(actor+0x28);U x=m.r(actor+0x24);U height=m.r(actor+0xdc);U ground=m.call(0x47df40,{x+0x40000,y});if(si(ground)>si(height))height=ground;
 y=m.r(actor+0x28);x=m.r(actor+0x24);ground=m.call(0x47df40,{x-0x40000u,y});if(si(ground)>si(height))height=ground;
 y=m.r(actor+0x28);x=m.r(actor+0x24);ground=m.call(0x47df40,{x,y+0x40000});if(si(ground)>si(height))height=ground;
 y=m.r(actor+0x28);x=m.r(actor+0x24);ground=m.call(0x47df40,{x,y-0x40000u});if(si(ground)>si(height))height=ground;
 angle=m.r(actor+0x30);m.call(0x477820,{angle,frame+0xc,frame+0x10});U speed=m.r(actor+0x7c);U trig=m.r(frame+0x10);const U dx=multiply(speed,trig);trig=m.r(frame+0xc);m.w(frame+0x18,dx);speed=m.r(actor+0x7c);const U dy=multiply(speed,trig);y=m.r(actor+0x28);m.w(frame+0x14,dy);x=m.r(actor+0x24);U rises=1;ground=m.call(0x47df40,{dx+x,dy+y});if(si(ground)>si(height)){if(si(ground-height)>0)rises=2;height=ground;}
 U step_y=m.r(frame+0x14);y=m.r(actor+0x28);U step_x=m.r(frame+0x18);x=m.r(actor+0x24);ground=m.call(0x47df40,{step_x*2+x,step_y*2+y});if(si(ground)>si(height)){if(si(ground-height)>0)++rises;height=ground;}
 step_y=m.r(frame+0x14);y=m.r(actor+0x28);step_x=m.r(frame+0x18);x=m.r(actor+0x24);ground=m.call(0x47df40,{step_x*4+x,step_y*4+y});if(si(ground)>si(height)){if(si(ground-height)>0)++rises;height=ground;}
 U z=m.r(actor+0x2c);m.w(actor+0xb4,z-height);U target=m.r(frame+0x34);const U mode=m.r(frame+0x3c,1);target+=height;const U step=0x100u<<(mode&31);const U vertical=m.r(actor+0xac);if(si(vertical)<0)target-=step;else{const U band=m.r(frame+0x38);m.w(frame+0x38,band+step);}z=m.r(actor+0x2c);U error=0;
 if(si(target)>si(z))error=target-z;else{const U band=m.r(frame+0x38);target+=band;if(si(target)<si(z))error=target-z;}
 const U stage=m.r(actor+0xd4);m.w(actor+0xb0,0);if(si(stage)>6&&si(stage)<12)error=1;if(si(m.r(actor+0xd4))>=12){m.w(actor+0xb0,0xfffffeccu);return;}
 const U force=rises<<8;if(si(error)>0){if(si(m.r(actor+0xac))>=0x800)m.w(actor+0xb0,0xffffff00u);else m.w(actor+0xb0,force);return;}
 if(si(error)<0){if(si(m.r(actor+0xac))<=-0x800)m.w(actor+0xb0,0x100);else{m.w(actor+0xb0,force);m.w(actor+0xb0,0u-force);}return;}
 const U current_vertical=m.r(actor+0xac);if(si(current_vertical)>0)m.w(actor+0xb0,0xffffff80u);else if(si(current_vertical)<0)m.w(actor+0xb0,0x80);const U current_force=m.r(actor+0xb0);m.w(actor+0xb0,current_force-0x4du);
}
}
