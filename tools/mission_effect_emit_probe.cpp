#include "dfre/mission_effect_emit.hpp"
#include "dfre/actor_behavior.hpp"
#include <array>
#include <iostream>
#include <sstream>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U F=0x1304000,G=0x1306000,D=0x1302000,T=0x8e44b0,C=0xc23810;constexpr std::pair<U,U> regions[]={{F,0x114},{G,12},{D,0x4f0},{T,68},{C,4}};}
int main(){try{U mode,mutation,seed;std::string input;while(std::cin>>mode>>mutation>>seed>>input){
 std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto region:regions)for(U i=0;i<region.second;++i){bytes[region.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};const auto put=[&](U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};
 std::ostringstream trace;BehaviorState random;random.rng=seed;unsigned draws=0,loads=0;U definition=D;MissionEffectEmitBindings b;
 b.read=[&](U at,U width){U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';return n;};
 b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,n);if((mutation&32)&&at==F+0x110)put(definition+0x90,0x87654321);if((mutation&512)&&at==F+0x7c)throw MissionEffectEmitError("Fixture store stopped");if(mode&&(mutation&2)&&at>=T&&at<T+68)put(at,n+77);};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';
  if(site==0x4828c0){if(mutation&128)throw MissionEffectEmitError("Fixture lookup stopped");const U index=mode?(args.at(0)^seed)&1023:seed&1;if(!mode){put(C,mutation&1024?F-index*0x13c:mutation&1?D+0x13c:D);definition=get(C)+index*0x13c;}return index;}
  if(site==0x49a620){if(mutation&256)throw MissionEffectEmitError("Fixture clear stopped");for(U i=0;i<0x114;++i)bytes.at(F+i)=0;if(mutation&2){put(G,get(G)+111);put(G+4,get(G+4)+222);put(G+8,get(G+8)+333);put(definition+0x6c,0x777);}if(mutation&4)put(C,D);return F;}
  if(site==0x477860){++draws;if((mutation&16)&&draws==2)throw MissionEffectEmitError("Fixture random stopped");if(mutation&8){put(definition+0x84,0x12345678);put(definition+0x90,0x23456789);put(F+0x24,0x3456789a);put(G+8,999);}return behavior_rng_next(random);}
  if(site==0x45d970){if(mutation&64)throw MissionEffectEmitError("Fixture allocator stopped");return 0;}
  if(site==0x45d6f0){++loads;if((mutation&4)&&loads==3)throw MissionEffectEmitError("Fixture resource stopped");if(mutation&1)put(T+64,args.at(0)+100);return 0;}
  throw MissionEffectEmitError("Fixture body is not owned");
 };
 bool failed=false;try{if(mode)mission_effect_definitions_initialize(b);else mission_effect_emit(b,G,F,0x114);}catch(const MissionEffectEmitError&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto region:regions)for(U i=0;i<region.second;++i){hash^=bytes.at(region.first+i);hash*=1099511628211ull;}
 std::cout<<failed<<' '<<random.rng<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
