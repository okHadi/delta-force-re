#include "dfre/mission_host_pump.hpp"
#include <limits>

namespace dfre {
namespace {
using U=std::uint32_t;
template<class F> const F& need(const F& f,const char* message) {
    if(!f) throw MissionHostPumpError(message);
    return f;
}
U word(const MissionHostPumpBindings& b,U a){return need(b.word,"Missing host word reader")(a);}
void put(const MissionHostPumpBindings& b,U a,U v){need(b.store,"Missing host word writer")(a,v);}
U call(const MissionHostPumpBindings& b,U a,std::vector<U> args={}){
    return need(b.call,"Missing host service binding")(a,args);
}
std::uint8_t byte(const MissionHostPumpBindings& b,U a){return need(b.byte,"Missing host byte reader")(a);}
void putbyte(const MissionHostPumpBindings& b,U a,std::uint8_t v){need(b.store_byte,"Missing host byte writer")(a,v);}
}
void mission_host_cd_upkeep(const MissionHostPumpBindings& b){
    if(word(b,0x5012e0)==0 || word(b,0x5012d4)==0 || word(b,0x5012d8)!=0 ||
       word(b,0x5012ec)>=word(b,0x49101c))return;
    const U callback=word(b,0x5012e4);
    if(callback)call(b,callback,{word(b,0x5012dc)});
    const U repeat=word(b,0x5012e0);
    if(repeat==0)return;
    const U track=word(b,0x5012dc);
    call(b,0x493cd0,{track,repeat});
}
void mission_host_pcm_upkeep(const MissionHostPumpBindings& b){
    if(!word(b,0x5012f8))return;
    for(;;){
        const U count=word(b,0xd08948);
        if(count!=0){
            const U index=word(b,0x5012f4);
            if(index!=0xffffffffu){
            if(!word(b,0xd08940) || !call(b,0x4b5243,{0xcc0930u+index*0x4000u,count,0}))return;
            }
        }
        const bool input16=word(b,0xd08954)==16;
        const U capacity=input16?0x8000u:0x4000u;
        const auto result=need(b.read_file,"Missing stream file reader")(word(b,0xd0894c),0xd00930,capacity);
        if(result.count>capacity)throw MissionHostPumpError("Stream read exceeds input buffer");
        if(!result.success || result.count==0){put(b,0xd08948,0);return;}
        const U output=input16?result.count>>1:result.count;
        if(result.count<capacity)need(b.seek_file,"Missing stream file seek")(word(b,0xd0894c),0x2c);
        const U next_index=(word(b,0x5012f4)+1u)&15u;
        put(b,0x5012f4,next_index);
        const U destination=0xcc0930u+next_index*0x4000u;
        if(word(b,0xd08954)==16){
            // Original pair loop cannot terminate for zero or odd output counts.
            if(output==0 || (output&1u))throw MissionHostPumpError("Original PCM pair loop cannot terminate");
            for(U i=0;i<output;i+=2){
                putbyte(b,destination+i,std::uint8_t(byte(b,0xd00931u+i*2u)^0x80u));
                putbyte(b,destination+i+1u,std::uint8_t(byte(b,0xd00933u+i*2u)^0x80u));
            }
        }else{
            for(U i=0;i<(output&~3u);++i)putbyte(b,destination+i,byte(b,0xd00930u+i));
        }
        put(b,0xd08948,output);
    }
}
void mission_host_midi_upkeep(const MissionHostPumpBindings& b){
    const U current=word(b,0x501950);
    if(!current)return;
    if(word(b,current+0x10u)==4 && word(b,0x501980)!=0 &&
       U(word(b,0x49101c)-word(b,0x501974))>0x960u){
        call(b,0x49efd0);call(b,0x49ef20);
    }
    if(word(b,0x50198c))put(b,0x50198c,0);
}
void mission_host_upkeep(const MissionHostPumpBindings& b){
    mission_host_cd_upkeep(b);mission_host_pcm_upkeep(b);mission_host_midi_upkeep(b);
}
bool mission_host_pump(const MissionHostPumpBindings& b){
    for(;;){
        const auto message=need(b.peek_remove,"Missing message reader")();
        if(!message){mission_host_upkeep(b);return true;}
        if(message->kind==0x12)return false;
        need(b.translate,"Missing message translator")(*message);
        need(b.dispatch,"Missing message dispatcher")(*message);
    }
}
void mission_host_clock_callback(const MissionHostPumpBindings& b){
    if(!word(b,0xcc0884) && !word(b,0xcc08bc))return;
    const U increment=word(b,0x491018),primary=word(b,0x491014);
    const U low=word(b,0x491020);
    put(b,0x491014,primary+increment);
    const U high=word(b,0x49101c);
    const U increment_low=word(b,0x491028),increment_high=word(b,0x491024);
    const U next=low+increment_low;
    put(b,0x491020,next);put(b,0x49101c,high+increment_high+U(next<low));
}
void mission_host_clock_configure(const MissionHostPumpBindings& b){
    const auto caps=need(b.timer_caps,"Missing timer capability reader")();
    const U minimum=caps[0]>1?caps[0]:1;
    const U chosen=minimum<caps[1]?minimum:caps[1];
    put(b,0x491010,chosen);call(b,0x4c658c,{chosen});
    const U requested=word(b,0xcc0868),user=word(b,0x49102c);
    const U handle=call(b,0x4c65a0,{requested?requested:25,0,0x4906a0,user,1});
    put(b,0x49100c,handle);
    if(requested){
        const U current=word(b,0xcc0868);
        const std::int64_t signed_period=current<=0x7fffffffu?std::int64_t(current):std::int64_t(current)-0x100000000LL;
        const std::int64_t quotient=0x78000000LL*signed_period/1000;
        if(quotient<std::numeric_limits<std::int32_t>::min() || quotient>std::numeric_limits<std::int32_t>::max())
            throw MissionHostPumpError("Original timer division overflows");
        const U q=U(quotient);
        put(b,0x491028,q<<8);put(b,0x491024,q>>24);
        put(b,0x491020,0);put(b,0x49101c,0);
    }
    put(b,0x491030,1);
}
MissionHostClockDelivery::MissionHostClockDelivery(U period_ms):period_(period_ms){
    if(!period_)throw MissionHostPumpError("Timer period must be positive");
}
void MissionHostClockDelivery::reset() noexcept{anchored_=false;remainder_=0;last_=0;}
U MissionHostClockDelivery::sample(U now,const MissionHostPumpBindings& b){
    if(!anchored_){last_=now;anchored_=true;return 0;}
    const std::uint64_t elapsed=std::uint64_t(U(now-last_))+remainder_;
    last_=now;remainder_=U(elapsed%period_);
    const U count=U(elapsed/period_);
    for(U i=0;i<count;++i)mission_host_clock_callback(b);
    return count;
}
} // namespace dfre
