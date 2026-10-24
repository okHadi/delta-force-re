#include "animation_head_fixture.hpp"
#include <iostream>
#include <stdexcept>
namespace {
using U=std::uint32_t;
void check(bool n,const char* text){if(!n)throw std::runtime_error(text);}
std::int32_t wrap(U n){n<<=7;return n<0x80000000u?std::int32_t(n>>7):-std::int32_t((std::uint64_t(0x100000000ull-n)+127)/128);}
void run(U duration,U pitch,U last_y){std::istringstream in(animation_head_fixture::run(duration,pitch,last_y));U failed,ticks;in>>failed>>ticks;std::array<U,18> delta{};for(auto& v:delta)in>>v;check(ticks==duration,"Current frame duration");const auto divisor=duration==1?wrap(last_y<<17):std::int32_t(duration);check(failed==U(divisor==0),"Source head division fault");check(delta[0]==(1u<<17)/duration&&delta[6]==(2u<<17)/duration&&delta[12]==(3u<<17)/duration,"Earlier pose prefix");if(!divisor){check(delta[13]==0,"Head fault retains last Y delta");return;}const auto head=wrap(pitch<<17)/divisor;const auto signed_pitch=pitch<128?std::int32_t(pitch):std::int32_t(pitch)-256;const auto sar=signed_pitch>=0?signed_pitch/4:-((-signed_pitch+3)/4);const auto body=wrap((U(sar)&255)<<17)/divisor;check(delta[13]==U(head),"Head uses source divisor");const auto original_y=duration==1?3u<<17:U(std::int32_t(3u<<17)/std::int32_t(duration));check(delta[7]==original_y+U(body),"Signed byte pitch shift");}
}
int main(){try{for(U duration:{1u,2u,3u,65535u})for(U pitch:{0u,1u,127u,128u,129u,252u,255u})for(U y:{0u,1u,127u,128u,255u})run(duration,pitch,y);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
