#pragma once

// Mission results: original data and state rules.
//
// Source: Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Addresses below are original virtual addresses. Rules marked "observed" were
// read from original code and replayed under Unicorn (see
// tools/check_mission_results_original.py). Objective evaluation stays in the
// events layer; this module consumes explicit objective inputs.
//
// Role summary (observed unless noted):
//   0x67f70c  campaign objective mask loaded from the profile at game start
//             (FUN_00407a90 sets it from profile[0x70+campaign*0x44]).
//   0x67dbec  live objective bit mask: FUN_00403a60 copies it from 0x67f70c,
//             original helper 0x42ee50 sets bits, helper 0x42e5b0 tests bits.
//   0x67f718  single-player completion flag in the result state block at
//             0x67f714. Only when it is 1 at mission end does the original
//             write the profile completion word and single-player totals. Supply the flag
//             from the 0x471010 result-message path.
//   0x67dbbc  game-loop end status: 0 play, 1 result flow, 2 quit, 3 abort,
//             4 player down (single player).
//   0x68ba34  current mission index (0-based) parsed from the mission filename.
//   0x4e6cfc  debrief won flag; derived from the stored completion word.
//
// The module must not evaluate BMS events. Feed it the objective mask and
// completion flag from the events layer.

#include "dfre/assets.hpp"

#include <cstdint>
#include <array>
#include <functional>
#include <string>

namespace dfre {

// Original state table addresses used by the result flow.
constexpr std::uint32_t state_game_loop = 0x4cece0;
constexpr std::uint32_t state_pre_menu = 0x4e6878;
constexpr std::uint32_t state_post_menu = 0x4eb834;
constexpr std::uint32_t state_debrief_menu = 0x4e7a58;
constexpr std::uint32_t state_pdm_menu = 0x4eb8a4;
constexpr std::uint32_t state_campaign_menu = 0x4d0cd4;
constexpr std::uint32_t state_main_menu = 0x4cf2ec;
constexpr std::uint32_t state_quit = 0;

// Original single-player campaign profile layout.
struct CampaignProfile {
    static constexpr std::uint32_t campaign_field = 0x28;        // 1..5
    static constexpr std::uint32_t completion_base = 0x30;       // word per mission
    static constexpr std::uint32_t mask_base = 0x70;             // prerequisite mask
    static constexpr std::uint32_t stride = 0x44;                // bytes per campaign
    static constexpr std::uint32_t selected_mission = 0x470;     // mission index
    static constexpr std::uint32_t mission_stride = 4;

