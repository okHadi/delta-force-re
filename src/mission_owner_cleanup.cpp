#include "dfre/mission_owner_cleanup.hpp"
#include <stdexcept>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int64_t si(U v){return v<0x80000000u?v:std::int64_t(v)-0x100000000ll;}
struct Memory {
 const MissionOwnerCleanupBindings& b;
 explicit Memory(const MissionOwnerCleanupBindings& value):b(value){if(!b.read||!b.write)throw std::runtime_error("owner cleanup memory provider required");}
 U r(U at,unsigned width=4)const{return b.read(at,width);}
 void w(U at,U value,unsigned width=4)const{b.write(at,width,width==4?value:value&255u);}
 void call(U site,std::vector<U> args={})const{if(!b.call)throw std::runtime_error("owner cleanup call provider required");b.call(site,args);}
};
void reset_playback(const Memory& m){if(m.r(0xd08940)){m.w(0x4a2712,1);m.call(0x4a2aed);m.w(0x4a2712,0);}}
void clear_ambient(const Memory& m){
 for(U offset=0;offset!=24;offset+=4){U handle=m.r(0x6e06e8+offset);if(handle)m.call(0x495070,{handle});}
 U count=m.r(0x6e06c8),index=0,offset=0;
 if(si(count)>0)do {
  U row=m.r(0x6e06c4)+offset;
  if(m.r(row+0x20,1)&4u){U pointer=m.r(row+0x18);m.call(0x4980d0,{pointer});}
  U base=m.r(0x6e06c4);m.w(base+offset+0x18,0);
  base=m.r(0x6e06c4);U flags=m.r(base+offset+0x20,1);m.w(base+offset+0x20,flags&0xfbu,1);
  ++index;count=m.r(0x6e06c8);offset+=0x28;
  if(si(index)>=si(count))break;
 }while(true);
 m.call(0x49a620,{0x6e0700,0,0x4000});
}
}
void mission_sound_cleanup(const MissionOwnerCleanupBindings& b){
 const Memory m(b);m.call(0x4560a0);reset_playback(m);clear_ambient(m);
 m.call(0x455b40);m.call(0x455ea0);
 for(U channel=10;channel!=16;++channel)m.call(0x4950a0,{channel,0});
 m.call(0x49cf70,{0x6e0518});m.call(0x49cf70,{0x6e05e0});
}
void mission_sound_bank_cleanup(const MissionOwnerCleanupBindings& b,U arguments,U frame){
 const Memory m(b);U context=m.r(arguments);if(!context)return;
 U banks=m.r(context+0x38);if(!banks)return;
 if((frame&3u)||frame>0xfffffff0u)throw std::runtime_error("sound bank cleanup frame range");
 m.w(frame+4,banks);U bank_count=m.r(context+0x28);m.w(frame,0);
 if(bank_count)do {
  m.w(frame+12,0);U bank=m.r(frame+4);
  if(m.r(bank+0x24)){
   m.w(frame+8,bank);
   do {
    U layer_slot=m.r(frame+8),layer=m.r(layer_slot+0x28),members=m.r(layer+0x10),i=0;
    for(;;){U count=m.r(layer,2);if(i>=count)break;
     U asset=m.r(members);
     if(m.r(asset+0x23,1)){U allocation=m.r(asset+0x24);m.call(0x4980d0,{allocation});m.w(asset+0x23,0,1);}
     ++i;members+=0x1c;
    }
    U allocation=m.r(layer+0x10);m.call(0x4980d0,{allocation});
    layer_slot=m.r(frame+8);U index=m.r(frame+12);bank=m.r(frame+4);
    layer_slot+=4;++index;U count=m.r(bank+0x24);m.w(frame+8,layer_slot);m.w(frame+12,index);
    if(index>=count)break;
   }while(true);
  }
  bank=m.r(frame+4);U allocation=m.r(bank+0x28);m.call(0x4980d0,{allocation});
  U next_bank=bank+0x50,index=m.r(frame);context=m.r(arguments);m.w(frame+4,next_bank);
  ++index;bank_count=m.r(context+0x28);m.w(frame,index);
  if(index>=bank_count)break;
 }while(true);
 context=m.r(arguments);U allocation=m.r(context+0x38);m.call(0x4980d0,{allocation});m.w(context+0x38,0);
}
void mission_resource_cleanup(const MissionOwnerCleanupBindings& b,U record){
 const Memory m(b);U pointer=m.r(record);if(pointer)m.call(0x4980d0,{pointer});
 pointer=m.r(record+4);if(pointer)m.call(0x4980d0,{pointer});
 pointer=m.r(record+8);if(pointer)m.call(0x4980d0,{pointer});
 m.call(0x49a620,{record,0,0x20});
}
void mission_resource_cleanup_global(const MissionOwnerCleanupBindings& b){mission_resource_cleanup(b,0x503304);}
}
