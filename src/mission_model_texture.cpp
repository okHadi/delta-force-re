#include "dfre/mission_model_texture.hpp"
#include <stdexcept>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int64_t signed_word(U v){return v<0x80000000u?v:std::int64_t(v)-0x100000000ll;}
struct Memory {
 const MissionModelTextureBindings& b;
 explicit Memory(const MissionModelTextureBindings& value):b(value){if(!b.read||!b.write)throw std::runtime_error("texture memory provider required");}
 U r(U at,unsigned width=4)const{return b.read(at,width);}
 void w(U at,U value,unsigned width=4)const{b.write(at,width,width==4?value:value&((U(1)<<(width*8))-1));}
 U call(U site,std::vector<U> args)const{if(!b.call)throw std::runtime_error("texture call provider required");return b.call(site,args);}
};
}
void mission_model_texture_initialize(const MissionModelTextureBindings& b){
 Memory m(b);m.w(0xc54480,0);m.call(0x49a620,{0xc54488,0,0x11940});
}
void mission_model_texture_load(const MissionModelTextureBindings& b,U args){
 Memory m(b);
 U count=m.r(0xc54480);if(signed_word(count)>=2000)return;
 const U record=0xc54488u+count*0x24u;
 m.call(0x499280,{record,0x24});
 U level=m.r(0xc80d10,1),flags=m.r(record+0x16,1);
 if(!(flags&(U(1)<<(level&31u)))){U size=m.r(record+0x10);m.call(0x4992d0,{size+0x400u,1});return;}
 U gate=m.r(0xc65dc8);
 if(gate){U size=m.r(record+0x10);m.call(0x4992d0,{size+0x400u,1});return;}
 U bank=m.r(args,1);m.w(record+0x17,bank,1);
 U size=m.r(record+0x10);U allocation=m.call(0x497f60,{size+0x400u,0x10,gate,record});
 m.w(record+0x1c,allocation);
 size=m.r(record+0x10);const U palette=allocation+size;
 size=m.r(record+0x10);U pixels=m.r(record+0x1c);m.w(record+0x20,palette);
 m.call(0x499280,{pixels,size+0x400u});
 U dest=0,source=0;
 do {
  U base=m.r(record+0x20);U value=m.r(base+source,1);m.w(base+dest,value,1);
  base=m.r(record+0x20);value=m.r(base+source+1,1);m.w(base+dest+1,value,1);
  base=m.r(record+0x20);dest+=3;value=m.r(base+source+2,1);source+=4;m.w(base+dest-1,value,1);
 }while(source!=0x400);
 if(m.r(0xc54484)){
  count=m.r(0xc54480);
  if(signed_word(count)>0){
   U i=0,prior=0xc54488;
   do {
    bank=m.r(args);U prior_bank=m.r(prior+0x17,1);
    if(prior_bank==bank){
     U prior_size=m.r(prior+0x10),current_size=m.r(record+0x10);
     if(prior_size==current_size&&!m.call(0x499e5e,{prior,record})){
      pixels=m.r(record+0x1c);m.call(0x4980d0,{pixels});
      U shared=m.r(prior+0x1c);m.w(record+0x1c,shared);
      shared=m.r(prior+0x20);m.w(record+0x20,shared);
      flags=m.r(record+0x16,1);m.w(record+0x16,flags|0x80u,1);
      count=m.r(0xc54480);m.w(0xc54480,count+1);return;
     }
    }
    ++i;count=m.r(0xc54480);prior+=0x24;
    if(signed_word(i)>=signed_word(count))break;
   }while(true);
  }
 }
 count=m.r(0xc54480);m.w(0xc54480,count+1);
}
}
