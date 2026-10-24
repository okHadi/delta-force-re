#include "dfre/mission_profile_start.hpp"
#include "dfre/mission_results.hpp"
#include <array>
#include <cassert>
#include <map>
#include <stdexcept>
#include <vector>
using namespace dfre;using U=std::uint32_t;
int main(){std::map<U,U>w{{0x4cee90,17},{0x4cf2e4,0x1100000},{0x1100028,3},{0x110013c,0x80000420},{0x67f548,0x1234},{0x503328,7}};std::array<unsigned char,0x514> result;result.fill(0xff);MissionDebriefBindings b;
 b.read=[&](U at,U width){assert(width==4);auto n=w.at(at);if(at==0x4cf2e4)w[at]=0x1200000;return n;};b.write=[&](U at,U width,U n){assert(width==4);w[at]=n;};
 b.call=[&](U site,const auto& args){assert(site==0x49a620);assert((args==std::vector<U>{0x67f714,0,0x514}));result.fill(0);w[0x67f548]=0xabcdef01;return 0u;};
 mission_profile_start_mask(b);assert(w.at(0x67f70c)==0x80000420);assert(w.at(0x67f54c)==17);assert(mission_profile_start_result(b)==7);assert(w.at(0x67dbec)==0x80000420);assert(w.at(0x67dbe4)==0xabcdef01);for(auto x:result)assert(!x);
 b.call={};w[0x67dbec]=0;w[0x67dbe4]=9;bool failed=false;try{mission_profile_start_result(b);}catch(const std::runtime_error&){failed=true;}assert(failed&&w.at(0x67dbec)==0x80000420&&w.at(0x67dbe4)==9);
 // Wrapped campaign arithmetic reaches a caller-owned address.
 w[0x4cf2e4]=0x1100000;w[0x1100028]=0x40000000;w[0x1100070]=0xfedcba98;mission_profile_start_mask(b);assert(w.at(0x67f70c)==0xfedcba98);
 // A copied profile keeps its old mask on abort/loss and receives the live mask
 // only on success, after the real profile-leave end-event call.
 for(U status:{0u,2u,1u}){
  AssetBytes profile(0x800,0);MissionResultImage image{};
  auto put=[&](U offset,U n){for(U i=0;i<4;++i)profile.at(offset+i)=std::uint8_t(n>>(i*8));};
  auto get=[&](U offset){U n=0;for(U i=0;i<4;++i)n|=U(profile.at(offset+i))<<(i*8);return n;};
  put(0x28,3);put(0x13c,0x80000040);w[0x4cf2e4]=0x1100000;w[0x67f548]=0x1234;w[0x503328]=0;
  MissionDebriefBindings start=b;
  start.read=[&](U at,U width){assert(width==4);if(at>=0x1100000&&at<0x1100800)return get(at-0x1100000);return w[at];};
  start.call=[&](U site,const auto& args){assert(site==0x49a620);assert((args==std::vector<U>{0x67f714,0,0x514}));image.fill(0);return 0u;};
  mission_profile_start_mask(start);mission_profile_start_result(start);
  assert(w.at(0x67dbec)==0x80000040&&w.at(0x67dbe4)==0x1234);
  w[0x67f548]=0;w[0x67f548]=w.at(0x67dbe4); //404150 restore before401E70.
  w[0x67f528]=1;w[0x67f520]=0;w[0x67f718]=status;w[0x68ba34]=0;image[4]=std::uint8_t(status);
  bool end=false;MissionProfileBindings leave;
  leave.cell=[&](U at){return w[at];};leave.store=[&](U at,U n){w[at]=n;};leave.profile=[&]() ->AssetBytes&{return profile;};leave.result=[&]{return image;};leave.fp_control=[] {return std::uint16_t(0x37f);};leave.end_events=[&]{end=true;w[0x67dbec]|=8;};
  mission_profile_leave(leave);assert(end);assert(get(0x13c)==(status==1?0x80000048u:0x80000040u));assert(w.at(0x4cee8c)==0x1234);
 }
}
