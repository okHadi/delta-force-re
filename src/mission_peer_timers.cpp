#include "dfre/mission_peer_timers.hpp"
#include <limits>
#include <stdexcept>
namespace dfre {
namespace {
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
template<class F> const F& need(const F& f,const char* s){if(!f)throw std::runtime_error(s);return f;}
std::uint32_t absolute(std::uint32_t n){return si(n)<0?0u-n:n;}
std::uint32_t divide(std::uint32_t n,std::uint32_t d){const auto q=std::int64_t(si(n))*65536/si(d);if(q<std::numeric_limits<std::int32_t>::min()||q>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("Peer timer signed division overflow");return std::uint32_t(q);}
}
std::uint32_t mission_peer_expiry_height(std::uint32_t height,std::uint16_t control){
    const auto pc=(control>>8)&3u,mode=(control>>10)&3u;if(pc==1)throw std::runtime_error("Peer timer x87 precision is reserved");
    const unsigned precision=pc==0?24:pc==2?53:64;
    // Exact source double4CC6B4:6553.600000000000363797880709171295166015625.
    constexpr std::uint64_t denominator=std::uint64_t(1)<<39,fraction=329853488333;
    auto integer=std::int64_t(si(height))+6553;auto part=fraction;
    std::uint64_t magnitude=std::uint64_t(integer<0?-integer:integer);int exponent=-1;
    if(magnitude){exponent=0;auto bits=magnitude;while(bits>>=1)++exponent;if(integer<0&&(magnitude&(magnitude-1))==0)--exponent;}
    const int gap=exponent-int(precision)+1;const bool negative=integer<0;
    const auto increment=[&](std::uint64_t quotient,std::uint64_t remainder,std::uint64_t step){
        if(!remainder)return false;if(mode==0)return remainder>step/2||(remainder==step/2&&(quotient&1));
        return mode==2||(mode==3&&negative);
    };
    if(gap>=0){
        const auto step=std::int64_t(1)<<gap;auto q=integer/step,rem=integer%step;if(rem<0){--q;rem+=step;}
        // A nonzero source fraction makes an integer half-way boundary larger.
        bool up=false;if(mode==0)up=rem>=step/2;else up=mode==2||(mode==3&&negative);
        if(step==1&&mode==0)up=true; // fraction is greater than one half
        if(up)++q;integer=q*step;part=0;
    }else if(gap>=-39){
        const auto step=std::uint64_t(1)<<(39+gap);auto q=part/step;const auto rem=part%step;
        if(increment(q,rem,step))++q;part=q*step;if(part==denominator){++integer;part=0;}
    }
    if(integer<0&&part)++integer;
    if(integer<std::numeric_limits<std::int32_t>::min()||integer>std::numeric_limits<std::int32_t>::max())return 0x80000000u;
    return std::uint32_t(integer);
}
void mission_peer_timers(const MissionPeerTimerBindings& b){
    const auto& read=need(b.read,"Peer timer cells are not bound");const auto& write=need(b.write,"Peer timer stores are not bound");
    for(std::uint32_t offset=0;offset!=0x100;offset+=0x10){const auto timer=0x6e5784+offset;
        const auto count=read(timer);if(si(count)<=0)continue;write(timer,count-1);
        if(si(read(0x6e5880))>0){
            const auto dx=read(0x6e5878)-read(0x6e577c+offset),dy=read(0x6e587c)-read(0x6e5780+offset);
            const auto ax=absolute(dx),ay=absolute(dy);const auto low=si(ax)<=si(ay)?ax:ay,high=si(ax)<=si(ay)?ay:ax;
            const auto distance=std::uint32_t(si(low)>>1)+high;
            if(distance){const auto x=read(0x6e577c+offset)+divide(dx,distance);write(0x6e577c+offset,x);write(0x6e5780+offset,read(0x6e5780+offset)+divide(dy,distance));}
        }
        if(read(timer)==0){
            MissionPeerExpiryPosition position{read(0x6e577c+offset),read(0x6e5780+offset),0,0,0,0,0};
            position[6]=need(b.height,"Peer expiry terrain height is not bound")(position[0],position[1]);
            const auto id=read(0x6e5778+offset);
            position[2]=mission_peer_expiry_height(position[6],need(b.fp_control,"Peer expiry x87 control is not bound")());
            need(b.expire,"Peer expiry handler is not bound")(id,position);
        }
    }
    write(0x6e5880,read(0x6e5880)-1);
}
}
