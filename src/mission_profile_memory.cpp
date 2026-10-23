#include "dfre/mission_profile_memory.hpp"
namespace dfre {namespace {
using U=std::uint32_t;
constexpr U pointer=0x4cf2e4,bank=0x21100000,stride=0x1000,limit=bank+9*stride;
void check(bool value,const char* text){if(!value)throw AssetError(text);}
}
MissionProfileMemory::MissionProfileMemory(std::function<AssetBytes&(std::size_t)> p,std::function<std::size_t()> s):profile_(std::move(p)),selected_(std::move(s)){check(bool(profile_)&&bool(selected_),"Profile memory needs current owners");}
AssetBytes& MissionProfileMemory::current(std::size_t slot)const{check(slot<9,"Profile slot is outside the source table");auto& bytes=profile_(slot);check(bytes.size()==PlayerProfile::byte_size,"Current profile size differs from the source record");return bytes;}
U MissionProfileMemory::source(std::size_t slot)const{(void)current(slot);return bank+U(slot)*stride;}
std::optional<U> MissionProfileMemory::record_base(U at,U width)const{
    const auto end=std::uint64_t(at)+width;check((width==1||width==2||width==4)&&end<=0x100000000ull,"Profile memory width or range is invalid");
    if(at<pointer+4&&end>pointer){check(at>=pointer&&end<=pointer+4,"Profile pointer access crosses its owner");return pointer;}
    if(at>=limit||end<=bank)return {};
    check(at>=bank&&end<=limit,"Profile access crosses the reserved bank");const U slot=(at-bank)/stride,offset=(at-bank)%stride;
    check(offset+width<=PlayerProfile::byte_size,"Profile access is outside the current record");(void)current(slot);return bank+slot*stride;
}
bool MissionProfileMemory::claims(U at,U width)const{return record_base(at,width).has_value();}
U MissionProfileMemory::read(U at,U width)const{
    const auto base=record_base(at,width);check(base.has_value(),"Profile memory read has no owner");
    if(*base==pointer){const U value=source(selected_());return (value>>((at-pointer)*8))&(width==4?0xffffffffu:((1u<<(width*8))-1));}
    const auto& bytes=current((*base-bank)/stride);U value=0;for(U i=0;i<width;++i)value|=U(bytes.at(at-*base+i))<<(i*8);return value;
}
void MissionProfileMemory::write(U at,U width,U value){
    const auto base=record_base(at,width);check(base.has_value()&&*base!=pointer,"Profile write needs current profile bytes");
    auto& bytes=current((*base-bank)/stride);for(U i=0;i<width;++i)bytes.at(at-*base+i)=std::uint8_t(value>>(i*8));
}
}
