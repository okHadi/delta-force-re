#include "dfre/mission_sound_bank.hpp"
#include <limits>

namespace dfre {
namespace {
using U=std::uint32_t;
template<class F> const F& need(const F& fn,const char* text){if(!fn)throw MissionSoundBankError(text);return fn;}
U read(const MissionSoundBankBindings& b,U at,U width=4){return need(b.read,"Sound bank shared reader is required")(at,width);}
void put(const MissionSoundBankBindings& b,U at,U width,U value){need(b.write,"Sound bank shared writer is required")(at,width,value);}
U call(const MissionSoundBankBindings& b,U site,std::initializer_list<U> args){return need(b.call,"Sound bank reached body is required")(site,{args});}
std::int32_t si(U n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
U rol(U n,unsigned shift){return (n<<shift)|(n>>(32-shift));}
U product(U a,U c){return U(std::uint64_t(std::int64_t(si(a))*si(c))>>16);}
void clear(const MissionSoundBankBindings& b,U at,U bytes){for(U i=0;i<bytes;i+=4)put(b,at+i,4,0);}
U attenuation(const MissionSoundBankBindings& b,U distance,U range,U scale,U ceiling){
    U gain=0;
    if(si(distance)<=si(range)){
        if(!range)throw MissionSoundBankError("Sound bank attenuation divides by zero");
        const auto quotient=(std::int64_t(si(distance))*65536)/si(range);
        if(quotient<std::numeric_limits<std::int32_t>::min()||quotient>std::numeric_limits<std::int32_t>::max())throw MissionSoundBankError("Sound bank attenuation divides outside signed range");
        const auto value=U(quotient);if(!(value&0xffff0000u))gain=read(b,0x501504u+(value>>8)*4);
    }
    const auto value=U(std::uint64_t(scale)*gain)>>8;
    return si(value)<si(ceiling)?value:ceiling;
}
}
U mission_sound_bank_random(const MissionSoundBankBindings& b,U range){
    const auto before=read(b,0x5014fc);const auto next=rol(rol(before,11)+before,3);put(b,0x5014fc,4,next);
    return U(std::uint64_t(next&0xffu)*range)>>8;
}
bool mission_sound_bank_filter(const MissionSoundBankBindings& b,U flags){return !(flags&6u&read(b,0x501500));}
bool mission_sound_bank_quality(const MissionSoundBankBindings& b,U flags){return (flags&0x20u)!=(read(b,0x501500)&0x20u);}
U mission_sound_bank_play(const MissionSoundBankBindings& b,U bank,U params,U context){
    if(!bank)return 0;
    clear(b,mission_sound_bank_scratch,0xa0);
    U produced=0;
    for(U index=0;index<read(b,bank+0x24);++index){
        const auto layer=read(b,bank+0x28+index*4);
        if(!read(b,layer,2))continue;
        if(mission_sound_bank_filter(b,read(b,layer+8)))continue;
        if((read(b,bank+0x4c,1)&1u)&&mission_sound_bank_quality(b,read(b,layer+8)))continue;
        U variant=0;
        if(read(b,layer+8,1)&0x10u){
            const auto cursor=read(b,layer+2,2);const auto count=read(b,layer,2);
            if(cursor>=count)throw MissionSoundBankError("Sound bank round-robin index exceeds its variants");
            variant=read(b,layer+0x10+cursor*4);
            const auto next=std::uint16_t(cursor+1);put(b,layer+2,2,next);
            if(next>=count)put(b,layer+2,2,0);
        }else variant=read(b,layer+0x10+mission_sound_bank_random(b,read(b,layer,2))*4);
        const auto asset=read(b,variant);
        if(!asset)continue;
        if(!read(b,asset+0x23,1)){
            call(b,0x499120,{context+0x3c});call(b,0x49d050,{asset,0});call(b,0x499020,{});
            const auto tag=read(b,context+0xc4),allocation=read(b,asset+0x24);
            call(b,0x498230,{allocation,tag});
        }
        if(produced>=mission_sound_bank_candidates)throw MissionSoundBankError("Sound bank exceeds its eight candidate records");
        const auto slot=mission_sound_bank_scratch+produced*0x14;
        put(b,slot+4,4,read(b,asset+0x24));
        put(b,slot+0x10,4,(read(b,layer+8,1)&1u)?read(b,params+4):0);
        const auto bank_random=mission_sound_bank_random(b,read(b,bank+0x20));const auto bank_rate=read(b,bank+0x1c)+bank_random;
        const auto variant_random=mission_sound_bank_random(b,read(b,variant+8));const auto variant_rate=read(b,variant+4)+variant_random;
        auto scale=read(b,params);if(!scale)scale=0x10000;
        put(b,slot+8,4,product(product(bank_rate,variant_rate),scale));
        const auto range=read(b,layer+4);
        if(!range)put(b,slot+0xc,4,read(b,params+8));
        else{
            const auto ceiling=read(b,variant+0x10),level=read(b,variant+0xc),distance=read(b,params+0x14);
            put(b,slot+0xc,4,attenuation(b,distance,range<<16,level,ceiling));
        }
        ++produced;
    }
    U completed=0;
    for(U slot=mission_sound_bank_scratch;completed<8&&read(b,slot+4);slot+=0x14,++completed){
        const auto volume=read(b,slot+0xc);
        if(volume<read(b,context+0xc0))continue;
        U result=0;
        if(read(b,params+0xc)==0xffffffffu){
            const auto rate=read(b,slot+8),level=read(b,slot+0xc),pan=read(b,slot+0x10),sample=read(b,slot+4);
            result=call(b,0x494dd0,{sample,pan,level,rate});
        }else{
            const auto rate=read(b,slot+8),level=read(b,slot+0xc),pan=read(b,slot+0x10),sample=read(b,slot+4),channel=read(b,params+0xc);
            result=call(b,0x494e80,{channel,sample,pan,level,rate});
        }
        put(b,slot,4,result);
    }
    return completed;
}
void mission_sound_global_request(const MissionSoundBankBindings& b,U id,U params){
    if(id>=512)throw MissionSoundBankError("Global sound id exceeds its definition table");
    const auto at=0x4f2ea8u+id*0x28;
    if(!read(b,at))return;
    clear(b,params,32);
    const auto flags=read(b,at+0x20,1);
    put(b,params,4,0x10000);put(b,params+8,4,0xff);put(b,params+0xc,4,0xffffffffu);
    put(b,params+0x10,4,0);put(b,params+0x14,4,0);put(b,params+4,4,0);
    mission_sound_bank_play(b,read(b,at),params,(flags&4u)?0x6e05e0u:0x6e0518u);
}
} // namespace dfre
