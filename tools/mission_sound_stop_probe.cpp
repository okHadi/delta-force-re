#include "dfre/mission_sound_stop.hpp"
#include <array>
#include <iostream>
#include <sstream>
using namespace dfre;using U=std::uint32_t;
namespace {constexpr U A=0x7dde80,R=0x6e0700;}
int main(){try{U seed;while(std::cin>>seed){
 const U actor=seed%19?A:0,scale=seed%13?seed&255:0x100+(seed&255),mutation=seed%128;
 std::array<std::uint8_t,16384> rows{};std::array<std::uint8_t,0x114> image{};
 for(U i=0;i<rows.size();++i)rows[i]=std::uint8_t(seed+i*17);for(U i=0;i<image.size();++i)image[i]=std::uint8_t(seed+i*17);
 const auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i){std::uint8_t c;if(at+i>=R&&at+i<R+rows.size())c=rows[at+i-R];else if(at+i>=A&&at+i<A+image.size())c=image[at+i-A];else throw MissionSoundBankError("Fixture unowned read");n|=U(c)<<(8*i);}return n;};
 const auto put=[&](U at,U width,U n){for(U i=0;i<width;++i){auto* c=at+i>=R&&at+i<R+rows.size()?&rows[at+i-R]:at+i>=A&&at+i<A+image.size()?&image[at+i-A]:nullptr;if(!c)throw MissionSoundBankError("Fixture unowned write");*c=std::uint8_t(n>>(8*i));}};
 for(U i=0;i<1024;++i){put(R+i*16+4,4,i%11?A+0x114:0);put(R+i*16+12,1,(seed+i)&255);}
 const std::array<U,3> selected{seed%1024,(seed*7+511)%1024,1023};for(U j=0;j<3;++j){put(R+selected[j]*16+4,4,actor);put(R+selected[j]*16+12,1,(seed+j%2)&255);}
 put(A+0x1c,4,R+selected[seed%3]*16);std::ostringstream trace;MissionSoundBankBindings b;
 b.read=[&](U at,U width){const U n=get(at,width);trace<<'R'<<at<<','<<width<<','<<n<<';';if((mutation&64)&&at>=R&&at<R+16384&&(at-R)%16==4)put(at+8,1,scale&255);if((mutation&16)&&at==A+0x1c)throw MissionSoundBankError("Fixture read stopped");return n;};
 b.write=[&](U at,U width,U n){trace<<'W'<<at<<','<<width<<','<<n<<';';put(at,width,n);if(mutation&32)throw MissionSoundBankError("Fixture write stopped");};
 b.call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(U n:args)trace<<','<<n;trace<<';';if(site!=0x49a620||args.size()!=3)throw MissionSoundBankError("Fixture body");if(mutation&8)throw MissionSoundBankError("Fixture clear stopped");
  for(U i=0;i<16;++i)put(args[0]+i,1,0);
  if(mutation&1)put(A+0x1c,4,args[0]);if(mutation&2)put(A+0x1c,4,R+((args[0]-R+16)%16384));
  if((mutation&4)&&args[0]<R+16368){put(args[0]+20,4,actor);put(args[0]+28,1,scale&255);}return args[0];};
 bool failed=false;try{mission_sound_stop(b,actor,scale);}catch(const MissionSoundBankError&){failed=true;}
 std::uint64_t hash=1469598103934665603ull;for(auto n:rows){hash^=n;hash*=1099511628211ull;}for(auto n:image){hash^=n;hash*=1099511628211ull;}
 std::cout<<failed<<' '<<hash<<' '<<trace.str()<<'\n';
}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
