#include "dfre/mission_character_resource.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
template<class T>const T& need(const T& f){if(!f)throw std::runtime_error("Character resource reached a missing provider");return f;}
U r(const MissionDebriefBindings& b,U at,U size=4){return need(b.read)(at,size);}
void w(const MissionDebriefBindings& b,U at,U n,U size=4){need(b.write)(at,size,n);}
U call(const MissionDebriefBindings& b,U site,const std::vector<U>& args){return need(b.call)(site,args);}
std::int64_t sign(U n){return n<0x80000000u?n:std::int64_t(n)-0x100000000ll;}
U scale(U n){return U(std::uint64_t(sign(n)*0x1d2b0ll)>>16);}
void copy(const MissionDebriefBindings& b,U source,U target){for(;;){auto n=r(b,source,1);w(b,target,n,1);if(!n)return;n=r(b,source+1,1);source+=2;w(b,target+1,n,1);target+=2;if(!n)return;}}
void add(const MissionDebriefBindings& b,U at,U base){const auto n=r(b,at);w(b,at,n+base);}
}
void mission_character_resource_relocate(const MissionDebriefBindings& b,U filename,U output){
 const U base=call(b,0x499410,{filename});w(b,output,base);
 U pointer=r(b,base+0xc)+base;w(b,base+0xc,pointer);
 pointer=r(b,base+4)+base;w(b,base+4,pointer);U bones=r(b,base+8);
 do{
  add(b,pointer,base);add(b,pointer+4,base);add(b,pointer+8,base);
  U track=r(b,pointer+4),count=r(b,pointer+0x10);
  if(count)do{for(U off:{0u,4u,8u,0x2cu,0x30u,0x34u})add(b,track+off,base);w(b,track+0x24,0x1402,2);track+=0x58;const U old=count;--count;if(sign(old)<=1)break;}while(true);
  const U flags=r(b,pointer+0x18);pointer+=(flags&1)?0x5c:0x3c;const U old=bones;--bones;if(sign(old)<=1)break;
 }while(true);
 pointer=r(b,base+0x14)+base;w(b,base+0x14,pointer);
 for(U index=0;index<255;++index,pointer+=4){U animation=r(b,pointer);if(animation){animation+=base;w(b,pointer,animation);U frames=r(b,animation);animation+=0x14;do{add(b,animation+0x1c,base);add(b,animation+0x20,base);if(r(b,animation+0x28))add(b,animation+0x28,base);animation+=r(b,animation);const U old=frames;--frames;if(sign(old)<=1)break;}while(true);}}
 pointer=r(b,base+0x1c)+base;w(b,base+0x1c,pointer);for(U index=0;index<255;++index,pointer+=4){U n=r(b,pointer);if(n)w(b,pointer,n+base);}
 add(b,base+0x24,base);add(b,base+0x28,base);
 for(U off:{0x34u,0x44u,0x48u,0x4cu}){U n=r(b,base+off);if(n)w(b,base+off,n+base);}
}
void mission_character_resource_prepare(const MissionDebriefBindings& b,U index,U locals){
 U base=r(b,0x6ba4a0+index*0x64);w(b,locals,base);
 auto n=r(b,base+0x38);base=r(b,locals);w(b,base+0x38,scale(n));
 n=r(b,base+0x3c);base=r(b,locals);w(b,base+0x3c,scale(n));
 n=r(b,base+0x40);base=r(b,locals);U bone=r(b,base+4),count=r(b,base+8);w(b,base+0x40,scale(n));
 if(sign(count)<=0)return;
 U slot=0;do{
  U vertex=r(b,bone),vertices=r(b,bone+0xc),i=0;
  if(sign(vertices)>0)do{for(U off:{0u,4u,8u}){n=r(b,vertex+off);w(b,vertex+off,scale(n));}++i;vertices=r(b,bone+0xc);vertex+=0x44;}while(sign(i)<sign(vertices));
  U track=r(b,bone+4);vertex=r(b,bone);count=r(b,bone+0x10);i=0;
  if(sign(count)>0)do{
   for(U off:{0u,4u,8u}){w(b,locals+4,0x44);n=r(b,track+off)-vertex;const auto divisor=r(b,locals+4);if(!divisor)throw std::runtime_error("Character resource unsigned division fault");const U quotient=n/divisor;if(off==8)w(b,track+0x28,0);w(b,track+off,quotient);}
   ++i;count=r(b,bone+0x10);track+=0x58;
  }while(sign(i)<sign(count));
  for(U off:{0x1cu,0x20u,0x24u,0x28u,0x2cu,0x30u,0x34u,0x3cu,0x40u,0x44u,0x48u,0x4cu,0x50u}){n=r(b,bone+off);w(b,bone+off,scale(n));}
  base=r(b,locals);++slot;count=r(b,base+8);bone+=0x5c;
 }while(sign(slot)<sign(count));
}
U mission_character_resource_label(const MissionDebriefBindings& b,U payload,U label){w(b,payload-8,label);return payload-0x10;}
U mission_character_resource_load(const MissionDebriefBindings& b,U frame){
 U source=r(b,frame+0x14);copy(b,source,frame-0x5c);w(b,frame+0x14,frame-0x5c);
 U row=0x6ba440,slot=0;
 for(;;){w(b,frame-4,row);const U count=r(b,0x6bbd40);w(b,frame-8,slot*0x64);
  if(sign(slot)<sign(count)){
   const U current_row=r(b,frame-4);const U name=r(b,frame+0x14);
   if(!call(b,0x499e5e,{name,current_row}))return r(b,frame-4);
   row=r(b,frame-4)+0x64;++slot;continue;
  }
  if(sign(slot)>=64)return 0;break;
 }
 row=r(b,frame-4);w(b,frame-0xc,row+0x60);source=r(b,frame+0x14);row=r(b,frame-4);copy(b,source,row);
 call(b,0x456560,{frame-0x5c,0x4cbecc});source=r(b,frame+0x14);const U output=r(b,frame-0xc);mission_character_resource_relocate(b,source,output);
 source=r(b,frame+0x14);row=r(b,frame-4);const U label=row+0x20;copy(b,source,label);
 const U offset=r(b,frame-8);const U payload=r(b,0x6ba4a0+offset);(void)mission_character_resource_label(b,payload,label);
 const U prepare_index=r(b,0x6bbd40);mission_character_resource_prepare(b,prepare_index,frame-0x7c);
 const U count=r(b,0x6bbd40);row=r(b,frame-4);w(b,0x6bbd40,count+1);return row;
}
}
