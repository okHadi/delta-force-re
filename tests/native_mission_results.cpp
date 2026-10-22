#include "dfre/mission_results.hpp"

#include <iostream>
#include <stdexcept>
#include <map>

namespace {
void require(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }

std::uint32_t word(const dfre::AssetBytes& data, std::size_t at) {
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) |
           (std::uint32_t(data[at + 2]) << 16) | (std::uint32_t(data[at + 3]) << 24);
}

void put(dfre::AssetBytes& data, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data.at(at + i) = std::uint8_t(value >> (8 * i));
}

dfre::AssetBytes profile_with(std::uint32_t campaign = 1, std::uint32_t selected_mission = 0) {
    dfre::AssetBytes profile(0xa90, 0);
    put(profile, 0x28, campaign);
    put(profile, 0x470 + campaign * 4, selected_mission);
    return profile;
}
}

int main() {
    try {
        // Layout: campaign 1 words start at 0x74, mask at 0xb4 (stride 0x44).
        require(dfre::CampaignProfile::completion_offset(1, 0) == 0x74, "campaign 1 completion base");
        require(dfre::CampaignProfile::completion_offset(1, 5) == 0x88, "campaign 1 last mission");
        require(dfre::CampaignProfile::completion_offset(3, 7) == 0x74 + 2 * 0x44 + 7 * 4, "campaign 3 mission 7");
        require(dfre::CampaignProfile::mask_offset(1) == 0xb4, "campaign 1 mask");
        require(dfre::CampaignProfile::mask_offset(5) == 0x1c4, "campaign 5 mask");

        // Loss: nothing is written (original 0x407e1d gate).
        {
            auto profile = profile_with(2, 3);
            put(profile, dfre::CampaignProfile::completion_offset(2, 3), 0);
            put(profile, dfre::CampaignProfile::mask_offset(2), 0x1234);
            dfre::MissionResultInput input{2, 3, false, 0xffffffff};
            const auto update = dfre::apply_mission_result(profile, input);
            require(!update.completion_written && !update.mask_written, "loss writes nothing");
            require(word(profile, dfre::CampaignProfile::completion_offset(2, 3)) == 0, "loss keeps completion");
            require(word(profile, dfre::CampaignProfile::mask_offset(2)) == 0x1234, "loss keeps mask");
        }

        // Win: completion word becomes 1 and the mask takes the objective bits.
        {
            auto profile = profile_with(1, 2);
            dfre::MissionResultInput input{1, 2, true, 0x00000055};
            const auto update = dfre::apply_mission_result(profile, input);
            require(update.completion_written && update.completion_word == 1, "win writes completion 1");
            require(update.mask_written && update.campaign_mask == 0x55, "win writes objective mask");
            require(word(profile, 0x74 + 2 * 4) == 1, "completion word offset");
            require(word(profile, 0xb4) == 0x55, "mask offset");
            require(word(profile, 0x74 + 0) == 0 && word(profile, 0x74 + 4) == 0, "untouched sibling words");
        }

        // Debrief won flag reads the stored word; replayed aborts stay won.
        {
            auto profile = profile_with(1, 0);
            require(!dfre::debrief_won(profile, 1, 0), "empty profile loses");
            put(profile, 0x74, 1);
            require(dfre::debrief_won(profile, 1, 0), "stored completion wins");
            put(profile, 0x74, 0x1234);
            require(dfre::debrief_won(profile, 1, 0), "nonzero completion wins");
            require(!dfre::debrief_won(profile, 1, 1), "other mission loses");
        }

        // Campaign completion scan.
        {
            auto profile = profile_with(1, 0);
            require(!dfre::campaign_complete(profile, 1, 6), "empty campaign incomplete");
            for (std::uint32_t mission = 0; mission < 5; ++mission) put(profile, 0x74 + mission * 4, 1);
            require(!dfre::campaign_complete(profile, 1, 6), "one mission missing");
            put(profile, 0x74 + 5 * 4, 1);
            require(dfre::campaign_complete(profile, 1, 6), "all six complete");
        }

        // Mission availability: not completed, prerequisites subset of mask.
        {
            auto profile = profile_with(1, 0);
            require(dfre::mission_available(profile, 1, 1, 0), "no prerequisites always available");
            require(!dfre::mission_available(profile, 1, 1, 0x100), "locked without mask bits");
            put(profile, 0xb4, 0x80);
            require(!dfre::mission_available(profile, 1, 1, 0x100), "missing high bit still locked");
            put(profile, 0xb4, 0x180);
            require(dfre::mission_available(profile, 1, 1, 0x100), "subset unlocks");
            put(profile, 0x74 + 4, 1);
            require(!dfre::mission_available(profile, 1, 1, 0x100), "unlock applies to mission 2");
        }

        // Objective bit helpers use the original 5-bit shift.
        require(dfre::objective_bit(0x2, 1), "bit 1 set");
        require(!dfre::objective_bit(0x2, 0), "bit 0 clear");
        require(dfre::objective_bit(0x2, 33), "bit 33 wraps to bit 1");
        require(dfre::set_objective_bit(0x10, 2) == 0x14, "set bit 2");
        require(dfre::set_objective_bit(0, 33) == 0x2, "set bit 33 wraps");
        require(dfre::set_objective_bit(0x4, 0) == 0x5, "set keeps other bits");

        // Debrief text request: campaign+1 file, mission+1 group key.
        {
            const auto first = dfre::debrief_text_request(1, 0, true);
            require(first.file == "dfcamp02.bin" && first.group == "Briefing1" && first.key == "Win1", "campaign 1 win request");
            const auto last = dfre::debrief_text_request(5, 9, false);
            require(last.file == "dfcamp06.bin" && last.group == "Briefing10" && last.key == "Lose10", "campaign 5 lose request");
        }

        // Post Menu single-player decision table.
        require(dfre::post_menu_next(dfre::MissionExit::Complete) == dfre::PostMenuNext::debrief_menu, "status 1 debrief");
        require(dfre::post_menu_next(dfre::MissionExit::Quit) == dfre::PostMenuNext::quit_game, "status 2 quit");
        require(dfre::post_menu_next(dfre::MissionExit::Message) == dfre::PostMenuNext::quit_game, "status 5 quit");
        require(dfre::post_menu_next(dfre::MissionExit::Abort) == dfre::PostMenuNext::main_menu, "status 3 main menu");
        require(dfre::post_menu_next(dfre::MissionExit::PlayerDown) == dfre::PostMenuNext::main_menu, "status 4 default branch");

        // Invalid campaign indexes are rejected like the original tables would use them.
        {
            auto profile = profile_with(1, 0);
            bool refused = false;
            try { dfre::apply_mission_result(profile, {0, 0, true, 0}); } catch (const dfre::AssetError&) { refused = true; }
            require(refused, "campaign 0 rejected");
            refused = false;
            try { dfre::debrief_won(profile, 6, 0); } catch (const dfre::AssetError&) { refused = true; }
            require(refused, "campaign 6 rejected");
            refused = false;
            try { dfre::campaign_complete(profile, 1, 17); } catch (const dfre::AssetError&) { refused = true; }
            require(refused, "mission count 17 rejected");
            for(const auto mission:{16u,0xffffffffu}){
                const auto before=profile;
                refused=false;try{dfre::apply_mission_result(profile,{1,mission,true,9});}catch(const dfre::AssetError&){refused=true;}
                require(refused&&profile==before,"invalid mission rejected before profile writes");
                refused=false;try{dfre::debrief_won(profile,1,mission);}catch(const dfre::AssetError&){refused=true;}
                require(refused,"invalid mission rejected before profile reads");
                refused=false;try{dfre::mission_available(profile,1,mission,0);}catch(const dfre::AssetError&){refused=true;}
                require(refused,"invalid mission availability rejected");
            }
        }

        // Short profile buffers are rejected before any write.
        {
            dfre::AssetBytes tiny(0x40, 0);
            bool refused = false;
            try { dfre::apply_mission_result(tiny, {1, 0, true, 0}); } catch (const dfre::AssetError&) { refused = true; }
            require(refused, "short profile rejected");
        }

        {
            auto profile=profile_with();dfre::MissionResultImage image{};
            std::map<std::uint32_t,std::uint32_t> cells{{0x67f528,1},{0x67f520,0},{0x68ba34,2},{0x503134,3},{0x503130,1}};
            const auto result_put=[&](std::size_t at,std::uint32_t n){for(unsigned i=0;i<4;++i)image.at(at+i)=std::uint8_t(n>>(8*i));};
            result_put(0xc,1);result_put(0x24+0x28,4);result_put(0x24+0x2c,7);result_put(0x24+0x30,98304);
            unsigned ends=0,snapshots=0;dfre::MissionProfileBindings bindings;
            bindings.cell=[&](auto at){return cells[at];};bindings.store=[&](auto at,auto value){cells[at]=value;};
            bindings.profile=[&]()->dfre::AssetBytes&{return profile;};
            bindings.result=[&]{++snapshots;return image;};bindings.fp_control=[] {return std::uint16_t(0x37f);};
            bindings.end_events=[&]{require(word(profile,0x74+8)==0,"End events ran after profile recording");++ends;result_put(4,1);cells[0x67f718]=1;cells[0x67dbec]=0x17;};
            dfre::mission_profile_leave(bindings);
            require(ends==1&&snapshots==1&&word(profile,0x74+8)==1&&word(profile,0xb4)==0x17,"Full profile leave ordering");
            require(word(profile,0x664)==4&&word(profile,0x678)==7&&word(profile,0x674)==6,"Single player profile totals");
            require(cells[0x67f4e8]==3&&cells[0x4ced28]==3&&cells[0x67f4ec]==1&&cells[0x4ced2c]==1,"Current display settings persisted");
            cells[0x67f528]=0;cells[0x67f718]=2;cells[0x67f70c]=0x29;result_put(4,2);
            dfre::mission_profile_leave(bindings);
            require(ends==1&&word(profile,0x664)==4&&word(profile,0xb4)==0x29,"Failed mission result gate");
            cells[0x67f520]=1;cells[0x67f53c]=4;result_put(0,2);
            dfre::mission_profile_record(bindings);
            require(word(profile,0x648)==4&&word(profile,0x650)==7&&word(profile,0x658)==6&&word(profile,0x65c)==1,"Multiplayer profile totals");
            const auto before=profile;result_put(0xc,17);bool refused=false;
            try{dfre::mission_profile_record(bindings);}catch(const dfre::AssetError&){refused=true;}
            require(refused&&profile==before,"Invalid result row wrote profile data");
            require(dfre::mission_profile_stat(-98304,3,0x37f)==std::uint32_t(-4),"Signed profile statistic truncation");
            require(dfre::mission_profile_stat(0x7fffffff,1,0x7f)==32768,"24-bit profile statistic rounding");
            require(dfre::mission_profile_stat(0x7fffffff,1,0x37f)==32767,"64-bit profile statistic rounding");
            require(dfre::mission_profile_stat(-0x7fffffff,1,0x47f)==std::uint32_t(-32768),"Profile statistic round down");
            require(dfre::mission_profile_stat(-2147483647-1,-2147483647-1,0x37f)==0x80000000u,"Profile FISTP indefinite result");
        }

        std::cout << "mission results checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
