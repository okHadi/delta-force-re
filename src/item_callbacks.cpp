#include "dfre/item_callbacks.hpp"
#include <array>
namespace dfre {
namespace {
struct Row { std::string_view name; std::uint32_t callback; };
constexpr std::array<Row,34> ai_rows{{
    {"null",0x45b590u},
    {"org0",0x45b5a0u},
    {"org1",0x45b5b0u},
    {"plyr",0x45bdf0u},
    {"brrl",0x45bf80u},
    {"clay",0x45c010u},
    {"cly2",0x45c080u},
    {"schg",0x45c110u},
    {"dec0",0x45c180u},
    {"sgen",0x42a520u},
    {"snd0",0x45c340u},
    {"snd1",0x45c3a0u},
    {"snd2",0x45c400u},
    {"slgt",0x4519a0u},
    {"tree",0x45c460u},
    {"veh0",0x429bc0u},
    {"helo",0x450e40u},
    {"spar",0x451d40u},
    {"sgul",0x451fd0u},
    {"btre",0x451e70u},
    {"prnh",0x4528c0u},
    {"vult",0x452e10u},
    {"mine",0x453220u},
    {"gnrc",0x4533f0u},
    {"cact",0x4534a0u},
    {"lamp",0x4535b0u},
    {"palm",0x453c20u},
    {"oakt",0x453740u},
    {"wdsm",0x453810u},
    {"wdmd",0x4538a0u},
    {"wdlg",0x453930u},
    {"bldg",0x453eb0u},
    {"peng",0x453f30u},
    {"vtrg",0x454740u},
}};
constexpr std::array<Row,13> move_rows{{
    {"null",0x4563a0u},
    {"smok",0x42a590u},
    {"slgt",0x451cb0u},
    {"veh0",0x429ea0u},
    {"helo",0x451590u},
    {"spar",0x451dd0u},
    {"sgul",0x452310u},
    {"prnh",0x452bc0u},
    {"vult",0x453000u},
    {"gdeb",0x453530u},
    {"lamp",0x4536e0u},
    {"psec",0x453dd0u},
    {"peng",0x4543b0u},
}};
bool equal(std::string_view a,std::string_view b) noexcept {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i){
        auto c=static_cast<unsigned char>(a[i]);
        if(c>='A'&&c<='Z') c=static_cast<unsigned char>(c+32);
        if(c!=static_cast<unsigned char>(b[i])) return false;
    }
    return true;
}
template<std::size_t N> std::uint32_t lookup(const std::array<Row,N>& rows,std::string_view name) noexcept {
    for(const auto& row:rows) if(equal(name,row.name)) return row.callback;
    return rows[0].callback;
}
std::string_view text(const char* source,std::size_t capacity) noexcept {
    std::size_t n=0;while(n<capacity&&source[n]) ++n;
    return {source,n};
}
}
std::uint32_t resolve_item_ai_callback(std::string_view name) noexcept { return lookup(ai_rows,name); }
std::uint32_t resolve_item_move_callback(std::string_view name) noexcept { return lookup(move_rows,name); }
void bind_runtime_item_callbacks(std::vector<RuntimeItemDefinition>& items,const ItemSoundResolver& sound) {
    for(auto& item:items){
        item.ai_data=resolve_item_ai_callback(text(item.ai_function,sizeof(item.ai_function)));
        item.move_data=resolve_item_move_callback(text(item.move_function,sizeof(item.move_function)));
        for(std::size_t i=0;i<4;++i){
            const auto id=sound?sound(text(item.ambient[i],sizeof(item.ambient[i]))):0u;
            item.ambient_handles[i]=id;
        }
    }
}
}
