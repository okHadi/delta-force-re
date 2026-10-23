#include "dfre/mission_heap.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,size,align,label,selector,flags,mutation,fail_read,fail_write,fail_call;std::istringstream in(line);if(!(in>>mode>>size>>align>>label>>selector>>flags>>mutation>>fail_read>>fail_write>>fail_call))throw std::runtime_error("Short heap fixture");U incoming=0x12345678,cached=0;in>>incoming>>cached;
 constexpr U A=0x1100000,C=0x1200000,T=0x1210000,F=0x1300000,CB=0x405000;struct Span{U at;std::vector<std::uint8_t> bytes;};std::vector<Span> spans{{0x5013a0,std::vector<std::uint8_t>(4)},{0xcc0870,std::vector<std::uint8_t>(4)},{A,std::vector<std::uint8_t>(0x1000)},{C,std::vector<std::uint8_t>(0x40)},{T,std::vector<std::uint8_t>(0x4000)},{F,std::vector<std::uint8_t>(0x30)}};
 const auto ptr=[&](U at,U width)->std::uint8_t*{for(auto& s:spans)if(at>=s.at&&std::uint64_t(at-s.at)+width<=s.bytes.size())return s.bytes.data()+at-s.at;throw std::runtime_error("Heap fixture address is unowned");};
 const auto put=[&](U at,U n){auto* p=ptr(at,4);for(U i=0;i<4;++i)p[i]=std::uint8_t(n>>(i*8));};const auto get=[&](U at){const auto* p=ptr(at,4);U n=0;for(U i=0;i<4;++i)n|=U(p[i])<<(i*8);return n;};
 for(U i=0;i<0x1000;++i)*ptr(A+i,1)=std::uint8_t(i*37+11);for(U i=0;i<0x30;++i)*ptr(F+i,1)=0xa5;
 put(0x5013a0,C);put(0xcc0870,(flags&16)?0:0x1000);
 for(U ctx:{C,C+0x20}){put(ctx,A);put(ctx+4,(flags&1)?0x180:0x1000);put(ctx+8,(flags&32)?0:A);put(ctx+0xc,(flags&8)?0xffffffff:0x1000023);put(ctx+0x10,(flags&2)?T:0);}
 const std::array<U,4> nodes{A,A+0x40,A+0xa0,A+0x140};const U lengths[]{0,0x20,0x40,0x10};for(U i=0;i<4;++i){put(nodes[i],lengths[i]);put(nodes[i]+4,100+i);put(nodes[i]+8,200+i);put(nodes[i]+0xc,i<3?nodes[i+1]:0);}
 const U payload=nodes[selector%4]+0x10;for(U i:{0u,1u,2047u}){put(T+i*8,i==2047?A+0xb0:payload);put(T+i*8+4,CB+(i==2047?8:i*4));}
 put(F,size);put(F+4,align);put(F+8,mode==2?payload:cached);put(F+0x20,size);put(F+0x24,align);put(F+0x28,0xfedcba98);put(F+0x2c,label);
 U reads=0,writes=0,calls=0,pointer_reads=0;bool failed=false;std::ostringstream trace;MissionPeriodicBindings b;
 b.read=[&](U at,U width){if(++reads==fail_read){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read stopped");}U n;try{n=get(at);}catch(const std::exception&){trace<<'F'<<at<<','<<width<<';';throw;}trace<<'R'<<at<<','<<width<<','<<n<<';';
  if((mutation&1)&&at==0x5013a0)put(at,n==C?C+0x20:C);if((mutation&2)&&at==F+0x20)put(at,size^0x10);if((mutation&4)&&at==A+0xc)put(at,A+0xa0);if((mutation&16)&&at==T+4&&++pointer_reads==1)put(at,CB+4);return n;};
 b.write=[&](U at,U width,U n){if(++writes==fail_write){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write stopped");}try{put(at,n);}catch(const std::exception&){trace<<'E'<<at<<','<<width<<','<<n<<';';throw;}trace<<'W'<<at<<','<<width<<','<<n<<';';if((mutation&8)&&at>=A&&at<A+0x1000)put(F+0x2c,label^0xabcdef01);if((mutation&64)&&mode==2&&at>=A&&at<A+0x1000)put(F+8,A+0xb0);};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(++calls==fail_call)throw std::runtime_error("Child stopped");if(site==0x49a620){std::fill_n(ptr(args.at(0),args.at(2)),args.at(2),std::uint8_t(args.at(1)));if(mutation&256)put(0x5013a0,C+0x20);return 0;}if(site==0x49dc80){if(mutation&128)put(0xcc0870,0x800);return flags&4?0:A;}if(site>=CB&&site<=CB+8&&(site-CB)%4==0){if(mutation&32){put(T+2047*8,args.at(0));put(T+2047*8+4,CB+8);}return 0;}throw std::runtime_error("Callback is unowned");};
 U result=0;try{switch(mode){case 0:result=mission_heap_initialize(b);break;case 1:result=mission_heap_allocate(b,F,incoming);break;case 2:result=mission_heap_release(b,F);break;case 3:result=mission_heap_find_previous(b,nodes[selector%4]);break;case 4:mission_heap_notify_free(b,payload);break;case 5:result=mission_heap_find_hole(b,A,A+selector,size,F);break;default:throw std::runtime_error("Heap mode is unknown");}}catch(const std::exception&){failed=true;result=0;}
 U a=1,c=0;for(const auto& span:spans)for(U i=0;i<span.bytes.size();++i){if(span.at==F&&((mode==1&&i>=12&&i<32)||(mode==2&&(i<8||i>=12))))continue;const auto byte=span.bytes[i];a=(a+byte)%65521;c=(c+a)%65521;}std::cout<<failed<<' '<<result<<' '<<((c<<16)|a)<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
