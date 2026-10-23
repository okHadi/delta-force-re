#include "dfre/mission_model_cleanup.hpp"
#include <stdexcept>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int64_t signed_word(U v){return v<0x80000000u?v:std::int64_t(v)-0x100000000ll;}
struct Memory {
 const MissionModelCleanupBindings& b;
 explicit Memory(const MissionModelCleanupBindings& value):b(value){if(!b.read||!b.write)throw std::runtime_error("model cleanup memory provider required");}
 U r(U at,unsigned width=4)const{return b.read(at,width);}
 void w(U at,U value)const{b.write(at,4,value);}
 void call(U site,std::vector<U> args)const{if(!b.call)throw std::runtime_error("model cleanup call provider required");b.call(site,args);}
};
void release_textures(const Memory& m){
 U count=m.r(0xc54480),index=0,cursor=0xc54480;
 if(signed_word(count)>0)do {
  if(m.r(cursor+0x1e,1)&0x80u)m.w(cursor+0x24,0);
  else {const U pointer=m.r(cursor+0x24);if(pointer){m.call(0x4980d0,{pointer});m.w(cursor+0x24,0);}}
  m.w(cursor+0x28,0);++index;count=m.r(0xc54480);cursor+=0x24;
  if(signed_word(index)>=signed_word(count))break;
 }while(true);
 m.w(0xc54480,0);m.call(0x49a620,{0xc54488,0,0x11940});
}
void release_lods(const Memory& m){
 U count=m.r(0xc65dcc),index=0,record=0xc65dd0;
 if(signed_word(count)>0)do {
  const U pointer=m.r(record+0x18);
  if(pointer)m.call(0x4980d0,{pointer});
  m.call(0x49a620,{record,0,0xb8});
  ++index;count=m.r(0xc65dcc);record+=0xb8;
  if(signed_word(index)>=signed_word(count))break;
 }while(true);
 m.w(0xc65dcc,0);
}
}
void mission_model_cleanup(const MissionModelCleanupBindings& b){
 const Memory m(b);release_textures(m);release_lods(m);m.call(0x49a620,{0xc80d14,0,0xf23c});
}
}
