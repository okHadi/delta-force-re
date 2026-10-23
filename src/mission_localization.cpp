#include "dfre/mission_localization.hpp"
namespace dfre {
std::uint32_t mission_localized_text(const MissionLocalizationBindings& b,
    std::uint32_t group,std::uint32_t key){
    if(!b.read)throw std::runtime_error("Localization current memory is not bound");
    const auto table=b.read(0x503230,4);
    if(!table)return 0x4ced16;
    if(b.read(0x67dbc9,1)&0x20)return 0x4ced14;
    if(!b.lookup_49a092)throw std::runtime_error("Localization lookup49A092 is not bound");
    const auto entry=b.lookup_49a092(table,group,key);
    if(entry){const auto text=b.read(entry,4);if(text)return text;}
    return 0x4ced16;
}
}
