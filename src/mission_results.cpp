#include "dfre/mission_results.hpp"

#include <cstdio>
#include <limits>

namespace dfre {
namespace {
std::uint32_t read_word(const AssetBytes& data, std::size_t at) {
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) |
           (std::uint32_t(data[at + 2]) << 16) | (std::uint32_t(data[at + 3]) << 24);
}

void write_word(AssetBytes& data, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data.at(at + i) = std::uint8_t(value >> (8 * i));
}

// Original profile buffer holds five 0x44-byte campaign records. Require the
// complete record area so a port cannot write outside a real profile.
void require_record_area(const AssetBytes& profile, std::uint32_t campaign) {
    if (campaign < 1 || campaign > 5) throw AssetError("Campaign index is outside the original table");
    if (profile.size() < CampaignProfile::mask_offset(5) + 4) throw AssetError("Profile is smaller than the original campaign records");
}
void require_mission(std::uint32_t mission){
    if(mission>=16)throw AssetError("Mission index is outside the campaign record");
}

std::string number(std::uint32_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02u", value);
    return buffer;
}
} // namespace

MissionResultUpdate apply_mission_result(AssetBytes& profile, const MissionResultInput& input) {
    require_record_area(profile, input.campaign);
    require_mission(input.mission);
    MissionResultUpdate update;
    if (!input.completed) return update; // original 0x407e1d gate ([0x67f718] != 1)
    const auto completion = CampaignProfile::completion_offset(input.campaign, input.mission);
    write_word(profile, completion, 1); // original 0x407f16 stores 1
    update.completion_written = true;
    update.completion_word = 1;
    const auto mask = CampaignProfile::mask_offset(input.campaign);
    write_word(profile, mask, input.objective_mask); // original 0x407a80 stores 0x67f70c
    update.mask_written = true;
    update.campaign_mask = input.objective_mask;
    return update;
}

bool debrief_won(const AssetBytes& profile, std::uint32_t campaign, std::uint32_t mission) {
    require_record_area(profile, campaign);
    require_mission(mission);
    return read_word(profile, CampaignProfile::completion_offset(campaign, mission)) != 0;
}

bool campaign_complete(const AssetBytes& profile, std::uint32_t campaign, std::uint32_t mission_count) {
    require_record_area(profile, campaign);
    if (mission_count == 0 || mission_count > 16) throw AssetError("Campaign mission count is outside the original table");
    for (std::uint32_t mission = 0; mission < mission_count; ++mission)
        if (read_word(profile, CampaignProfile::completion_offset(campaign, mission)) == 0) return false;
    return true;
}

bool mission_available(const AssetBytes& profile, std::uint32_t campaign,
                       std::uint32_t mission, std::uint32_t prerequisites) {
    require_record_area(profile, campaign);
    require_mission(mission);
    if (read_word(profile, CampaignProfile::completion_offset(campaign, mission)) != 0) return false;
    const auto mask = read_word(profile, CampaignProfile::mask_offset(campaign));
    return !prerequisites || (mask & prerequisites) == prerequisites;
}

bool objective_bit(std::uint32_t objective_mask, std::uint32_t bit) noexcept {
    return (objective_mask >> (bit & 31)) & 1u; // x86 shl uses cl mod 32
}

std::uint32_t set_objective_bit(std::uint32_t objective_mask, std::uint32_t bit) noexcept {
    return objective_mask | (1u << (bit & 31));
}

DebriefTextRequest debrief_text_request(std::uint32_t campaign, std::uint32_t mission, bool won) {
    if (campaign < 1 || campaign > 5) throw AssetError("Campaign index is outside the original table");
    require_mission(mission);
    DebriefTextRequest request;
    request.file = "dfcamp" + number(campaign + 1) + ".bin"; // original 0x4cae34
    request.group = "Briefing" + std::to_string(mission + 1); // original 0x4cad50
    request.key = (won ? "Win" : "Lose") + std::to_string(mission + 1); // 0x4cad48 / 0x4cadd0
    return request;
}

PostMenuNext post_menu_next(MissionExit status) noexcept {
    switch (status) {
    case MissionExit::Quit: return PostMenuNext::quit_game;        // original 0x42460c
    case MissionExit::Message: return PostMenuNext::quit_game;     // original 0x424620
    case MissionExit::Complete: return PostMenuNext::debrief_menu; // original 0x42466d
    default: return PostMenuNext::main_menu;                       // original 0x424708
    }
}

