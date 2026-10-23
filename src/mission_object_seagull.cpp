#include "dfre/mission_object_seagull.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <utility>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U value){std::int32_t result;std::memcpy(&result,&value,4);return result;}
U sar(U value,unsigned bits){return U(si(value)>>bits);}
U absolute(U value){return si(value)<0?0u-value:value;}
U product(U a,U b){const auto wide=std::int64_t(si(a))*si(b);return sar(U(std::uint64_t(wide)>>22),6);}
struct Access {
 const MissionObjectSeagullBindings& b;
 U r(U at,U width=4)const{if(!b.read)throw MissionTickError("Seagull reader is not bound");return b.read(at,width);}
 void w(U at,U value,U width=4)const{if(!b.write)throw MissionTickError("Seagull writer is not bound");b.write(at,width,width==1?value&255:width==2?value&65535:value);}
 U call(U site,std::initializer_list<U> args)const{if(!b.call)throw MissionTickError("Seagull body is not bound");return b.call(site,args);}
};
bool matching(const Access& m,U at,U actor){
 if(!m.r(at))return false;if(m.r(at+0x6c,1)&4)return false;
 const U side=m.r(at+0x72,2);if(side!=m.r(actor+0x72,2))return false;
 const U candidate=m.r(at+4),own=m.r(actor+4),kind=m.r(candidate+0x20);return kind==m.r(own+0x20);
}
U source(const MissionTickState& state,const AiActorRef& ref){static constexpr std::size_t caps[]{1024,1024,512,256,1024};if(ref.group>4||ref.index>=caps[ref.group]||!state.actor_at(ref))throw MissionTickError("Seagull actor has no physical owner");return ref.group==4?0x822e80u+U(ref.index)*0x114:mission_actor_source_address(ref.group,ref.index);}
}
U mission_seagull_flock_first(const MissionObjectSeagullBindings& b,U actor){const Access m{b};for(U slot=0;slot<1024;++slot){const U at=0x7dde80+slot*0x114;if(matching(m,at,actor))return at;}return 0;}
U mission_seagull_flock_index(const MissionObjectSeagullBindings& b,U actor){const Access m{b};U index=0xffffffff;for(U slot=0;slot<1024;++slot){const U at=0x7dde80+slot*0x114;if(matching(m,at,actor)){++index;if(at==actor)return index;}}return index;}
void mission_object_seagull_ai(const MissionObjectSeagullBindings& b,U actor,U frame){
 if((frame&3u)||std::uint64_t(frame)+28>0x100000000ull)throw MissionTickError("Seagull AI needs 28 aligned current frame bytes");
 const Access m{b};
 if(!m.r(0x67f528)){m.w(actor+0xfc,0x400);return;}
 if(m.r(actor+0x6c,1)&4){const U flags=m.r(actor+0x6d,1);m.w(actor+0xfc,0x400);m.w(actor+0x6d,flags|1,1);return;}
 if(std::int16_t(m.r(actor+0x74,2))<=0){const U z=m.r(actor+0x2c),y=m.r(actor+0x28),flags=m.r(actor+0x6c,2),x=m.r(actor+0x24);m.w(actor+0x88,0);m.w(actor+0xf0,0);m.w(actor+0x6c,flags|0x10c,2);m.call(0x43e7d0,{0x44,x,y,z});return;}
 m.w(actor+0xe8,0);const U index=mission_seagull_flock_index(b,actor);m.w(actor+0xf4,index);m.w(frame+0x14,index);
 if(!index){
  m.w(actor+0x88,0x30000);const U counter=m.r(actor+0x8f,1);m.w(actor+0xec,0x400000);m.w(actor+0x8f,counter+1,1);
  if(!(m.call(0x477860,{})&15)){const U z=m.r(actor+0x2c),y=m.r(actor+0x28),x=m.r(actor+0x24);m.call(0x43e7d0,{0x43,x,y,z});}
 }else{
  const U leader=mission_seagull_flock_first(b,actor);const U level=m.r(leader+0xec);m.w(actor+0xec,level);
  const U own_x=m.r(actor+0x24);U dx=m.r(leader+0x24)-own_x;const U own_y=m.r(actor+0x28);U dy=m.r(leader+0x28)-own_y;
  const U own_z=m.r(actor+0x2c),dz=m.r(leader+0x2c)-own_z;m.w(frame+0x10,dz);
  const U first_vx=m.r(leader+0x3c);dx+=first_vx<<4;const U index_byte=m.r(frame+0x14,1),index_now=m.r(frame+0x14),first_vy=m.r(leader+0x40);
  U negative=0u-sar(index_now+1,1),signed_side=(index_byte&1)?negative:0u-negative;dy+=first_vy<<4;
  const U vx=m.r(leader+0x3c);m.w(frame+8,vx*negative);const U vy=m.r(leader+0x40);m.w(frame+0xc,vy*signed_side);
  U skew=m.r(frame+8);skew+=m.r(frame+0xc);dx+=skew<<6;const U vy_again=m.r(leader+0x40);U cross=negative*vy_again;signed_side=0u-signed_side;cross+=signed_side*m.r(leader+0x3c);dy+=cross<<6;
  const U z_distance=absolute(m.r(frame+0x10));const U distance=m.call(0x477670,{absolute(dx),absolute(dy),z_distance});m.w(frame+0x18,distance);
  if(si(distance)<0x30000){m.w(actor+0x8c,m.r(leader+0x30));const U shifted=m.r(leader+0x7c),speed=m.r(leader+0x7c);m.w(actor+0x88,speed-sar(shifted,6));}
  else{
   const U requested=distance<<2;m.w(actor+0x88,requested);if(si(requested)>0x60000)m.w(actor+0x88,0x60000);
   const U angle=m.call(0x477770,{dx,dy});U delta=angle-m.r(actor+0x30);
   if(si(absolute(delta))>0x40000000){
    const U speed=m.r(actor+0x88),current_distance=m.r(frame+0x18);m.w(actor+0xe8,1);
    if(si(speed<<2)>si(current_distance)){const U leader_angle=m.r(leader+0x30),current_speed=m.r(actor+0x88),own_angle=m.r(actor+0x30);delta=leader_angle-own_angle;m.w(actor+0x88,sar(current_speed,1));}
    const U speed_again=m.r(actor+0x88),distance_again=m.r(frame+0x18);
    if(si(speed_again<<2)>si(distance_again)){const U leader_angle=m.r(leader+0x30),current_speed=m.r(actor+0x88),own_angle=m.r(actor+0x30);delta=leader_angle-own_angle;m.w(actor+0x88,sar(current_speed,1));}
    if(si(m.r(frame+0x18))<0x60000){const U leader_angle=m.r(leader+0x30),own_angle=m.r(actor+0x30);m.w(actor+0x88,0x6000);delta=leader_angle-own_angle;}
   }
   m.w(actor+0x8c,m.r(actor+0x30)+delta);
  }
 }
 const U speed=m.r(actor+0x7c),target=m.r(actor+0x88);
 if(speed!=target){const U difference=target-speed;if(si(absolute(difference))<0x8000)m.w(actor+0x7c,m.r(actor+0x88));else m.w(actor+0x7c,m.r(actor+0x7c)+(si(difference)>0?0x8000u:0xffff8000u));}
 const U yaw=m.r(actor+0x30);m.call(0x477820,{yaw,frame,frame+4});
 const U x_speed=m.r(actor+0x7c),cosine=m.r(frame+4);m.w(actor+0x3c,product(x_speed,cosine));
 const U sine=m.r(frame),y_speed=m.r(actor+0x7c);m.w(actor+0x40,product(y_speed,sine));
 const U flags=m.r(actor+0x6d,1);m.w(actor+0xfc,0x10);m.w(actor+0x6d,flags|1,1);
}
void mission_object_seagull_move(const MissionObjectSeagullBindings& b,U actor){
 const Access m{b};
 if(!(m.r(actor+0x6c,1)&4)){
  const U vx=m.r(actor+0x3c),x=m.r(actor+0x24),y=m.r(actor+0x28),level=m.r(actor+0xec);const U vy=m.r(actor+0x40);m.w(actor+0x24,x+vx);const U z=m.r(actor+0x2c);m.w(actor+0x28,y+vy);
  if(si(z)<si(level)){const U speed=m.r(actor+0x7c),height=m.r(actor+0x2c);m.w(actor+0x2c,height+sar(speed,9));}
  else{const U speed=m.r(actor+0x7c),height=m.r(actor+0x2c);if(si(level+speed)<si(height))m.w(actor+0x2c,height-sar(speed,9));}
  const U desired=m.r(actor+0x8c),yaw=m.r(actor+0x30),index=m.r(actor+0xf4);U turn=desired-yaw;
  if(index){if(si(turn)>0x800000)turn=0x800000;else if(si(turn)<-0x800000)turn=0xff800000;}
  else{if(si(turn)>0x80000)turn=0x80000;else if(si(turn)<-0x80000)turn=0xfff80000;const U height=m.r(actor+0x2c),target=m.r(actor+0xec);if(si(height)<si(target)){const U speed=m.r(actor+0x7c),current=m.r(actor+0x2c);m.w(actor+0x2c,current+sar(speed,9));}}
  const U current_yaw=m.r(actor+0x30);const U speed=m.r(actor+0x7c);m.w(actor+0x30,current_yaw+turn);
  const U amount=si(speed)>=0x30000?0x2000000:0x3000000,evading=m.r(actor+0xe8);
  if(evading){const U bank=m.r(actor+0xe0);if(si(bank)>0)m.w(actor+0xe0,bank-amount);const U current=m.r(actor+0xe0);if(si(current)<0)m.w(actor+0xe0,current+amount);return;}
  if(m.r(actor+0xe4)){const U bank=m.r(actor+0xe0)+amount;m.w(actor+0xe0,bank);if(si(bank)>0x18000000)m.w(actor+0xe4,evading);}
  else{const U bank=m.r(actor+0xe0)-amount;m.w(actor+0xe0,bank);if(si(bank)<-0x18000000)m.w(actor+0xe4,1);}
  return;
 }
 const U y=m.r(actor+0x28),x=m.r(actor+0x24);const U ground=m.call(0x47df40,{x,y})+0x1000,z=m.r(actor+0x2c);
 if(si(ground)<=si(z)){const U fall=m.r(actor+0xf0),fall_again=m.r(actor+0xf0);m.w(actor+0x2c,z+fall);m.w(actor+0xf0,fall_again-0x134);}
 const U height=m.r(actor+0x2c);
 if(si(ground)>=si(height)){
  if(si(m.r(actor+0xf0))<-0x1000){const U now_y=m.r(actor+0x28),now_x=m.r(actor+0x24);m.call(0x43e7d0,{0x48,now_x,now_y,height});}
  const U fall=m.r(actor+0xf0),vx=m.r(actor+0x3c);m.w(actor+0x2c,ground);const U vy=m.r(actor+0x40);m.w(actor+0x3c,sar(vx,2));m.w(actor+0xf0,sar(0u-fall,3));m.w(actor+0x40,sar(vy,2));
 }else{const U vx=m.r(actor+0x3c),now_x=m.r(actor+0x24),now_y=m.r(actor+0x28),vy=m.r(actor+0x40);m.w(actor+0x24,now_x+vx);m.w(actor+0x28,now_y+vy);}
 if(si(m.r(actor+0xf0))<-0x4d0){const U bank=m.r(actor+0xe0);if(si(bank)>0x30000000)m.w(actor+0xe0,bank-0x2000000);if(si(m.r(actor+0xe0))<0x30000000)m.w(actor+0xe3,m.r(actor+0xe3,1)+2,1);}
 else{const U bank=m.r(actor+0xe0);if(si(bank)>0)m.w(actor+0xe0,bank-0x2000000);if(si(m.r(actor+0xe0))<0)m.w(actor+0xe3,m.r(actor+0xe3,1)+2,1);}
}
void bind_mission_object_seagull(MissionTickState& state,MissionObjectSeagullBindings b,U frame){
 if(state.update_callbacks.count(0x451fd0)||state.secondary_callbacks.count(0x452310))throw MissionTickError("Seagull callbacks already have an owner");
 state.update_callbacks.emplace(0x451fd0,[b,frame](auto& current,const auto& ref,const auto& action){if(action.kind!=AiActorActionKind::update||action.target!=0x451fd0)throw MissionTickError("Seagull AI received another callback");mission_object_seagull_ai(b,source(current,ref),frame);});
 state.secondary_callbacks.emplace(0x452310,[b=std::move(b)](auto& current,const auto& ref,const auto& action){if(action.kind!=AiActorActionKind::secondary||action.target!=0x452310)throw MissionTickError("Seagull move received another callback");mission_object_seagull_move(b,source(current,ref));});
}
}
