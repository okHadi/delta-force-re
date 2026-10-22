#pragma once
#include "dfre/input_state.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace dfre {

// Original game command dispatcher 0x477D40 for Delta Force (1998).
// Source: Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// See docs/gameplay-dispatch.md and .local/opencode/gameplay_dispatch-needs.md.

inline constexpr std::uint32_t gameplay_dispatch_function = 0x477d40;
inline constexpr std::uint32_t gameplay_command_row_size = 0x2c;
// Command table flags 0x4FB326 (+0x2A in each row).
inline constexpr std::uint16_t command_flag_actor_gate = 0x0001;  // needed by the first three gate terms
inline constexpr std::uint16_t command_flag_single_player = 0x0010;
// Gate sources.
inline constexpr std::uint32_t local_actor_pointer = 0xc23820;
inline constexpr std::uint32_t actor_flags_offset = 0x6c;
inline constexpr std::uint32_t actor_flags_gate_bit = 0x4;
// Actor fields written by the dispatcher.
inline constexpr std::uint32_t actor_look_yaw_offset = 0x30;
inline constexpr std::uint32_t actor_look_pitch_offset = 0x34;
inline constexpr std::uint32_t actor_stance_offset = 0x74;

// Typed call intent categories. The address is always exact; the category
// only groups the consumer.
enum class DispatchCallKind : std::uint8_t {
    ui_button,        // 0x404BA0
    dialog,           // 0x404CF0 + 0x4799D0
    dialog_text,      // 0x401D50
    message,          // 0x479F00
    format_int,       // 0x493A80 "%i"
    game_event,       // 0x46F3E0
    weapon_fire,      // 0x483C70
    weapon_reload,    // 0x483BD0
    weapon_select,    // 0x483DD0
    weapon_cycle,     // 0x484B70
    weapon_aim,       // 0x484C20
    weapon_supply,    // 0x4848C0, 0x484960, 0x484320, 0x484A50, 0x484C80, 0x484D00, 0x484D80
    weapon_unknown,   // 0x4847C0 (scope command)
    screen_mode,      // 0x4027D0, 0x403280, 0x403480, 0x402D50, 0x428810
    actor_call,       // 0x4604C0
    multiplayer_call, // 0x447FB0, 0x471830, 0x447970, 0x444570, 0x4483F0, 0x448410
    team_check,       // 0x448C90, 0x448CA0, 0x448CD0
    mission_call,     // 0x449600, 0x470FF0
    camera_mode,      // 0x42AC10
    camera_picture,   // 0x477270..0x477300
    screenshot,       // 0x473EB0
    unknown,          // any address without evidence
};

// Argument sentinels. The original passes stack addresses and call results
// through later calls; the port names those positions instead of raw values.
inline constexpr std::uint32_t dispatch_arg_formatted_text = 0xffffff01; // "%i" buffer from 0x493A80
inline constexpr std::uint32_t dispatch_arg_created_handle = 0xffffff02; // result of 0x404CF0
inline constexpr std::uint32_t dispatch_arg_text_handle = 0xffffff03;    // result of 0x401D50
inline constexpr std::uint32_t dispatch_arg_uninit_payload = 0xffffff04; // 16 uninitialized stack bytes
inline constexpr std::uint32_t dispatch_arg_local_actor = 0xffffff05;    // [0xC23820] actor pointer
inline constexpr std::uint32_t dispatch_arg_timeout = 0xffffffff;        // the -1 pass-through argument

struct DispatchCall {
    DispatchCallKind kind = DispatchCallKind::unknown;
    std::uint32_t address = 0;
    // Semantic stack arguments at [esp+4], [esp+8], ...; exact original values.
    std::vector<std::uint32_t> arguments;
    // True when arguments[0] must receive the result of the previous intent.
    bool threaded_result = false;
};
inline bool operator==(const DispatchCall& left,const DispatchCall& right){
    return left.kind==right.kind&&left.address==right.address&&left.arguments==right.arguments
        &&left.threaded_result==right.threaded_result;
}
inline bool operator!=(const DispatchCall& left,const DispatchCall& right){return !(left==right);}

