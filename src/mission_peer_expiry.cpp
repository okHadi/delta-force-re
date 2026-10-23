#include "dfre/mission_peer_expiry.hpp"
#include <limits>
#include <stdexcept>
namespace dfre {
namespace {
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
template<class F> const F& need(const F& f,const char* s){if(!f)throw std::runtime_error(s);return f;}
}
std::uint32_t mission_peer_request_height(std::uint32_t height,std::uint16_t control){
    const auto pc=(control>>8)&3u,mode=(control>>10)&3u;if(pc==1)throw std::runtime_error("Peer timer x87 precision is reserved");
    const unsigned precision=pc==0?24:pc==2?53:64;
    // Exact source double4CC6AC:13107.20000000000072759576141834259033203125.
    constexpr std::uint64_t denominator=std::uint64_t(1)<<39,fraction=109951162778;
    auto integer=std::int64_t(si(height))+13107;auto part=fraction;
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
        if(step==1&&mode==0)up=false; // fraction is less than one half
        if(up)++q;integer=q*step;part=0;
    }else if(gap>=-39){
        const auto step=std::uint64_t(1)<<(39+gap);auto q=part/step;const auto rem=part%step;
        if(increment(q,rem,step))++q;part=q*step;if(part==denominator){++integer;part=0;}
    }
    if(integer<0&&part)++integer;
    if(integer<std::numeric_limits<std::int32_t>::min()||integer>std::numeric_limits<std::int32_t>::max())return 0x80000000u;
    return std::uint32_t(integer);
}
void mission_peer_expiry(const MissionPeerExpiryBindings& b,std::uint32_t actor,MissionPeerExpiryRecord record){
    if(!need(b.enabled_67f528,"Peer expiry host gate is not bound")())return;
    if(!record.words||record.count<4)throw std::runtime_error("Peer expiry position is incomplete");
    std::array<std::uint8_t,18> packet;
    const auto put=[&](unsigned at,std::uint32_t n,unsigned width){for(unsigned i=0;i<width;++i)packet[at+i]=std::uint8_t(n>>(8*i));};
    put(2,record.words[0],4);put(6,record.words[1],4);
    put(10,mission_peer_request_height(record.words[2],need(b.fp_control,"Peer expiry x87 control is not bound")()),4);
    put(14,record.words[3]>>16,2);put(0,(actor-0x7cca80u)/0x114u,2);packet[16]=2;packet[17]=200;
    need(b.enqueue,"Peer expiry outbox is not bound")(0x27,packet.data(),packet.size());
    need(b.apply,"Peer expiry apply handler is not bound")(actor,record,200);
}
}

namespace dfre {
void mission_peer_expiry_effect(const MissionPeerExpiryEffectBindings& b,std::uint32_t actor,MissionPeerExpiryRecord input,std::int32_t count,std::uint32_t tail){
    if(!input.words||input.count<3)throw std::runtime_error("Peer expiry effect position is incomplete");
    const auto& call=need(b.call,"Peer expiry effect calls are not bound");
    (void)call(0x477850,{0x5fc});
    (void)call(0x45dc70,{3,input.words[0],input.words[1],input.words[2]});
    (void)call(0x4657d0,{5,input.words[0],input.words[1],input.words[2],actor});
    (void)call(0x43e7d0,{0x32,input.words[0],input.words[1],input.words[2]});
    (void)call(0x45fcf0,{input.words[0],input.words[1],input.words[2],200,0,0});
    std::array<std::uint32_t,6> local{input.words[0],input.words[1],input.words[2],0,0,tail};
    for(std::int32_t i=0;i<count;++i){
        const auto angle=call(0x477860,{});const auto pitch=call(0x477860,{});
        if(input.count<4)throw std::runtime_error("Peer expiry effect angle is incomplete");
        local[4]=pitch<<14;local[3]=input.words[3]+(angle<<16);
        const auto speed=(call(0x477860,{})|0x100u)*30u;
        need(b.spawn,"Peer expiry fragment handler is not bound")(local,actor,speed);
    }
}
}
