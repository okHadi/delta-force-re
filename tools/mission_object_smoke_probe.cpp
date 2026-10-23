#include "dfre/mission_object_smoke.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U A=0x822e80,O=0x7dde80;}
int main(){try{U mode,mutation,seed;std::string one,two;while(std::cin>>mode>>mutation>>seed>>one>>two){
 std::array<std::uint8_t,0x114> actor{},owner{};for(U i=0;i<0x114;++i){actor[i]=std::uint8_t(std::stoul(one.substr(i*2,2),nullptr,16));owner[i]=std::uint8_t(std::stoul(two.substr(i*2,2),nullptr,16));}
 const auto get=[&](U at,U width){const auto& bytes=at>=A&&std::uint64_t(at-A)+width<=actor.size()?actor:at>=O&&std::uint64_t(at-O)+width<=owner.size()?owner:throw MissionTickError("Fixture read has no owner");const auto base=&bytes==&actor?A:O;U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at-base+i))<<(i*8);return n;};
 const auto put=[&](U at,U width,U n){auto& bytes=at>=A&&std::uint64_t(at-A)+width<=actor.size()?actor:at>=O&&std::uint64_t(at-O)+width<=owner.size()?owner:throw MissionTickError("Fixture write has no owner");const auto base=&bytes==&actor?A:O;for(U i=0;i<width;++i)bytes.at(at-base+i)=std::uint8_t(n>>(i*8));};
 BehaviorState random;random.rng=seed;std::ostringstream trace;MissionObjectSmokeBindings b;bool velocity_read=false;
 b.read=[&](U at,U width){U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if(!velocity_read&&(mutation&32)&&at==A+0x40){velocity_read=true;put(A+0x40,4,0x12345678);}return n;};
 b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);
  if((mutation&64)&&at==A+0x24){put(A+0x28,4,777);put(A+0x2c,4,888);put(A+0x7c,4,12345);put(A+0x44,4,255);}
  if((mutation&128)&&at==A+0x7c)put(A+0x7c,4,0x30000);
  if((mutation&256)&&at==A+0x3b){put(A+0x7c,4,0x34567);put(A+0x40,4,19);put(A+0x44,4,29);}
  if((mutation&16)&&at==(mode?A+0x24:A+0x7c))throw MissionTickError("Fixture store stopped");
 };
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';
  if(site==0x42a440){if(mutation&1)put(A+0x7c,4,99);if(mutation&2){put(A+0xf8,4,O);put(A+0x24,4,111);}if(mutation&512)throw MissionTickError("Fixture emitter stopped");return 0u;}
  if(site==0x45da30){if(mutation&4)put(A+0xfc,4,123);return 0u;}
  if(site==0x477860){if(mutation&8)throw MissionTickError("Fixture random stopped");if(mutation&4)put(A+0xfc,4,456);return behavior_rng_next(random);}
  throw MissionTickError("Fixture body is not owned");
 };
 bool failed=false;try{if(mode)mission_object_smoke_move(b,A);else mission_object_smoke_generator(b,A);}catch(const MissionTickError&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto* bytes:{&actor,&owner})for(auto n:*bytes){hash^=n;hash*=1099511628211ull;}
 std::cout<<failed<<' '<<random.rng<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
