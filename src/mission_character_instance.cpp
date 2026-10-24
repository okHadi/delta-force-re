#include "dfre/mission_character_instance.hpp"
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;using R=MissionCharacterAllocateRegisters;using B=MissionCharacterInstanceBindings;
U get(const B& b,U a,U n=4){if(!b.memory.read)throw MissionCharacterAllocateError("Character instance needs current memory reads");return b.memory.read(a,n);}
void put(const B& b,U a,U v,U n=4){if(!b.memory.write)throw MissionCharacterAllocateError("Character instance needs current memory writes");b.memory.write(a,n,v);}
std::int64_t si(U v){return v<=0x7fffffffu?std::int64_t(v):std::int64_t(v)-0x100000000LL;}
U sar(U v,U n){return U(si(v)>=0?si(v)/(std::int64_t(1)<<n):-((-si(v)+(std::int64_t(1)<<n)-1)/(std::int64_t(1)<<n)));}
bool more(U& count){const U old=count;--count;return si(old)>1;}
void divide(R& r){r.edx=si(r.eax)<0?0xffffffffu:0;const auto a=si(r.eax),d=si(r.ebx);if(!d||(a==-0x80000000LL&&d==-1))throw MissionCharacterAllocateError("Character instance signed division fault");r.eax=U(a/d);r.edx=U(a%d);}
void twice_product(R& r,U v){const auto product=std::uint64_t(si(r.eax)*si(v))*2;r.eax=U(product);r.edx=U(product>>32);}
void decode(const B& b,R& r,bool timed){
 const U instance=r.esi,frame=r.edi;
 r.eax=get(b,r.esi+0x58);bool custom=false;if(si(r.eax)>0x28)custom=get(b,r.edi+0x28)!=0;
 r.ecx=get(b,r.esi+0x14);if(custom)r.edi=get(b,r.edi+0x28);r.ebp=get(b,r.esi+0x80);r.esi=get(b,r.esi+0x84);
 do{
  if(timed){for(U i=0;i<3;++i){r.eax=0;r.eax=custom?(get(b,r.edi+i,1)<<12):0x10000;r.eax-=get(b,r.ebp+12+i*4);divide(r);put(b,r.esi+12+i*4,r.eax);}}
  else{
   r.eax=custom?get(b,r.edi,1):0x10000;r.ebx=custom?get(b,r.edi+1,1):0x10000;r.edx=custom?get(b,r.edi+2,1):0x10000;
   if(custom){r.eax<<=12;r.ebx<<=12;r.edx<<=12;}
   r.eax-=get(b,r.ebp+12);r.ebx-=get(b,r.ebp+16);r.edx-=get(b,r.ebp+20);
   put(b,r.esi+12,r.eax);put(b,r.esi+16,r.ebx);put(b,r.esi+20,r.edx);
  }
  r.ebp+=24;r.esi+=24;if(custom)r.edi+=4;
 }while(more(r.ecx));
 if(custom&&!timed)r.ebx=1;
 r.edi=frame;r.esi=instance;r.ecx=get(b,r.esi+0x14);r.eax=get(b,r.esi+0x58);put(b,0x439a14,r.eax);r.edi+=r.eax;r.ebp=get(b,r.esi+0x80);r.esi=get(b,r.esi+0x84);
 const U delta=r.esi,packed=r.edi,pose=r.ebp;
 do{
  if(timed){for(U i=0;i<3;++i){r.eax=0;r.eax=get(b,r.edi+i,1)<<17;r.eax-=get(b,r.ebp+i*4);r.eax=sar(r.eax<<7,7);divide(r);put(b,r.esi+i*4,r.eax);}}
  else{
   r.eax=get(b,r.edi,1);r.ebx=get(b,r.edi+1,1);r.edx=get(b,r.edi+2,1);r.eax<<=17;r.ebx<<=17;r.edx<<=17;
   r.eax-=get(b,r.ebp);r.ebx-=get(b,r.ebp+4);r.edx-=get(b,r.ebp+8);
   r.eax=sar(r.eax<<7,7);r.ebx=sar(r.ebx<<7,7);r.edx=sar(r.edx<<7,7);
   put(b,r.esi,r.eax);put(b,r.esi+4,r.ebx);put(b,r.esi+8,r.edx);
  }
  r.ebp+=24;r.esi+=24;r.edi+=4;
 }while(more(r.ecx));
 r.ebp=pose;r.edi=packed;r.esi=delta;r.eax=r.edi;r.eax-=get(b,0x439a14);
 if(get(b,r.eax+4)&0x80){
  r.edx=instance;r.ecx=0x30;r.eax=0;r.eax=get(b,r.edx+0x65,1)<<17;r.eax-=get(b,r.ecx+r.ebp+4);r.eax=sar(r.eax<<7,7);divide(r);put(b,r.ecx+r.esi+4,r.eax);
  r.edx=instance;r.ecx=0x18;r.eax=0;r.eax=get(b,r.edx+0x65,1);r.eax=sar((r.eax&0x80)?r.eax|0xffffff00u:r.eax,2)&255;r.eax<<=17;r.eax=sar(r.eax<<7,7);divide(r);
  const U at=r.ecx+r.esi+4;const U old=get(b,at);put(b,at,old+r.eax);
 }
 r.esi=instance;
 if(timed){for(U i=0;i<3;++i){r.eax=get(b,0x439a08+i*4);divide(r);put(b,r.esi+0x1c+i*4,r.eax);}}
 else{r.eax=get(b,0x439a08);r.ebx=get(b,0x439a0c);r.ecx=get(b,0x439a10);put(b,r.esi+0x1c,r.eax);put(b,r.esi+0x20,r.ebx);put(b,r.esi+0x24,r.ecx);}
 r.edi=get(b,r.esi+12);r.ecx=get(b,r.edi+8);r.edi=get(b,r.esi+16);r.ebp=get(b,r.edi+0x20);r.edi=get(b,r.esi+0x8c);r.esi=get(b,r.esi+0x90);
 do{
  if(timed){for(U i=0;i<3;++i){r.eax=get(b,r.ebp+i*4);r.eax-=get(b,r.edi+i*4);divide(r);put(b,r.esi+i*4,r.eax);}}
  else{r.eax=get(b,r.ebp);r.ebx=get(b,r.ebp+4);r.edx=get(b,r.ebp+8);r.eax-=get(b,r.edi);r.ebx-=get(b,r.edi+4);r.edx-=get(b,r.edi+8);put(b,r.esi,r.eax);put(b,r.esi+4,r.ebx);put(b,r.esi+8,r.edx);}
  r.esi+=12;r.edi+=12;r.ebp+=12;
 }while(more(r.ecx));
}
}
void mission_character_instance_delta(const B& b,R& r){decode(b,r,true);}
void mission_character_instance_immediate(const B& b,R& r){decode(b,r,false);}
void mission_character_instance_rotate(const B& b,R& r){
 r.eax=get(b,0x4399d0)&0x1ffffff;r.eax=sar(r.eax,16);r.ebx=get(b,0x42f1d0+r.eax*4);r.ecx=get(b,0x42f3d0+r.eax*4);
 r.eax=get(b,0x439a10);twice_product(r,r.ebx);r.ebp=r.edx;r.eax=get(b,0x439a08);twice_product(r,r.ecx);r.edx-=r.ebp;put(b,0x439b90,r.edx);
 r.eax=get(b,0x439a10);twice_product(r,r.ecx);r.ebp=r.edx;r.eax=get(b,0x439a08);twice_product(r,r.ebx);r.edx+=r.ebp;put(b,0x439a10,r.edx);r.eax=get(b,0x439b90);put(b,0x439a08,r.eax);
}
void mission_character_instance_frame(const B& b,R& r){
 const U instance=r.esi;r.ebp=get(b,r.esi+16);
 if(r.ebp){const U previous=r.ebp;r.eax=get(b,r.edi+0x18,1)*12;r.ebp=get(b,r.ebp+0x20);r.edx=get(b,r.edi+0x20);r.ebp+=r.eax;r.edx+=r.eax;
  r.eax=get(b,r.ebp);r.ebx=get(b,r.ebp+4);r.ecx=get(b,r.ebp+8);r.eax-=get(b,r.edx);r.ebx-=get(b,r.edx+4);r.ecx-=get(b,r.edx+8);r.eax+=get(b,r.edi+12);r.ebx+=get(b,r.edi+16);r.ecx+=get(b,r.edi+20);
  put(b,0x439a08,r.eax);put(b,0x439a0c,r.ebx);put(b,0x439a10,r.ecx);mission_character_instance_rotate(b,r);
  r.ebp=get(b,r.esi+12);r.ecx=get(b,r.ebp+8);r.ebp=previous;r.ebp=get(b,r.ebp+0x20);r.edx=get(b,r.esi+0x8c);
  do{r.eax=get(b,r.ebp);r.ebx=get(b,r.ebp+4);put(b,r.edx,r.eax);put(b,r.edx+4,r.ebx);r.eax=get(b,r.ebp+8);put(b,r.edx+8,r.eax);r.ebp+=12;r.edx+=12;}while(more(r.ecx));
 }
 put(b,r.esi+16,r.edi);r.ebx=get(b,r.edi+8,2);put(b,r.esi+24,r.ebx);if(r.ebx==1)mission_character_instance_immediate(b,r);else mission_character_instance_delta(b,r);r.esi=instance;
}
void mission_character_instance_auxiliary(const B& b,R& r){
 r.edi=get(b,r.esi);r.eax=get(b,r.edi+0x38);if(!r.eax){r.eax=0x3400;put(b,r.edi+0x38,r.eax);}r.ebx=get(b,r.edi+0x3c);if(!r.ebx){r.ebx=0x4800;put(b,r.edi+0x3c,r.ebx);}r.ecx=get(b,r.edi+0x40);if(!r.ecx){r.ecx=0x4800;put(b,r.edi+0x40,r.ecx);}
 put(b,0x43a1a0,0);put(b,0x43a1a4,0x800);put(b,0x43a19c,0xfffff800);put(b,0x43a1a8,r.eax);put(b,0x43a1ac,r.ebx);put(b,0x43a1b0,r.ecx);r.ebx+=0x400;put(b,0x43a1b4,r.ebx);r.ecx+=0x2800;put(b,0x43a1b8,r.ecx);r.eax=0-r.eax;r.ebx=0-r.ebx;r.ecx=0-r.ecx;put(b,0x43a198,r.eax);put(b,0x43a18c,r.ebx);put(b,0x43a188,r.ecx);r.ebx+=0x400;put(b,0x43a194,r.ebx);r.ecx+=0x2800;put(b,0x43a190,r.ecx);
 r.ebp=0x43b4f5;r.ecx=get(b,0x43b4f1);r.edi=get(b,r.esi+0x94);
 do{r.edx=get(b,r.ebp);put(b,r.edi,r.edx);for(U i=1;i<6;++i){r.eax=get(b,r.ebp+i*4);r.ebx=get(b,0x43a1a0+r.eax*4);put(b,r.edi+i*4,r.ebx);}r.edi+=24;r.ebp+=24;}while(more(r.ecx));
}
void mission_character_instance_construct(const B& b,R& r){
 const U instance=r.esi;put(b,r.esi,r.edi);put(b,r.esi+12,0);put(b,r.esi+16,0);r.eax=get(b,r.edi);
 if(r.eax==0x30524843)put(b,r.esi+0x58,0x24);else if(r.eax==0x31524843)put(b,r.esi+0x58,0x34);else{r.edx=0x439e2b;if(!b.memory.call)throw MissionCharacterAllocateError("Character header error43B6D5 is not bound");b.memory.call(0x43b6d5,r);}
 r.ecx=get(b,r.edi+16)*0x70+0x7ac;r.edx=r.esi+4;const U saved_size=r.ecx,saved_dest=r.edx,saved_chr=r.edi,saved_instance=r.esi;
 if(!b.allocate)throw MissionCharacterAllocateError("Character arena allocation497F60 is not bound");r.eax=b.allocate(r.ecx,4,0,0x439e3b);r.edx=saved_dest;put(b,r.edx,r.eax);r.ecx=saved_size;r.edi=saved_chr;r.esi=saved_instance;r.ebp=get(b,r.esi+4);r.eax=0;
 for(;;){put(b,r.ebp,r.eax);r.ebp+=4;const U old=r.ecx;r.ecx-=4;if(si(old)<=4)break;}
 r.eax=get(b,r.esi+4);put(b,r.esi+0x7c,r.eax);
 for(const auto p:{std::pair<U,U>{0x80,4},{0x84,0x18},{0x88,0x18},{0x8c,0x3c}}){r.ecx=get(b,r.edi+16)*p.second;r.eax+=r.ecx;put(b,r.esi+p.first,r.eax);}
 r.ecx=0x60;r.eax+=r.ecx;put(b,r.esi+0x90,r.eax);r.ecx=0x60;r.eax+=r.ecx;put(b,r.esi+0x94,r.eax);r.eax+=0x1e0;put(b,r.esi+0x98,r.eax);put(b,r.eax+8,0);put(b,r.eax+4,0);
 r.ecx=get(b,r.edi+16);r.ebp=get(b,r.edi+12);r.edx=get(b,r.esi+0x7c);r.ebx=get(b,r.edi+4);
 do{r.eax=get(b,r.ebp);const U flag=get(b,r.ebx+24);r.eax*=flag&1?0x5c:0x3c;r.eax+=r.ebx;put(b,r.edx,r.eax);r.ebp+=4;r.edx+=4;}while(more(r.ecx));
 r.eax=get(b,r.edi+16);put(b,r.esi+20,r.eax);mission_character_clear_scalars(b.memory,r);r.esi=instance;mission_character_instance_auxiliary(b,r);
}
}
