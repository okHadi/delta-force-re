#include "dfre/mission_object_seagull.hpp"
#include "dfre/projectiles.hpp"
#include "dfre/world_hits.hpp"
#include "dfre/actor_queries.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U A=0x7dde80,D=0x1300000,F=0x1302000;constexpr std::pair<U,U> regions[]={{A,4*0x114},{D,0x278},{F,28},{0x67f528,4},{0xbd0060,4}};}
int main(){try{U mode,slot,mutation;std::string input;while(std::cin>>mode>>slot>>mutation>>input){
 std::unordered_map<U,std::uint8_t> bytes;std::size_t cursor=0;for(auto region:regions)for(U i=0;i<region.second;++i){bytes[region.first+i]=std::uint8_t(std::stoul(input.substr(cursor,2),nullptr,16));cursor+=2;}
 const auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i){auto found=bytes.find(at+i);if(found==bytes.end()){if(at+i>=A&&at+i<A+1024*0x114)continue;throw MissionTickError("Fixture address is not owned");}n|=U(found->second)<<(i*8);}return n;};const auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};
 const U actor=A+slot*0x114;std::ostringstream trace;unsigned calls=0,stores=0;MissionObjectSeagullBindings b;
 b.read=[&](U at,U width){U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&1)&&at==actor+0x72)put(actor+4,D+0x13c);if((mutation&2)&&at==actor+0x7c)put(actor+0x88,0x80000000);return n;};
 b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,n,width);++stores;if((mutation&4)&&at==actor+0xec)put(A+0x24,get(A+0x24)+0x123456);if((mutation&8)&&at==actor+0x30)put(actor+0x7c,0x10000);if((mutation&16)&&at==actor+0xe0)put(actor+0xe8,1);if((mutation&2048)&&stores==((mutation>>8)%9+1))throw MissionTickError("Fixture store stopped");};
 b.call=[&](U site,const std::vector<U>& args)->U{trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';++calls;if((mutation&4096)&&calls==((mutation>>8)%5+1))throw MissionTickError("Fixture body stopped");
  if(site==0x477860){BehaviorState random;random.rng=get(0xbd0060);const U draw=behavior_rng_next(random);put(0xbd0060,random.rng);if(mutation&32){put(actor+0x24,123);put(actor+0x28,456);put(actor+0x2c,789);}return draw;}
  if(site==0x43e7d0){if(mutation&64){put(actor+0xf0,0x80000000);put(actor+0x3c,0x7fffffff);put(actor+0x6d,0xab,1);}return 0;}
  if(site==0x477820){const U index=args.at(0)>>21;put(args.at(1),U(kSineTableQ22.at(index)));put(args.at(2),U(kSineTableQ22.at(index+512)));if(mutation&128){put(actor+0x7c,0x80000000);put(actor+0x6d,0xf0,1);}return 0;}
  if(site==0x477670){if(mutation&256){put(A+0x30,0xabcdef01);put(A+0x7c,0x80000000);put(F+0x18,0x30000);}return collision_approximate_length(args.at(0),args.at(1),args.at(2));}
  if(site==0x477770){if(mutation&512){put(actor+0x30,0x7fffffff);put(F+0x18,0x50000);}return query_angle_477770(std::int32_t(args.at(0)),std::int32_t(args.at(1)));}
  if(site==0x47df40){if(mutation&1024){put(actor+0x2c,0x80000000);put(actor+0xf0,0xffffe000);}return get(D+0x100);}
  throw MissionTickError("Fixture body is not owned");
 };
 bool failed=false;U result=0;try{if(!mode)mission_object_seagull_ai(b,actor,F);else if(mode==1)mission_object_seagull_move(b,actor);else if(mode==2)result=mission_seagull_flock_first(b,actor);else result=mission_seagull_flock_index(b,actor);}catch(const std::exception&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto region:regions)for(U i=0;i<region.second;++i){hash^=bytes.at(region.first+i);hash*=1099511628211ull;}
 std::cout<<failed<<' '<<result<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