    // profile[0x30 + campaign*0x44 + mission*4] (original 0x407efc/0x41f320).
    static constexpr std::uint32_t completion_offset(std::uint32_t campaign, std::uint32_t mission) {
        return completion_base + campaign * stride + mission * mission_stride;
    }
    // profile[0x70 + campaign*0x44] (original 0x407a80/0x407c86).
    static constexpr std::uint32_t mask_offset(std::uint32_t campaign) {
        return mask_base + campaign * stride;
    }
};

// Original game-loop end status values written to 0x67dbbc.
enum class MissionExit : std::uint32_t {
    Playing = 0,   // 0x67dbbc == 0, continue
    Complete = 1,  // result flow: Post Menu then single-player debrief
    Quit = 2,      // Post Menu requests state 0
    Abort = 3,     // Post Menu requests Main Menu in single player
    PlayerDown = 4, // single player: FUN_00404e30 restart path
    Message = 5    // Post Menu shows a message then requests state 0 (0x424620)
};

// Explicit objective inputs from the events layer.
struct MissionResultInput {
    std::uint32_t campaign = 1;        // 1..5 (profile 0x28 semantics)
    std::uint32_t mission = 0;         // 0-based (0x68ba34 semantics)
    bool completed = false;            // original 0x67f718 == 1 at mission end
    std::uint32_t objective_mask = 0;  // original 0x67dbec live objective bits
};

// Values written by the original end-of-mission recorder.
struct MissionResultUpdate {
    bool completion_written = false;
    std::uint32_t completion_word = 0; // always 1 when written (0x407f16)
    bool mask_written = false;
    std::uint32_t campaign_mask = 0;   // original 0x67f70c value stored
};

// Completion and mask writes from 0x401e70 -> 0x407de0 -> 0x407980.
// Supply campaigns 1..5 and mission slots 0..15. Bind other result effects
// through the live mission host.
//   completed != true  : profile unchanged (mask keeps its stored value).
//   completed == true  : completion word := 1, campaign mask := objective_mask.
MissionResultUpdate apply_mission_result(AssetBytes& profile, const MissionResultInput& input);

// Reads the original completion word and returns whether the debrief shows the
// won text (original 0x41f320: nonzero word -> won flag 1). Values other than 0
// or 1 are preserved by the original and treated as won.
bool debrief_won(const AssetBytes& profile, std::uint32_t campaign, std::uint32_t mission);

// Original 0x41ef40: true when every mission word of the campaign is nonzero.
bool campaign_complete(const AssetBytes& profile, std::uint32_t campaign, std::uint32_t mission_count);

// Mission availability: not completed and all prerequisite bits present.
// Prerequisite bits come from the BMS header word at 0x7c.
bool mission_available(const AssetBytes& profile, std::uint32_t campaign,
                       std::uint32_t mission, std::uint32_t prerequisites);

// Original objective bit helpers (0x42e5b0 test, 0x42ee50 set). The x86 shift
// masks the bit index to 5 bits.
bool objective_bit(std::uint32_t objective_mask, std::uint32_t bit) noexcept;
std::uint32_t set_objective_bit(std::uint32_t objective_mask, std::uint32_t bit) noexcept;

// Debrief text selection (original 0x41ef80). The campaign text table is
// "dfcamp%02i.bin" with file index campaign+1; the group is "Briefing%d" and
// the key "Win%d" or "Lose%d" with mission+1.
struct DebriefTextRequest {
    std::string file;
    std::string group;
    std::string key;
};
DebriefTextRequest debrief_text_request(std::uint32_t campaign, std::uint32_t mission, bool won);

// Single-player Post Menu decision (original 0x4245b0): status 1 opens the
// debrief, 2 and 5 quit, anything else returns to the main menu. Values 0 and 4
// are handled by the game loop before Post Menu runs.
enum class PostMenuNext { quit_game, debrief_menu, main_menu };
PostMenuNext post_menu_next(MissionExit status) noexcept;

// Result state block address (original copy buffer for the debriefing snapshot).
constexpr std::uint32_t result_state_block = 0x67f714;
constexpr std::uint32_t result_state_bytes = 0x514;
constexpr std::uint32_t result_snapshot = 0x4e7544; // copied by 0x41f390 (debrief enter)

using MissionResultImage=std::array<std::uint8_t,result_state_bytes>;
struct MissionProfileBindings {
    // Bind current shared cells. Keep each returned profile buffer alive through
    // the call. The settings stage captures its owner before its final store.
    std::function<std::uint32_t(std::uint32_t)> cell;
    std::function<void(std::uint32_t,std::uint32_t)> store;
    std::function<AssetBytes&()> profile;
    std::function<MissionResultImage()> result;
    std::function<std::uint16_t()> fp_control;
    std::function<void()> end_events; // 42EBA0
};
// 407DE0 product: x87 precision/rounding, truncation, and FISTP indefinite.
std::uint32_t mission_profile_stat(std::int32_t ratio,std::int32_t count,std::uint16_t control);
void mission_profile_record(const MissionProfileBindings&); // 407DE0
void mission_profile_settings(const MissionProfileBindings&); // 407980
void mission_profile_leave(const MissionProfileBindings&); // 401E70

} // namespace dfre