// Every original global this dispatcher reads or writes. Names carry the address.
struct DispatchGlobals {
    // Gate and branch sources read by 0x477D40.
    std::uint32_t multiplayer_67f520=0;
    std::uint32_t mission_67f528=0;
    std::uint32_t mission2_67f52c=0;
    std::uint32_t menu_flag_67dbc0=0;
    std::uint32_t server_67dbb8=0;
    std::uint32_t resolution_index_67f4f4=0;
    std::uint32_t resolution_max_503134=0;
    std::uint32_t lb_size_67f4f8=0;
    std::uint32_t mouse_disabled_503130=0;
    std::uint32_t command_tick_6e471c=0;
    std::uint32_t tick_base_bd4844=0;
    std::uint32_t pause_6e4714=0;
    std::uint32_t pause2_6e4718=0;
    std::uint32_t toggle_6e558c=0;       // 0x6C/0x6D last-game toggle (0x448C90)
    std::uint32_t toggle_6e5590=0;       // 0x6E lock-game toggle (0x448CA0)
    std::uint32_t toggle_6e5594=0;       // 0x6F lock-names toggle (0x448CD0)
    // Values written by the dispatcher.
    std::uint32_t saturation_67f4f0=0;   // 0x12
    std::uint32_t mouse_scale_67f500=0;  // 0x14
    std::uint32_t toggle_67f4fc=0;       // 0x0A flipmouse
    std::uint32_t toggle_67f504=0;       // 0x0E crosshairs
    std::uint32_t hud_color_index_67f508=0;  // 0x0B, cycles over 6 entries
    std::uint32_t hud_color_value_67db58=0;  // 0x0B, 0x67DB40[index]
    std::uint32_t hud_detail_67f50c=0;   // 0x1B cycles 0..2
    std::uint32_t toggle_67f510=0;       // 0x1D pinp
    std::uint32_t toggle_67f514=0;       // 0x11 showgun
    std::uint32_t toggle_67f518=0;       // 0x24 Turbo
    std::uint32_t toggle_67f564=0;       // 0x30 Verbose
    std::uint32_t toggle_503428=0;       // 0x31 SaveScores
    std::uint32_t toggle_67db78=0;       // 0x09 help
    std::uint32_t toggle_67db7c=0;       // 0x25 briefing
    std::uint32_t toggle_67db80=0;       // 0x26 BigMap
    std::uint32_t toggle_67db84=0;       // 0x27 OldMessages
    std::uint32_t toggle_67db88=0;       // 0x28 ShowFriendly
    std::uint32_t toggle_67db8c=0;       // 0x29 MissionGoals
    std::uint32_t toggle_67db90=0;       // 0x2B AudioMacros
    std::uint32_t flag_67db70=0;         // 0x15 escape cascade
    std::uint32_t flag_67db74=0;         // 0x15 escape cascade
    std::uint32_t type_flag_67db6c=0;    // 0x22 Pause secondary and 0x15 escape cascade
    std::uint32_t menu_state_67dbbc=0;   // 0x03/0x04/0x2E exit values
    std::uint8_t flags_67dbc8=0;         // 0x05 sky bit 0x04; 0x20 selects the 484320 path
    std::uint8_t flags_67dbc9=0;         // 0x7B bit 0x08; 0x79 bit 0x01
    std::uint32_t screenshot_67dbf8=0;   // 0x02 PrintScreen state
    std::uint32_t screenshot_bfbb00=0;   // 0x02 PrintScreen state
    std::uint32_t fo_toggle_c23a34=0;    // 0x1E fo; 0x1F tab acts when set
    std::uint32_t fov_c23a28=0;          // 0x168/0x169 zoom scalar
    std::uint32_t map_terrain_c23a2c=0;  // 0x16A/0x16B
    std::uint32_t map_elevation_c23a30=0;// 0x16A/0x16B
    std::uint32_t camera_mode_bd0040=0;  // 0x17C toggles the 0x17D..0x184 picture calls
    std::uint32_t control_c23a00=0;      // movement control word; see movement.hpp bits
    std::uint32_t pulse_c23a04=0;        // 0xA1 Crouch
    std::uint32_t pulse_c23a08=0;        // 0xA2 Prone
    std::uint32_t pulse_c23a0c=0;        // 0xA4 Stand
    std::uint32_t camera_6ba424=0;       // 0x19A CameraLeft; dword covers 0x6BA424..0x6BA427
    std::uint32_t camera_6ba428=0;       // 0x19C/0x19D CameraUp/Down
    std::uint32_t camera_6ba42c=0;       // 0x19E/0x19F CameraZoomIn/Out
    // 0x0B HUD colour table at 0x67DB40 (shipped image holds six zeros).
    std::array<std::uint32_t,6> hud_color_table_67db40{};
};

// Normalized local actor handle for the fields the dispatcher touches.
struct DispatchActor {
    std::uint32_t flags_6c=0;
    std::uint32_t look_yaw_30=0;
    std::uint32_t look_pitch_34=0;
    std::uint32_t stance_74=0;
};

// One dispatch result. The original returns void; the port reports the calls.
struct DispatchResult {
    bool gated=false;                  // true when the command did not pass the gate
    std::vector<DispatchCall> calls;   // in original order, including nested dispatches
};

// 0x477D40. Applies all owned state writes to `globals`/`actor` and appends the
// exact typed calls in original order. `commands` supplies the row flags by id;
// a missing row behaves like flags 0.
DispatchResult dispatch_game_command(std::uint32_t id,std::int32_t value,DispatchGlobals& globals,
                                     DispatchActor& actor,const CommandTable& commands);

} // namespace dfre
