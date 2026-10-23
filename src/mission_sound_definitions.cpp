#include "dfre/mission_sound_definitions.hpp"
#include <array>
namespace dfre {
void mission_sound_definitions_initialize(const MissionSoundAssetBindings& b,std::uint32_t name){
    if(!b.call)throw MissionSoundAssetError("Ambient definition loader is not bound");
    b.call(0x43edb0,{name});
    if(!b.read||!b.write)throw MissionSoundAssetError("Sound definition storage is not bound");
    const auto id=[&](std::uint32_t row){const auto n=b.read(row,2)&0xffffu;return n<0x8000?std::int32_t(n):std::int32_t(n)-0x10000;};
    for(std::uint32_t index=1;index<512;++index){
        const auto current=0x4f2ea4u+index*40;const auto current_id=id(current);
        if(current_id>=512||current_id==std::int32_t(index))continue;
        for(std::uint32_t candidate=1;candidate<512;++candidate){
            const auto other=0x4f2ea4u+candidate*40;
            if(id(other)!=std::int32_t(index))continue;
            std::array<std::uint32_t,10> temporary{};
            for(unsigned word=0;word<10;++word)temporary[word]=b.read(current+word*4,4);
            for(unsigned word=0;word<10;++word){const auto value=b.read(other+word*4,4);b.write(current+word*4,4,value);}
            for(unsigned word=0;word<10;++word)b.write(other+word*4,4,temporary[word]);
            break;
        }
    }
}
}
