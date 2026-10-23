#include "dfre/mission_peer_leave.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U mode,seed,flags,mutation,fail_call,fail_read,fail_write,remaining;std::istringstream in(line);if(!(in>>mode>>seed>>flags>>mutation>>fail_call>>fail_read>>fail_write>>remaining))throw std::runtime_error("Short peer leave fixture");
 constexpr U F=0x1300000,C=0x6e9a90,S=0x6e8090;struct Span{U at;std::vector<std::uint8_t> bytes;};std::vector<Span> spans{{0x67f520,std::vector<std::uint8_t>(0x18)},{0x6e5588,std::vector<std::uint8_t>(8)},{S,std::vector<std::uint8_t>(0x1a00)},{C,std::vector<std::uint8_t>(0xa53c4)},{F,std::vector<std::uint8_t>(0xce)}};
 for(auto& span:spans)for(U i=0;i<span.bytes.size();++i)span.bytes[i]=std::uint8_t((i*37)^(seed>>((i%4)*8)));
 const auto ptr=[&](U at,U width)->std::uint8_t*{for(auto& s:spans)if(at>=s.at&&std::uint64_t(at-s.at)+width<=s.bytes.size())return s.bytes.data()+at-s.at;throw std::runtime_error("Unmapped fixture");};
 const auto put=[&](U at,U width,U n){auto* p=ptr(at,width);for(U i=0;i<width;++i)p[i]=std::uint8_t(n>>(i*8));};const auto get=[&](U at,U width){const auto* p=ptr(at,width);U n=0;for(U i=0;i<width;++i)n|=U(p[i])<<(i*8);return n;};
 put(0x67f534,4,flags&1);put(0x6e558c,4,(flags>>1)&1);put(0x67f528,4,(flags>>2)&1);put(0x67f520,4,(flags>>3)&1);put(0x6e5588,4,(flags>>4)&1);
 for(U i=0;i<17;++i){put(C+i*0x9b84+0x18c,1,(seed+i)%3!=0);put(C+i*0x9b84+0x1c4,2,(seed+i)%6);}for(U i=0;i<64;++i)put(S+i*0x68+0x40,4,(i+seed)%3?0x80000000u+i:0);
 U calls=0,reads=0,writes=0,result=0;bool failed=false;std::ostringstream trace;MissionPeriodicBindings b;
 b.read=[&](U at,U width){if(++reads==fail_read){trace<<'F'<<at<<','<<width<<';';throw std::runtime_error("Read stopped");}const U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
 b.write=[&](U at,U width,U n){if(++writes==fail_write){trace<<'E'<<at<<','<<width<<','<<n<<';';throw std::runtime_error("Write stopped");}put(at,width,n);trace<<'W'<<at<<','<<width<<','<<n<<';';};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(++calls==fail_call)throw std::runtime_error("Child stopped");
  if(site==0x49a620){std::fill_n(ptr(args.at(0),args.at(2)),args.at(2),std::uint8_t(args.at(1)));if(args[0]==F&&(mutation&16)){put(0x67f534,4,1);put(0x6e558c,4,0);}}
  else if(site==0x4403c0){put(F+1,1,get(F+1,1)+1);put(F+3,4,args.at(1));if(mutation&1)put(args[1]+0x1c4,2,1);if(mutation&2)put(args[1]+0x1c4,2,0);if((mutation&4)&&args[1]+0x9b84<C+0xa53c4)put(args[1]+0x9b84+0x18c,1,1);if(mutation&32)put(F+2,1,get(F+2,1)^1);}
  else if(site==0x441750){if(mutation&8)put(args.at(0)+0x1c4,2,1);}
  else if(site==0x460e40){if(mutation&1)put(0x6e5588,4,get(0x6e5588,4)^1);return remaining?--remaining+1:0;}
  else if(site==0x442ae0){if(mutation&2){put(0x67f528,4,0);put(0x67f520,4,0);}if(mutation&4)put(0x6e5588,4,0);}
  else if(site!=0x4421d0&&site!=0x490880)throw std::runtime_error("Unowned child");return 0;};
 try{if(mode)result=mission_peer_leave_drain(b);else mission_peer_reset(b,F);}catch(const std::exception&){failed=true;result=0;}
 U a=1,c=0;for(const auto& span:spans)for(auto byte:span.bytes){a=(a+byte)%65521;c=(c+a)%65521;}std::cout<<failed<<' '<<result<<' '<<((c<<16)|a)<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
