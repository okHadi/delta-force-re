#include "dfre/mission_object_effects.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
int main(){try{std::string line;while(std::getline(std::cin,line)){
 U body,host,owner,count,result,mutation,fail;std::string hex;std::istringstream in(line);if(!(in>>body>>host>>owner>>count>>result>>mutation>>fail>>hex)||hex.size()!=0x228)throw std::runtime_error("Short object effect fixture");
 constexpr U A=0x7dde80,P=0x1200000,D=0x1210000,F=0x1300000;std::map<U,std::uint8_t> bytes;std::ostringstream trace;
 auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};
 auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};
 for(U i=0;i<0x114;++i)bytes[A+i]=std::uint8_t(std::stoul(hex.substr(i*2,2),nullptr,16));for(U i=0;i<24;++i)bytes[P+i]=bytes.at(A+0x24+i);for(U i=0;i<0x13c;++i)bytes[D+i]=std::uint8_t(i^0x6d);for(U i=0;i<0x140;++i)bytes[F+i]=0xa5;for(U global:{0x67f528u,0xc23810u})for(U i=0;i<4;++i)bytes[global+i]=0;
 put(A+4,D);put(0x67f528,host);put(0xc23810,D-2*0x13c);std::vector<std::pair<U,U>> ranges;
 if(body==0){put(F+0x1c,owner);put(F+0x20,P);put(F+0x24,count);ranges={{0,18},{0x1c,12}};}else if(body==1){for(U i=0;i<4;++i)put(F+0x38+i*4,i?get(P+(i-1)*4):owner);ranges={{0,43},{0x38,16}};}else if(body==2){put(F+0x2c,owner);put(F+0x30,P);put(F+0x34,count);ranges={{0,24},{0x2c,12}};}else if(body==3){for(U i=0;i<4;++i)put(F+0x120+i*4,i?get(P+(i-1)*4):owner);ranges={{0,0x114},{0x120,16}};}else throw std::runtime_error("Unknown object effect body");
 U calls=0,random=0;MissionObjectEffectBindings b;
 b.read=[&](U at,U width){auto n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};b.write=[&](U at,U width,U n){put(at,n,width);trace<<'W'<<at<<','<<width<<','<<n<<';';};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<';';if(++calls==fail)throw std::runtime_error("Fixture call stopped");if(site==0x49a620){for(U i=0;i<args.at(2);++i)bytes.at(args.at(0)+i)=std::uint8_t(args.at(1));if(mutation&4){if(body==1)put(A+0x24,0x44332211);if(body==3)put(F+0x124,0x55667788);}return args.at(0);}if(site==0x442630&&(mutation&1)){put(P,11);put(P+4,22);put(P+8,33);if(body==0){put(F+0x1c,0x10203040);put(F+0x24,2);}}if(site==0x45dc70&&(mutation&2)){put(P,44);put(P+4,55);put(P+8,66);put(P+12,0x77889900);}if(site==0x477850){random=args.at(0);return 0;}if(site==0x477860){random=random*1664525u+1013904223u;return random>>16;}if(site==0x460d20||site==0x45d970)return result?A:0;if(site==0x4828c0)return 2;return 0;};
 bool failed=false;U returned=0;try{if(body==0)mission_object_effect_request(b,F);if(body==1)mission_object_effect_publish(b,F);if(body==2)mission_object_effect_apply(b,F);if(body==3)returned=mission_object_effect_allocate(b,F);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;auto hash_range=[&](U at,U size){for(U i=0;i<size;++i){hash^=bytes.at(at+i);hash*=1099511628211ull;}};hash_range(A,0x114);hash_range(P,24);hash_range(D,0x13c);for(auto range:ranges)hash_range(F+range.first,range.second);std::cout<<failed<<' '<<returned<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
