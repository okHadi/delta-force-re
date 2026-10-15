#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dfre {

// df.cfg settings. Defaults come from 0x407120, the reader callback from
// 0x406640 and the writer from 0x407360. Values are stored verbatim; the
// original only clamps maxplayers (above 0x10 becomes 0x10, 0x406b8c).
struct GameConfig {
    // MUSIC
    std::int32_t cd_play=0,menu_track=0,game_track=0,cd_volume=0,fx_volume=0,dialog_volume=0;
    // DISPLAY
    std::int32_t game_res=0,window_size=0,windowed=0,renderer=0,max_res=0,gamma=0,turbo=0;
    // CONTROLS
    std::int32_t flipmouse=0,crosshairs=0,mousescale=0,showgun=0,picinpic=0;
    // MULTIPLAYER
    std::string login_password,game_password,squad_password,game_name,phone_number,internet_address;
    std::int32_t mp_service=0,mp_gametype=0,maxplayers=0,dedicated=0,com_port=0,baud_rate=0;
    std::int32_t max_lives=0,max_kills=0,team_req=0,attributes=0,replay=0,time_limit=0,koth_limit=0;
    std::int32_t ammo_respawn=0,timeout=0,gpsicon=0,showtags=0,contest=0;
    // MAP
    std::int32_t map_infrared=0,map_iff=0,map_awac=0;
    // SYSTEM
    std::int32_t preempt_pff=0,whichbrowser=0;
    std::string country,red_password,blue_password;
    std::int32_t mp_verbose=0;
    // Set by 0x407120 and never saved to df.cfg.
    std::int32_t runtime_ced58=0,runtime_ced60=0,runtime_ced64=0,runtime_ced68=0;
    std::int32_t window_scale=0; // 0x4ced5c, default 0x10000
    std::int32_t map_scale_lo=0; // 0x4cf2d0, default 0
    std::int32_t map_scale_hi=0; // 0x4cf2d4, default 1
};

// Original fixed widths of the string fields in the 0x4ced20 block. The
// original copies without a length check; this port keeps full strings.
inline constexpr std::size_t login_password_capacity=32;
inline constexpr std::size_t game_password_capacity=32;
inline constexpr std::size_t squad_password_capacity=48;
inline constexpr std::size_t game_name_capacity=36;
inline constexpr std::size_t country_capacity=4;
inline constexpr std::size_t phone_number_capacity=32;
inline constexpr std::size_t internet_address_capacity=32;
inline constexpr std::size_t red_password_capacity=16;
inline constexpr std::size_t blue_password_capacity=16;

// Pass the CPUID family byte kept at 0x4f8704; renderer and max_res follow it.
GameConfig default_game_config(std::int32_t cpu_family);

// Parse an unmodified df.cfg text. Returns the number of key lines matched.
// Keys compare case-insensitively and values use the original 0x49afa0 atoi.
// The reused 0x8e4040 line buffer, the trailing-NUL token and the stale third
// token slot of 0x4565f0 are modeled, so odd lines behave like 0x406640.
std::size_t load_game_config_text(GameConfig& config,std::string_view text);

// Exact writer output (LF endings; the shipped Windows file has CRLF because
// the original CRT opens df.cfg in text mode).
std::string save_game_config_text(const GameConfig& config);

// DFDK.KEY soldier bindings, loaded into 0x4e7ddc as 46 rows of
// {key,mouse,joystick}. mouse is 0 none, 1 left, 2 right, 3 middle.
struct ActionBinding {
    std::uint32_t key=0,mouse=0,joystick=0;
};
inline bool operator==(const ActionBinding& a,const ActionBinding& b){return a.key==b.key&&a.mouse==b.mouse&&a.joystick==b.joystick;}
inline bool operator!=(const ActionBinding& a,const ActionBinding& b){return !(a==b);}
inline constexpr std::size_t action_count=46;
using ActionBindings=std::array<ActionBinding,action_count>;
// Shipped DFDK.KEY values for all 46 rows (names come from the file comments).
ActionBindings default_action_bindings();

struct ActionLoadResult {
    bool section_found=false;
    std::size_t applied=0,rejected=0;
};

// Parse a DFDK.KEY text with the original 0x49b010/0x49b230/0x49b580 reader.
// Loading first converts "\r\n" to '\n' + NUL, tabs to spaces and uppercases
// every byte, and only as many NUL-terminated strings as there were CRLF pairs
// are read. The [KEYS] section then ends at the next '[' line and KEY=index,
// key,mouse,joy lines are split on ", " with atoi values. Rows the text does
// not set keep their previous value, like the loader writing into the zeroed
// 0x4e7ddc table. Rows whose index is not 0..45 or whose fields are missing are
// rejected instead of corrupting memory.
ActionLoadResult load_action_bindings_text(ActionBindings& bindings,std::string_view text);
std::string_view action_name(std::size_t id);

// Command table in the executable at 0x4fb2fc (121 rows, stride 0x2c):
// id, name, key (word +0x24), mouse (+0x26), joystick (+0x28), flags (+0x2a).
struct HotkeyCommand {
    std::uint32_t id=0;
    std::string_view name;
    std::uint16_t key=0,mouse=0,joystick=0,flags=0;
};
const std::vector<HotkeyCommand>& hotkey_commands();
const HotkeyCommand* find_hotkey_command(std::uint32_t id);

// Original 0x47a2a4 test: key_state[key] != 0, then mouse and joystick masks.
// Mouse bits observed at 0xd09348: left 0x1, right 0x2, middle 0x10.
inline constexpr std::uint32_t mouse_left=0x1,mouse_right=0x2,mouse_middle=0x10;
bool hotkey_active(const HotkeyCommand& command,const std::array<std::uint8_t,256>& key_state,
                   std::uint32_t mouse_buttons,std::uint32_t joystick_buttons);

} // namespace dfre
