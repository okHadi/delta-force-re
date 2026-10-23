#include "dfre/mission_object_tree_spawn.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <utility>
namespace dfre { namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t v;std::memcpy(&v,&n,4);return v;}
U absbits(U n){const auto sign=0u-(n>>31);return (n^sign)-sign;}
U product(U a,U b){return U(std::uint64_t(std::int64_t(si(a))*si(b))>>16);}
struct Memory {
 const MissionObjectTreeBindings& b;
 U read(U at,U width=4)const{if(!b.read)throw MissionTickError("Tree current memory is not bound");return b.read(at,width);}
 void write(U at,U n,U width=4)const{if(!b.write)throw MissionTickError("Tree current store is not bound");b.write(at,width,n);}
 U call(U site,const std::vector<U>& args)const{if(!b.call)throw MissionTickError("Tree required body is not bound");return b.call(site,args);}
};
}
U mission_object_tree_signal(const MissionObjectTreeBindings& b,U actor){
 Memory m{b};U best=0;
 for(U slot=0;slot<256;++slot){
  const U candidate=0x7cca80u+slot*0x114u;
  if(!m.read(candidate))continue;
  const U side=m.read(candidate+0x70,2);const U own_side=m.read(actor+0x70,2);
  if(side==own_side||!side)continue;
  if(m.read(candidate+0x6c,1)&4)continue;
  const U local=m.read(0xc23820);
  if(candidate==local && (m.read(0x67dbc9,1)&8))continue;
  const U x=m.read(candidate+0x24);const U y=m.read(candidate+0x28);
  const U own_y=m.read(actor+0x28);const U own_x=m.read(actor+0x24);
  const U dy=y-own_y,dx=x-own_x;
  const U own_z=m.read(actor+0x2c);const U z=m.read(candidate+0x2c);
  const U distance_word=m.call(0x477670,{absbits(dx),absbits(dy),absbits(z-own_z)});
  const auto distance=si(distance_word)>>16;
  if(distance>=0x50000)continue;
  const U inverse=distance?U(std::int64_t(0x50000)/distance):0x50000;
  const U action=m.read(candidate+0x80)*0x1cu;
  const U definition=m.read(candidate+4);const U frames=m.read(definition+0x78);
  const U posture=m.read(candidate+0x6d,1);const U weight=m.read(frames+action+0x10);
  const U score=product(product(inverse,weight),(posture&0x40)?0x400000u:0x80000u);
  if(si(score)>si(best))best=score;
 }
 return best;
}
void mission_object_tree_spawn(const MissionObjectTreeBindings& b,U actor,U frame,U message){
 (void)message;Memory m{b};const U cooldown=m.read(actor+0xe0);U current=m.read(0xc23810);
 if(si(cooldown)>0)m.write(actor+0xe0,cooldown-1);
 else if(si(m.call(0x44fba0,{actor}))>0x100000){
  m.write(actor+0xe0,10);U found=0,index=0;const U count=m.read(0xc23814);
  if(si(count)>0)do{
   if(m.read(current+0x20)==0xfd3)found=current;
   const U now=m.read(0xc23814);++index;current+=0x13c;
   if(si(index)>=si(now))break;
  }while(true);
  if(found){
   const U z=m.read(actor+0x2c);const U y=m.read(actor+0x28);const U x=m.read(actor+0x24);
   m.call(0x43e7d0,{0x4c,x,y,z});m.call(0x477860,{});U yaw=m.call(0x477860,{})<<16;
   for(U i=0;i<3;++i){
    m.call(0x49a620,{frame,0,0x114});
    const U base=m.read(0xc23810);const U definition=U(std::int64_t(si(found-base))/0x13c);
    m.write(frame+4,found);m.write(frame,definition);
    const U graphic=m.read(found+0x6c);m.write(frame+0x10,graphic);
    const U alternate=m.read(found+0x70);m.write(frame+0x14,alternate);
    const U px=m.read(actor+0x24);m.write(frame+0x24,px);
    const U py=m.read(actor+0x28);m.write(frame+0x28,py);
    const U pz=m.read(actor+0x2c);m.write(frame+0x2c,pz+0x40000);
    const U noise=m.call(0x477860,{})<<16;m.write(frame+0x30,(noise&0x0f000000u)+yaw);
    m.write(frame+0x6c,0);const U update=m.read(found+0x84);m.write(frame+0x110,update);
    const U secondary=m.read(found+0x90);m.write(frame+0x10c,secondary);
    yaw+=0x40000000;m.call(0x45d970,{frame,0});
   }
  }
 }
 m.write(actor+0xfc,0x40);
}
void bind_mission_object_tree_spawn(MissionTickState& state,MissionObjectTreeBindings b,U frame){
 if(state.update_callbacks.count(0x451e70))throw MissionTickError("Tree callback already has an owner");
 state.update_callbacks.emplace(0x451e70,[b=std::move(b),frame](auto& current,const auto& ref,const auto& action){
  static constexpr std::size_t capacities[]{1024,1024,512,256};
  if(action.kind!=AiActorActionKind::update||action.target!=0x451e70)throw MissionTickError("Tree received another callback");
  if(ref.group>=4||ref.index>=capacities[ref.group]||!current.actor_at(ref))throw MissionTickError("Tree actor has no physical owner");
  mission_object_tree_spawn(b,mission_actor_source_address(ref.group,ref.index),frame);
 });
}
}
