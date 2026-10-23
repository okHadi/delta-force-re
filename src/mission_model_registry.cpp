#include "dfre/mission_model_registry.hpp"
#include <cstring>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U value){std::int32_t result;std::memcpy(&result,&value,4);return result;}
struct Access {
 const MissionModelRegistryBindings& b;
 U r(U at,U width=4)const{if(!b.read)throw MissionModelRegistryError("Model registry reader is not bound");return b.read(at,width);}
 void w(U at,U value,U width=4)const{if(!b.write)throw MissionModelRegistryError("Model registry writer is not bound");b.write(at,width,width==1?value&255:value);}
 U call(U site,std::initializer_list<U> args)const{if(!b.call)throw MissionModelRegistryError("Model registry body is not bound");return b.call(site,args);}
};
}
U mission_model_registry_load(const MissionModelRegistryBindings& b,U arguments,U frame){
 if((arguments|frame)&3u||std::uint64_t(arguments)+12>0x100000000ull||std::uint64_t(frame)+32>0x100000000ull)throw MissionModelRegistryError("Model registry needs aligned current arguments and 32 scratch bytes");
 const Access m{b};const U name=m.r(arguments),flag=m.r(arguments+8);m.w(flag,0);
 const U count=m.r(0xc80d14);U slot=0;
 if(si(count)>0){U entry=0xc80d24;for(;;){const U result=0xc80d20+slot*0x7c;if(!m.call(0x499e5e,{name,entry}))return result;const U current_count=m.r(0xc80d14);++slot;entry+=0x7c;if(si(slot)>=si(current_count))break;}}
 if(si(m.r(0xc80d14))>=500)return 0;
 U input=name,output=frame;
 for(;;){U byte=m.r(input,1);m.w(output,byte,1);if(!byte)break;byte=m.r(input+1,1);input+=2;m.w(output+1,byte,1);output+=2;if(!byte)break;}
 m.call(0x456560,{frame,0x4cdc88});m.call(0x48fd00,{frame});
 const U bank=m.r(arguments+4),current_count=m.r(0xc80d14),loaded=m.call(0x486370,{0xc80d20+current_count*0x7c,frame,bank});
 if(!loaded){m.call(0x48fd70,{});return 0;}
 m.call(0x48fd70,{});const U final_flag=m.r(arguments+8);m.w(final_flag,1);
 const U next=m.r(0xc80d14)+1;m.w(0xc80d14,next);return 0xc80d20+next*0x7c-0x7c;
}
}
