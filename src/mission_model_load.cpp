#include "dfre/mission_model_load.hpp"
#include <cstring>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U value){std::int32_t result;std::memcpy(&result,&value,4);return result;}
struct Access {
 const MissionModelLoadBindings& b;
 U r(U at,U width=4)const{if(!b.read)throw MissionModelLoadError("Model load reader is not bound");return b.read(at,width);}
 void w(U at,U value,U width=4)const{if(!b.write)throw MissionModelLoadError("Model load writer is not bound");b.write(at,width,width==2?value&65535:value);}
 U call(U site,std::initializer_list<U> args)const{if(!b.call)throw MissionModelLoadError("Model load body is not bound");return b.call(site,args);}
};
}
U mission_model_load(const MissionModelLoadBindings& b,U arguments,U frame){
 if((arguments|frame)&3u||std::uint64_t(arguments)+12>0x100000000ull||std::uint64_t(frame)+4>0x100000000ull)throw MissionModelLoadError("Model load needs aligned current arguments and one frame word");
 const Access m{b};const U graphic=m.r(arguments),name=m.r(arguments+4),bank=m.r(arguments+8);
 if(!m.call(0x499040,{name}))return 0;
 m.call(0x499120,{name});m.call(0x499280,{graphic,0x7c});
 if(m.r(graphic,1)!='3'||m.r(graphic+1,1)!='D'||m.r(graphic+2,1)!='I'||m.r(graphic+3,1)<5){m.call(0x499020,{});return 0;}
 const U first_texture=m.r(0xc54480,2);m.w(graphic+0x70,first_texture,2);m.call(0x499280,{frame,4});
 const U texture_count=m.r(frame);if(si(texture_count)>0){U i=0;do{m.call(0x485ba0,{bank});++i;}while(si(i)<si(m.r(frame)));}
 const U end_texture=m.r(0xc54480,2),initial_lods=m.r(graphic+0x14);m.w(graphic+0x72,end_texture,2);
 U index=0;
 if(si(initial_lods)>0){U output=graphic;do{
  const U end=U(si(m.r(graphic+0x70))>>16),start=U(si(m.r(graphic+0x6e))>>16);output+=4;
  const U level=m.call(0x485e30,{start,end,bank});m.w(output+0x4c,level);++index;
 }while(si(index)<si(m.r(graphic+0x14)));}
 U unused=m.r(graphic+0x14);if(si(unused)<4){U output=graphic+unused*4;do{output+=4;++unused;m.w(output+0x4c,0);}while(si(unused)<4);}
 m.call(0x499020,{});const U first=m.r(graphic+0x50);m.w(graphic+0x74,0);
 if(first){U i=0,offset=0;for(;;){const U level=m.r(graphic+0x50);if(si(i)>=si(m.r(level+0xa8)))break;const U material=m.r(level+0xac)+offset,rate=m.r(material+0x1f,1);
  if(rate&&rate<8){const U high=U(si(m.r(material+0x1a))>>16),frames=m.r(material+0x1e,1),duration=high*frames;if(si(duration)>si(m.r(graphic+0x74)))m.w(graphic+0x74,duration);}
  offset+=0x4c;++i;
 }}
 const U last=m.r(graphic+0x50);if(last)m.w(graphic+0x68,m.r(last+0x24));return 1;
}
}
