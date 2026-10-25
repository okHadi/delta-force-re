#include "dfre/mission_debrief_format.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#include <string_view>
namespace dfre {namespace {
using U=std::uint32_t;
struct Format {U address;std::string_view bytes;std::size_t arguments;};
constexpr std::array<Format,14> formats{{
    {0x4cae34,"dfcamp%02i.bin",3},{0x4cad48,"Win%d",3},{0x4cadd0,"Lose%d",3},
    {0x4cad50,"Briefing%d",3},{0x4cad5c,"Unable to find mission %d debriefing won text",3},
    {0x4cadd8,"Unable to find mission %d debriefing lose text",3},
    {0x4cae08,"%s ~ ~ %s",4},{0x4cadbc,"%s ~ %s : %d ~ ~ %s",6},
    {0x4cae14,"Unable to find '%s'",3},
    {0x4cc67c,"WPN%03i",3},{0x4cc570,"%i",3},{0x4cc278,"%s %s",4},
    {0x4ccc60,"STR_PWNAME%02i",3},{0x4ccc80,"STR_SPWNAME%02i",3}
}};
template<class T> const T& need(const T& f){if(!f)throw std::runtime_error("Debrief format reached a missing provider");return f;}
std::int32_t si(U n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
}
U mission_debrief_format(const MissionDebriefBindings& b,const std::vector<U>& args){
    if(args.size()<2)throw std::runtime_error("Debrief format call is incomplete");
    const Format* selected=nullptr;for(const auto& format:formats)if(format.address==args[1]){selected=&format;break;}
    if(!selected)throw std::runtime_error("Debrief format pointer is unsupported");
    if(args.size()!=selected->arguments)throw std::runtime_error("Debrief format argument count differs");
    for(std::size_t i=0;i<=selected->bytes.size();++i){
        const auto current=need(b.read)(selected->address+U(i),1)&255u;
        const auto expected=i<selected->bytes.size()?U(std::uint8_t(selected->bytes[i])):0u;
        if(current!=expected)throw std::runtime_error("Debrief literal bytes differ");
    }
    U count=0;const auto store=[&](U at,U c){
        for(const auto& format:formats)if(at>=format.address&&std::uint64_t(at)<std::uint64_t(format.address)+format.bytes.size()+1)
            throw std::runtime_error("Debrief output overlaps a format literal");
        need(b.write)(at,1,c);
    };
    const auto emit=[&](U c){store(args[0]+count,c);++count;};
    const auto literal=[&](std::string_view bytes){for(unsigned char c:bytes)emit(c);};
    const auto integer=[&](U value,U width=0){
        const bool negative=(value&0x80000000u)!=0;U magnitude=negative?0u-value:value;
        std::array<std::uint8_t,10> digits{};U size=0;
        do{digits[size++]=std::uint8_t('0'+magnitude%10);magnitude/=10;}while(magnitude);
        if(negative)emit('-');for(U used=size+U(negative);used<width;++used)emit('0');
        while(size)emit(digits[--size]);
    };
    const auto string=[&](U source){
        if(!source)return;
        U length=0,cursor=source;
        for(;;){const auto c=need(b.read)(cursor++,1)&255u;if(!c||length==0xffffffffu)break;++length;}
        if(si(length)<=0)return;
        for(U i=0;i<length;++i){const auto c=need(b.read)(source++,1)&255u;emit(c);}
    };
    switch(args[1]){
    case 0x4cae34:literal("dfcamp");integer(args[2],2);literal(".bin");break;
    case 0x4cad48:literal("Win");integer(args[2]);break;
    case 0x4cadd0:literal("Lose");integer(args[2]);break;
    case 0x4cad50:literal("Briefing");integer(args[2]);break;
    case 0x4cad5c:literal("Unable to find mission ");integer(args[2]);literal(" debriefing won text");break;
    case 0x4cadd8:literal("Unable to find mission ");integer(args[2]);literal(" debriefing lose text");break;
    case 0x4cae08:string(args[2]);literal(" ~ ~ ");string(args[3]);break;
    case 0x4cadbc:string(args[2]);literal(" ~ ");string(args[3]);literal(" : ");integer(args[4]);literal(" ~ ~ ");string(args[5]);break;
    case 0x4cae14:literal("Unable to find '");string(args[2]);literal("'");break;
    case 0x4cc67c:literal("WPN");integer(args[2],3);break;
    case 0x4cc570:integer(args[2]);break;
    case 0x4cc278:string(args[2]);literal(" ");string(args[3]);break;
    case 0x4ccc60:literal("STR_PWNAME");integer(args[2],2);break;
    case 0x4ccc80:literal("STR_SPWNAME");integer(args[2],2);break;
    default:throw std::runtime_error("Debrief format recipe is missing");
    }
    store(args[0]+count,0);return count;
}
}
