#include "dfre/mission_audio_frame.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/world_hits.hpp"
#include <algorithm>
#include <stdexcept>
#include <limits>
namespace dfre {
namespace {
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::uint32_t ab(std::uint32_t n){return n&0x80000000u?0u-n:n;}
template<class F> const F& need(const F& f,const char* text){if(!f)throw std::runtime_error(text);return f;}
constexpr std::array<std::uint32_t,256> attenuation_table{
    255,238,223,210,199,189,180,172,164,158,152,146,141,136,132,127,
    124,120,116,113,110,107,104,101,99,96,94,91,89,87,85,83,
    81,80,78,76,75,73,72,70,69,67,66,65,63,62,61,60,
    59,58,57,56,55,54,53,52,51,50,49,48,47,47,46,45,
    44,43,43,42,41,41,40,39,39,38,38,37,36,36,35,35,
    34,34,33,33,32,32,31,31,30,30,29,29,28,28,27,27,
    27,26,26,25,25,25,24,24,24,23,23,23,22,22,22,21,
    21,21,20,20,20,19,19,19,19,18,18,18,18,17,17,17,
    17,16,16,16,16,15,15,15,15,14,14,14,14,14,13,13,
    13,13,13,12,12,12,12,12,11,11,11,11,11,11,10,10,
    10,10,10,10,10,9,9,9,9,9,9,9,8,8,8,8,
    8,8,8,7,7,7,7,7,7,7,7,7,6,6,6,6,
    6,6,6,6,6,5,5,5,5,5,5,5,5,5,5,5,
    4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,
};
}
std::uint32_t mission_audio_attenuation(std::int32_t distance,std::int32_t range,std::uint8_t scale,std::uint8_t ceiling){
    std::uint32_t gain=0;
    if(distance<=range){
        if(!range)throw std::runtime_error("Audio attenuation divides by zero");
        const auto quotient=(std::int64_t(distance)*65536)/range;
        if(quotient<std::numeric_limits<std::int32_t>::min()||quotient>std::numeric_limits<std::int32_t>::max())throw std::runtime_error("Audio attenuation divide overflows");
        const auto value=std::uint32_t(quotient);
        if(!(value&0xffff0000u))gain=attenuation_table[value>>8];
    }
    const auto result=(std::uint32_t(scale)*gain)>>8;
    return si(result)<std::int32_t(ceiling)?result:ceiling;
}
void mission_audio_frame(MissionAudioFrameCells& c,const MissionAudioFrameBindings& b){
    ++c.counter_4f7ea8;if(si(c.counter_4f7ea8)<8)return;c.counter_4f7ea8=0;
    auto actor=[&](std::uint32_t handle,std::uint32_t offset){return need(b.actor_word,"Audio scheduler needs current actor words")(handle,offset);};
    auto definition=[&](std::uint32_t handle,std::uint32_t offset){return need(b.definition_word,"Audio scheduler needs loaded sound definitions")(handle,offset);};
    std::size_t count=0;
    for(std::size_t i=0;i<c.registrations_6e0700.size();++i){
        auto& r=c.registrations_6e0700[i];if(!r.definition)continue;
        if(!actor(r.actor,0)){r={};continue;}
        const auto dx=ab(actor(r.actor,0x24)-std::uint32_t(c.listener_6e06a8.x));
        if(si(dx)>si(definition(r.definition,0x1c)))continue;
        const auto dy=ab(actor(r.actor,0x28)-std::uint32_t(c.listener_6e06a8.y));
        if(si(dy)>si(definition(r.definition,0x1c)))continue;
        const auto dz=ab(actor(r.actor,0x2c)-std::uint32_t(c.listener_6e06a8.z));
        const auto distance=collision_approximate_length(si(dx),si(dy),si(dz));
        if(distance>si(definition(r.definition,0x1c)))continue;
        auto& candidate=c.candidates_6e0270[count];candidate.registration=std::uint32_t(i);
        const auto handle=r.definition;
        candidate.volume=mission_audio_attenuation(distance,si(definition(handle,0x1c)),std::uint8_t(definition(handle,0x24)),std::uint8_t(definition(handle,0x25)));
        if(++count==64)break;
    }
    const auto selected_count=std::min(count,std::size_t(6));
    if(count>6)for(std::size_t i=0;i<6;++i)for(std::size_t j=i+1;j<count;++j)
        if(si(c.candidates_6e0270[i].volume)<si(c.candidates_6e0270[j].volume))std::swap(c.candidates_6e0270[i],c.candidates_6e0270[j]);
    for(std::size_t i=0;i<selected_count;++i){
        const auto& candidate=c.candidates_6e0270[i];const auto& r=c.registrations_6e0700[candidate.registration];auto& s=c.selected_6e0470[i];
        s.identity=r.identity;s.played=0;s.registration=candidate.registration;s.volume=candidate.volume;s.sample=definition(r.definition,0x18);
        s.rate=actor(r.actor,0x20);if(!s.rate)s.rate=0x10000;
        const auto angle=query_angle_477770(si(actor(r.actor,0x24)-std::uint32_t(c.listener_6e06a8.x)),si(actor(r.actor,0x28)-std::uint32_t(c.listener_6e06a8.y)));
        s.pan=255u-((c.yaw_6e06b4-angle)>>24);
    }
    for(std::size_t i=0;i<6;++i)if(c.handles_6e06e8[i]){
        std::size_t match=0;while(match<selected_count&&c.identities_6e06d0[i]!=c.selected_6e0470[match].identity)++match;
        if(match<selected_count){
            const auto& s=c.selected_6e0470[match];need(b.update,"Audio scheduler needs its backend update")(c.handles_6e06e8[i],s.pan,s.volume,s.rate);c.selected_6e0470[match].played=1;
        }else{need(b.stop,"Audio scheduler needs its backend stop")(c.handles_6e06e8[i]);c.handles_6e06e8[i]=0;}
    }
    for(std::size_t i=0;i<selected_count;++i)if(!c.selected_6e0470[i].played){
        std::size_t free=0;while(free<6&&c.handles_6e06e8[free])++free;
        if(free<6){
            const auto& s=c.selected_6e0470[i];c.identities_6e06d0[i]=s.identity;
            const auto handle=need(b.start,"Audio scheduler needs its backend start")(s.sample,s.pan,s.volume,s.rate,std::uint32_t(10+i));c.handles_6e06e8[i]=handle;
        }
    }
}
} // namespace dfre