namespace {
template<class F> F profile_need(const F& fn,const char* message){if(!fn)throw AssetError(message);return fn;}
std::int32_t profile_signed(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::int64_t profile_round(std::int64_t value,unsigned precision,unsigned mode){
    const bool negative=value<0;const auto magnitude=std::uint64_t(negative?-value:value);
    auto remaining=magnitude;unsigned bits=0;while(remaining){++bits;remaining>>=1;}
    if(bits<=precision)return value;
    const auto shift=bits-precision;auto high=magnitude>>shift;
    const auto low=magnitude&((std::uint64_t(1)<<shift)-1);
    bool increment=false;
    if(mode==0){const auto half=std::uint64_t(1)<<(shift-1);increment=low>half||(low==half&&(high&1));}
    else if(mode==1)increment=negative&&low;
    else if(mode==2)increment=!negative&&low;
    if(increment)++high;
    const auto rounded=std::int64_t(high<<shift);return negative?-rounded:rounded;
}
std::uint32_t result_word(const MissionResultImage& image,std::size_t at){
    return std::uint32_t(image.at(at))|std::uint32_t(image.at(at+1))<<8|
        std::uint32_t(image.at(at+2))<<16|std::uint32_t(image.at(at+3))<<24;
}
}
std::uint32_t mission_profile_stat(std::int32_t ratio,std::int32_t count,std::uint16_t control){
    const auto pc=(control>>8)&3u,mode=(control>>10)&3u;
    if(pc==1)throw AssetError("Profile statistics need a defined x87 precision");
    const auto precision=pc==0?24u:pc==2?53u:64u;
    const auto first=profile_round(ratio,precision,mode);
    const auto product=profile_round(first*count,precision,mode)/65536;
    if(product<std::numeric_limits<std::int32_t>::min()||product>std::numeric_limits<std::int32_t>::max())return 0x80000000u;
    return std::uint32_t(product);
}
void mission_profile_record(const MissionProfileBindings& b){
    const auto image=profile_need(b.result,"Profile result image is not bound")();
    const auto multiplayer=profile_need(b.cell,"Profile cells are not bound")(0x67f520);
    if(!multiplayer&&result_word(image,4)!=1)return;
    auto& profile=profile_need(b.profile,"Current profile is not bound")();
    if(profile.size()<0x67c)throw AssetError("Profile statistic fields are incomplete");
    const auto row=result_word(image,0xc);
    if(row>=17)throw AssetError("Profile result row exceeds its client table");
    const auto at=std::size_t(row)*0x24;
    if(!multiplayer){
        const auto campaign=read_word(profile,0x28),mission=profile_need(b.cell,"Profile cells are not bound")(0x68ba34);
        require_record_area(profile,campaign);require_mission(mission);
        write_word(profile,CampaignProfile::completion_offset(campaign,mission),1);
    }
    const auto add=[&](std::size_t offset,std::uint32_t value){write_word(profile,offset,read_word(profile,offset)+value);};
    add(multiplayer?0x648:0x664,result_word(image,at+0x28));
    add(multiplayer?0x650:0x678,result_word(image,at+0x2c));
    const auto offset=multiplayer?0x658u:0x674u;
    const auto before=read_word(profile,offset);
    const auto amount=mission_profile_stat(profile_signed(result_word(image,at+0x30)),
        profile_signed(result_word(image,at+0x28)),profile_need(b.fp_control,"Profile x87 control is not bound")());
    write_word(profile,offset,before+amount);
    if(multiplayer){
        const auto type=profile_need(b.cell,"Profile cells are not bound")(0x67f53c);
        if((type==2||type==4||type==5)&&result_word(image,4)==result_word(image,0))add(0x65c,1);
    }
}
void mission_profile_settings(const MissionProfileBindings& b){
    constexpr std::array<std::array<std::uint32_t,2>,23> pairs{{
        {{0x67f4e4,0x4ced24}},{{0x67f4e8,0x4ced28}},{{0x67f4ec,0x4ced2c}},{{0x67f4f0,0x4ced30}},
        {{0x67f4f4,0x4ced34}},{{0x67f4f8,0x4ced38}},{{0x67f4fc,0x4ced3c}},{{0x67f500,0x4ced44}},
        {{0x67f504,0x4ced40}},{{0x67f510,0x4ced4c}},{{0x67f514,0x4ced48}},{{0x67f508,0x4cf2d0}},
        {{0x67f50c,0x4cf2d4}},{{0x67f540,0x4cee80}},{{0x67f544,0x4cee84}},{{0x67f54c,0x4cee88}},
        {{0x67f548,0x4cee8c}},{{0x67f534,0x4cee94}},{{0x67f550,0x4cee98}},{{0x67f554,0x4cee9c}},
        {{0x67f558,0x4ceea0}},{{0x67f55c,0x4ceea4}},{{0x67f564,0x4cf2e0}}
    }};
    for(std::size_t i=0;i+1<pairs.size();++i){const auto& pair=pairs[i];const auto value=profile_need(b.cell,"Profile cells are not bound")(pair[0]);profile_need(b.store,"Profile cell writer is not bound")(pair[1],value);}
    const auto last=profile_need(b.cell,"Profile cells are not bound")(pairs.back()[0]);
    auto& profile=profile_need(b.profile,"Current profile is not bound")();
    profile_need(b.store,"Profile cell writer is not bound")(pairs.back()[1],last);
    if(profile.size()<0x2c)throw AssetError("Profile campaign field is incomplete");
    const auto campaign=read_word(profile,0x28);require_record_area(profile,campaign);
    write_word(profile,CampaignProfile::mask_offset(campaign),profile_need(b.cell,"Profile cells are not bound")(0x67f70c));
}
void mission_profile_leave(const MissionProfileBindings& b){
    const auto cell=[&](std::uint32_t at){return profile_need(b.cell,"Profile cells are not bound")(at);};
    const auto store=[&](std::uint32_t at,std::uint32_t value){profile_need(b.store,"Profile cell writer is not bound")(at,value);};
    if(cell(0x67f528))profile_need(b.end_events,"Profile end-event stage is not bound")();
    store(0x67f4e8,cell(0x503134));store(0x67f4ec,cell(0x503130));
    if(!cell(0x67f520)&&cell(0x67f718)==1)store(0x67f70c,cell(0x67dbec));
    mission_profile_record(b);mission_profile_settings(b);
}

} // namespace dfre
