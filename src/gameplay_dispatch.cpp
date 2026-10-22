#include "dfre/gameplay_dispatch.hpp"

#include <initializer_list>

namespace dfre {
namespace {

// Original string and callback addresses used by the dispatch branches.
constexpr std::uint32_t kMiscGroup = 0x4cd55c;          // "Misc"
constexpr std::uint32_t kFormatPercentI = 0x4cd564;     // "%i"
constexpr std::uint32_t kStrMisc06 = 0x4cd550;          // 0x01 command
constexpr std::uint32_t kStrMisc27 = 0x4cd568;          // 0x12 saturation
constexpr std::uint32_t kStrMisc28 = 0x4cd574;          // 0x14 mousescale
constexpr std::uint32_t kStrMisc04 = 0x4cd580;          // 0x28 on
constexpr std::uint32_t kStrMisc05 = 0x4cd58c;          // 0x28 off
constexpr std::uint32_t kStrMisc20 = 0x4cd598;          // 0x30 on
constexpr std::uint32_t kStrMisc21 = 0x4cd5a4;          // 0x30 off
constexpr std::uint32_t kSaveScoresOn = 0x4cd5b0;       // "!Save Scores: On"
constexpr std::uint32_t kSaveScoresOff = 0x4cd5c4;      // "!Save Scores: Off"
constexpr std::uint32_t kStrMisc22 = 0x4cd5d8;          // 0x65 talk
constexpr std::uint32_t kStrMisc23 = 0x4cd5e4;          // 0x64 stalk
constexpr std::uint32_t kStrMisc24 = 0x4cd5f0;          // 0x68 talkred
constexpr std::uint32_t kStrMisc25 = 0x4cd5fc;          // 0x69 talkblue
constexpr std::uint32_t kStrMisc18 = 0x4cd608;          // 0x6C success
constexpr std::uint32_t kStrMisc19 = 0x4cd614;          // 0x6C failure
constexpr std::uint32_t kStrMisc29 = 0x4cd620;          // 0x6D success
constexpr std::uint32_t kStrMisc30 = 0x4cd62c;          // 0x6D failure
constexpr std::uint32_t kStrMisc31 = 0x4cd638;          // 0x6E success
constexpr std::uint32_t kStrMisc32 = 0x4cd644;          // 0x6E failure
constexpr std::uint32_t kStrMisc26 = 0x4cd650;          // 0x79 double ammo on
constexpr std::uint32_t kCallbackCommand = 0x479830;    // 0x01
constexpr std::uint32_t kCallbackTalk = 0x477a00;       // 0x65
constexpr std::uint32_t kCallbackStalk = 0x477ac0;      // 0x64
constexpr std::uint32_t kCallbackTalkRed = 0x477b80;    // 0x68
constexpr std::uint32_t kCallbackTalkBlue = 0x477c40;   // 0x69
constexpr std::uint32_t kDialogTimeout = 0x3c;

// The original gate reads the flags byte at 0x4FB326 + id*0x2C: the row with
// row index == command id, not the row whose id field matches.
std::uint16_t command_flags(const CommandTable& commands,std::uint32_t id) {
    const auto& rows = commands.rows();
    if (id < rows.size()) return rows[id].flags;
    return 0;
}
void toggle_low_byte(std::uint32_t& field,std::uint32_t mask) noexcept {
    field = (field & ~0xffu) | ((field & 0xffu) ^ mask);
}
// x87 fild/fmul then the 0x49BAE6 helper (frndint with the control word set to
// truncate) then fistp. Out-of-range values store the integer indefinite value.
std::int32_t x87_truncate(const std::int32_t value,const double factor) noexcept {
    const long double product = static_cast<long double>(value) * static_cast<long double>(factor);
    if (product >= 2147483648.0L || product < -2147483648.0L) return std::int32_t(0x80000000);
    return static_cast<std::int32_t>(product);
}
void call(std::vector<DispatchCall>& calls,DispatchCallKind kind,std::uint32_t address,
          std::initializer_list<std::uint32_t> arguments,bool threaded=false) {
    calls.push_back(DispatchCall{kind,address,std::vector<std::uint32_t>(arguments),threaded});
}
// 0x404CF0(group, key, -1) then 0x479F00(created handle, -1).
void message_key(std::vector<DispatchCall>& calls,std::uint32_t key) {
    call(calls,DispatchCallKind::dialog,0x404cf0,{kMiscGroup,key,dispatch_arg_timeout});
    call(calls,DispatchCallKind::message,0x479f00,{dispatch_arg_created_handle,dispatch_arg_timeout},true);
}
// 0x479F00(literal string, -1).
void message_string(std::vector<DispatchCall>& calls,std::uint32_t text) {
    call(calls,DispatchCallKind::message,0x479f00,{text,dispatch_arg_timeout});
}
// 0x404CF0(group, key, 0, callback, 0x3C) then 0x4799D0(created handle, 0, callback, 0x3C).
void dialog(std::vector<DispatchCall>& calls,std::uint32_t key,std::uint32_t callback) {
    call(calls,DispatchCallKind::dialog,0x404cf0,{kMiscGroup,key,0,callback,kDialogTimeout});
    call(calls,DispatchCallKind::dialog,0x4799d0,
         {dispatch_arg_created_handle,0,callback,kDialogTimeout},true);
}
// 0x493A80("%i") plus the 0x404CF0/0x401D50/0x479F00 chain for a formatted value.
void value_message(std::vector<DispatchCall>& calls,std::uint32_t key,std::uint32_t value) {
    call(calls,DispatchCallKind::format_int,0x493a80,{dispatch_arg_formatted_text,kFormatPercentI,value});
    call(calls,DispatchCallKind::dialog,0x404cf0,
         {kMiscGroup,key,dispatch_arg_formatted_text,0,0});
    call(calls,DispatchCallKind::dialog_text,0x401d50,
         {dispatch_arg_created_handle,dispatch_arg_formatted_text,0,0},true);
    call(calls,DispatchCallKind::message,0x479f00,{dispatch_arg_text_handle,dispatch_arg_timeout},true);
}
}

DispatchResult dispatch_game_command(std::uint32_t id,std::int32_t value,DispatchGlobals& globals,
                                     DispatchActor& actor,const CommandTable& commands) {
    DispatchResult result;
    const std::uint16_t flags = command_flags(commands,id);
    const bool actor_gate = (actor.flags_6c & actor_flags_gate_bit) != 0;
    const bool flagged = (flags & command_flag_actor_gate) != 0;
    // 0x477D47-0x477DF7 gate.
    if (flagged && actor_gate) { result.gated = true; return result; }
    if (flagged && globals.pause_6e4714 != 0) { result.gated = true; return result; }
    if (globals.menu_flag_67dbc0 != 0 && globals.multiplayer_67f520 != 0 && flagged) {
        result.gated = true; return result;
    }
    if ((flags & command_flag_single_player) != 0 && globals.multiplayer_67f520 != 0) {
        result.gated = true; return result;
    }
    auto& calls = result.calls;
    const std::uint32_t uvalue = std::uint32_t(value);
    if (id < 0x8c) {
        if (id < 0x24) {
            if (id > 0xd) {
                if (id < 0xf) { globals.toggle_67f504 ^= 1; return result; }          // 0x0E crosshairs
                if (id > 0x1b) {
                    if (id < 0x1d) return result;                                     // 0x1C tracers
                    if (id < 0x20) {
                        if (id < 0x1e) { globals.toggle_67f510 ^= 1; return result; } // 0x1D pinp
                        if (id > 0x1e) {                                              // 0x1F tab
                            if (globals.fo_toggle_c23a34 == 0) return result;
                            call(calls,DispatchCallKind::weapon_supply,0x484a50,{});
                            return result;
                        }
                        toggle_low_byte(globals.fo_toggle_c23a34,1);                  // 0x1E fo
                        return result;
                    }
                    if (id < 0x21) { call(calls,DispatchCallKind::weapon_supply,0x484c80,{}); return result; } // 0x20
                    if (id < 0x22) { call(calls,DispatchCallKind::actor_call,0x4604c0,{dispatch_arg_local_actor}); return result; } // 0x21 Drop
                    if (id > 0x22) { call(calls,DispatchCallKind::weapon_aim,0x484c20,{}); return result; } // 0x23
                    if (globals.multiplayer_67f520 != 0) return result;              // 0x22 Pause
                    globals.type_flag_67db6c = globals.multiplayer_67f520;
                    toggle_low_byte(globals.pause_6e4714,1);
                    return result;
                }
                if (id < 0x12) {
                    if (id < 0x10) {                                                 // 0x0F lbsize
                        call(calls,DispatchCallKind::ui_button,0x404ba0,
                             {globals.lb_size_67f4f8 == 1 ? 2u : 1u});
                        return result;
                    }
                    if (id != 0x11) return result;                                   // 0x10 gamma
                    globals.toggle_67f514 ^= 1;                                      // 0x11 showgun
                    return result;
                }
                if (id < 0x13) {                                                     // 0x12 saturation
                    if (value != 0) {
                        globals.saturation_67f4f0 = uvalue;
                        call(calls,DispatchCallKind::screen_mode,0x4027d0,{});
                    }
                    value_message(calls,kStrMisc27,globals.saturation_67f4f0);
                    return result;
                }
                if (id < 0x15) {
                    if (id != 0x14) return result;                                   // 0x13 saturation (unhandled)
                    if (value != 0) globals.mouse_scale_67f500 = uvalue;             // 0x14 mousescale
                    value_message(calls,kStrMisc28,globals.mouse_scale_67f500);
                    return result;
                }
                if (id > 0x15) {
                    if (id != 0x1b) return result;                                   // 0x16..0x1A
                    globals.hud_detail_67f50c += 1;                                  // 0x1B huddetail
                    if (globals.hud_detail_67f50c > 2) globals.hud_detail_67f50c = 0;
                    return result;
                }
                // 0x15 escape cascade.
                if (globals.pause_6e4714 != 0 || globals.pause2_6e4718 != 0) {
                    globals.pause2_6e4718 = 0;
                    globals.pause_6e4714 = 0;
                    return result;
                }
                if (globals.toggle_67db90 != 0) { globals.toggle_67db90 = globals.pause_6e4714; return result; }
                if (globals.toggle_67db84 != 0) { globals.toggle_67db84 = globals.pause_6e4714; return result; }
                if (globals.type_flag_67db6c != 0) { globals.type_flag_67db6c = globals.pause_6e4714; return result; }
                if (globals.toggle_67db7c != 0) { globals.toggle_67db7c = globals.pause_6e4714; return result; }
                if (globals.toggle_67db78 != 0) { globals.toggle_67db78 = globals.pause_6e4714; return result; }
                if (globals.map_terrain_c23a2c != 0) { globals.map_terrain_c23a2c = globals.pause2_6e4718; return result; }
                if (globals.toggle_67db80 == 0) {
                    globals.flag_67db70 = globals.toggle_67db84;
                    globals.flag_67db74 = globals.toggle_67db84;
                    globals.type_flag_67db6c = 1;
                    return result;
                }
                globals.toggle_67db80 = globals.toggle_67db90;
                return result;
            }
            if (id > 6) {
                if (id < 8) {                                                        // 0x07 fullScreen
                    call(calls,DispatchCallKind::screen_mode,0x403280,{globals.mouse_disabled_503130 == 0 ? 1u : 0u});
                    call(calls,DispatchCallKind::screen_mode,0x402d50,{0});
                    call(calls,DispatchCallKind::screen_mode,0x428810,{0});
                    return result;
                }
                if (id < 10) {                                                       // 0x08/0x09
                    if (id > 8) { globals.toggle_67db78 ^= 1; return result; }       // 0x09 help
                    const std::int32_t max_plus = std::int32_t(globals.resolution_max_503134 + 1);
                    const std::uint32_t count = std::int32_t(globals.resolution_index_67f4f4) < max_plus
                        ? 0u : std::uint32_t(max_plus);                              // 0x08 resolution
                    call(calls,DispatchCallKind::screen_mode,0x403480,{count});
                    call(calls,DispatchCallKind::screen_mode,0x402d50,{0});
                    call(calls,DispatchCallKind::screen_mode,0x428810,{0});
                    return result;
                }
                if (id < 0xb) { globals.toggle_67f4fc ^= 1; return result; }         // 0x0A flipmouse
                if (id > 0xb) {
                    if (id > 0xc) {                                                  // 0x0D restart
                        call(calls,DispatchCallKind::game_event,0x46f3e0,
                             {0x13,dispatch_arg_uninit_payload,0x10});
                        return result;
                    }
                    globals.server_67dbb8 ^= 1;                                      // 0x0C server
                    return result;
                }
                globals.hud_color_index_67f508 += 1;                                 // 0x0B hudcolor
                if (globals.hud_color_index_67f508 > 5) globals.hud_color_index_67f508 = 0;
                globals.hud_color_value_67db58 =
                    globals.hud_color_table_67db40[globals.hud_color_index_67f508 % 6];
                return result;
            }
            if (id > 2) {
                if (id < 4) { globals.menu_state_67dbbc = 1; return result; }        // 0x03 exit
                if (id < 5) { globals.menu_state_67dbbc = 2; return result; }        // 0x04 quit
                if (id > 5) { call(calls,DispatchCallKind::weapon_unknown,0x4847c0,{}); return result; } // 0x06 scope
                globals.flags_67dbc8 ^= 4;                                           // 0x05 sky
                return result;
            }
            if (id == 0) return result;                                              // 0x00 null
            if (id > 1) {                                                            // 0x02 PrintScreen
                call(calls,DispatchCallKind::screenshot,0x473eb0,{0x67dbf8,0xbfbb00});
                return result;
            }
            dialog(calls,kStrMisc06,kCallbackCommand);                               // 0x01 command
            return result;
        }
        if (id < 0x25) { globals.toggle_67f518 ^= 1; return result; }                // 0x24 Turbo
        if (id < 0x65) {
            if (id < 0x2b) {
                if (id < 0x27) {
                    if (id > 0x25) { globals.toggle_67db80 ^= 1; return result; }    // 0x26 BigMap
                    globals.toggle_67db7c ^= 1;                                      // 0x25 briefing
                    return result;
                }
                if (id < 0x28) { globals.toggle_67db84 ^= 1; return result; }        // 0x27 OldMessages
                if (id < 0x29) {                                                     // 0x28 ShowFriendly
                    globals.toggle_67db88 ^= 1;
                    message_key(calls,globals.toggle_67db88 == 0 ? kStrMisc05 : kStrMisc04);
                    return result;
                }
                if (id > 0x29) { call(calls,DispatchCallKind::weapon_supply,0x484d80,{}); return result; } // 0x2A NextFlag
                globals.toggle_67db8c ^= 1;                                          // 0x29 MissionGoals
                return result;
            }
            if (id < 0x2c) { globals.toggle_67db90 ^= 1; return result; }            // 0x2B AudioMacros
            if (id < 0x2f) {
                if (id < 0x2d) { call(calls,DispatchCallKind::multiplayer_call,0x447fb0,{uvalue}); return result; } // 0x2C Punt
                if (id < 0x2e) { call(calls,DispatchCallKind::multiplayer_call,0x471830,{uvalue}); return result; } // 0x2D Bozo
                globals.menu_state_67dbbc = 5;                                       // 0x2E (unbound row)
                return result;
            }
            if (id < 0x30) { call(calls,DispatchCallKind::weapon_supply,0x484d00,{}); return result; } // 0x2F PrevWaypoint
            if (id < 0x31) {                                                         // 0x30 Verbose
                globals.toggle_67f564 ^= 1;
                message_key(calls,globals.toggle_67f564 == 0 ? kStrMisc21 : kStrMisc20);
                return result;
            }
            if (id < 0x32) {                                                         // 0x31 SaveScores
                globals.toggle_503428 ^= 1;
                message_string(calls,globals.toggle_503428 == 0 ? kSaveScoresOff : kSaveScoresOn);
                return result;
            }
            if (id != 100) return result;                                            // 0x32..0x63
            if (std::uint32_t(globals.command_tick_6e471c - globals.tick_base_bd4844) < 9) return result;
            if (globals.multiplayer_67f520 == 0) return result;
            dialog(calls,kStrMisc23,kCallbackStalk);                                 // 0x64 stalk
            return result;
        }
        if (id < 0x66) {                                                             // 0x65 talk
            if (std::uint32_t(globals.command_tick_6e471c - globals.tick_base_bd4844) < 9) return result;
            if (globals.multiplayer_67f520 == 0) return result;
            dialog(calls,kStrMisc22,kCallbackTalk);
            return result;
        }
        if (id > 0x6b) {
            if (id < 0x6d) {                                                         // 0x6C LastGame
                if (globals.multiplayer_67f520 == 0) return result;
                if (globals.mission_67f528 == 0) return result;
                toggle_low_byte(globals.toggle_6e558c,1);
                message_key(calls,globals.toggle_6e558c == 0 ? kStrMisc19 : kStrMisc18);
                return result;
            }
            if (id < 0x6e) {                                                         // 0x6D LockGame
                if (globals.multiplayer_67f520 == 0) return result;
                if (globals.mission_67f528 == 0) return result;
                toggle_low_byte(globals.toggle_6e5590,1);
                if (globals.toggle_6e5590 != 0) call(calls,DispatchCallKind::multiplayer_call,0x446630,{});
                message_key(calls,globals.toggle_6e5590 == 0 ? kStrMisc30 : kStrMisc29);
                return result;
            }
            if (id < 0x6f) {                                                         // 0x6E LockNames
                if (globals.multiplayer_67f520 == 0) return result;
                if (globals.mission_67f528 == 0) return result;
                toggle_low_byte(globals.toggle_6e5594,1);
                message_key(calls,globals.toggle_6e5594 == 0 ? kStrMisc32 : kStrMisc31);
                return result;
            }
            if (id < 0x79) {
                if (id != 0x78) return result;                                       // 0x6F..0x77
                call(calls,DispatchCallKind::weapon_supply,0x4848c0,{});             // 0x78 resupply
                return result;
            }
            if (id < 0x7a) {                                                         // 0x79 double ammo cheat
                // Original 0x484960 is a thunk: xor byte [0x67DBC9],1 then jump 0x4848C0.
                globals.flags_67dbc9 ^= 1;
                call(calls,DispatchCallKind::weapon_supply,0x4848c0,{});
                if ((globals.flags_67dbc9 & 1) == 0) return result;
                message_key(calls,kStrMisc26);
                return result;
            }
            if (id < 0x7b) {                                                         // 0x7A resupply cheat
                actor.stance_74 = (actor.stance_74 & 0xffff0000u) | 0x4000u;
                return result;
            }
            if (id < 0x7c) { globals.flags_67dbc9 ^= 8; return result; }               // 0x7B
            if (id != 0x7c) return result;                                           // 0x7C raindrops cheat
            call(calls,DispatchCallKind::multiplayer_call,0x447970,{1,8});
            return result;
            return result;
        }
        if (id < 0x68) {
            if (id > 0x66) { call(calls,DispatchCallKind::multiplayer_call,0x444570,{uvalue}); return result; } // 0x67 netdelay
            // 0x66 killlist.
            if (globals.mission2_67f52c == 0 || globals.mission_67f528 == 0 || globals.multiplayer_67f520 == 0) {
                if (globals.mission_67f528 != 0 && globals.multiplayer_67f520 != 0) {
                    call(calls,DispatchCallKind::mission_call,0x449600,{});
                    return result;
                }
            } else if (globals.server_67dbb8 != 0) {
                call(calls,DispatchCallKind::mission_call,0x449600,{});
                return result;
            }
            call(calls,DispatchCallKind::mission_call,0x470ff0,{});
            return result;
        }
        if (id < 0x69) {                                                             // 0x68 talkred
            if (globals.multiplayer_67f520 == 0) return result;
            if (globals.mission_67f528 == 0) return result;
            dialog(calls,kStrMisc24,kCallbackTalkRed);
            return result;
        }
        if (id > 0x69) {                                                             // 0x6A/0x6B
            if (id > 0x6a) { call(calls,DispatchCallKind::multiplayer_call,0x448410,{uvalue}); return result; }
            call(calls,DispatchCallKind::multiplayer_call,0x4483f0,{});
            return result;
        }
        if (globals.multiplayer_67f520 == 0) return result;                          // 0x69 talkblue
        if (globals.mission_67f528 == 0) return result;
        dialog(calls,kStrMisc25,kCallbackTalkBlue);
        return result;
    }
    // id >= 0x8C.
    if (id < 0x8d) { call(calls,DispatchCallKind::weapon_fire,0x483c70,{}); return result; }  // 0x8C attack_1
    if (id < 0xba) {
        if (id < 0x9b) {
            if (id < 0x93) {
                if (id > 0x8e) {
                    if (id < 0x90) {                                                 // 0x8F move_forward
                        if ((globals.flags_67dbc8 & 0x20) != 0) {
                            call(calls,DispatchCallKind::weapon_supply,0x484320,{5});
                            return result;
                        }
                        globals.control_c23a00 |= 0x2;
                        return result;
                    }
                    if (id < 0x91) { globals.control_c23a00 |= 0x400; return result; } // 0x90 move_jump
                    if (id > 0x91) {                                                 // 0x92 look_up
                        if ((globals.flags_67dbc8 & 0x20) == 0) {
                            globals.control_c23a00 |= 0x20;
                            return result;
                        }
                        call(calls,DispatchCallKind::weapon_supply,0x484320,{1});
                        return result;
                    }
                    if ((globals.flags_67dbc8 & 0x20) != 0) {                        // 0x91 look_down
                        call(calls,DispatchCallKind::weapon_supply,0x484320,{2});
                        return result;
                    }
                    globals.control_c23a00 |= 0x40;
                    return result;
                }
                if (id > 0x8d) {                                                     // 0x8E move_back
                    if ((globals.flags_67dbc8 & 0x20) != 0) {
                        call(calls,DispatchCallKind::weapon_supply,0x484320,{6});
                        return result;
                    }
                    globals.control_c23a00 |= 0x4;
                    return result;
                }
                call(calls,DispatchCallKind::weapon_fire,0x483c70,{});               // 0x8D attack_2
                return result;
            }
            if (id < 0x94) {                                                         // 0x93 slide_left
                if ((globals.flags_67dbc8 & 0x20) != 0) {
                    call(calls,DispatchCallKind::weapon_supply,0x484320,{3});
                    return result;
                }
                globals.control_c23a00 |= 0x8;
                return result;
            }
            if (id < 0x97) {
                if (id > 0x94) {
                    if (id > 0x95) { globals.control_c23a00 |= 0x200; return result; } // 0x96 turn_right
                    globals.control_c23a00 |= 0x100;                                 // 0x95 turn_left
                    return result;
                }
                if ((globals.flags_67dbc8 & 0x20) != 0) {                            // 0x94 slide_right
                    call(calls,DispatchCallKind::weapon_supply,0x484320,{4});
                    return result;
                }
                globals.control_c23a00 |= 0x10;
                return result;
            }
            if (id < 0x98) { globals.control_c23a00 |= 0x2000; return result; }      // 0x97 fly_down
            if (id < 0x99) { actor.look_pitch_34 = 0; return result; }               // 0x98 center_view
            if (id < 0x9a) { globals.control_c23a00 |= 0x4000; return result; }      // 0x99 fly_up
            // 0x9A toggle_item: nested 0x0D dispatch when the actor gate or menu is set.
            if (actor_gate || globals.menu_flag_67dbc0 != 0) {
                const auto nested = dispatch_game_command(0xd,0,globals,actor,commands);
                calls.insert(calls.end(),nested.calls.begin(),nested.calls.end());
            }
            return result;
        }
        if (id < 0x9c) {                                                             // 0x9B look_up_abs
            actor.look_pitch_34 = actor.look_pitch_34 + uvalue;
            return result;
        }
        if (id < 0xa3) {
            if (id < 0x9e) {
                if (id > 0x9c) {                                                     // 0x9D turn_left_abs
                    actor.look_yaw_30 = actor.look_yaw_30 + uvalue;
                    return result;
                }
                actor.look_pitch_34 = actor.look_pitch_34 - uvalue;                  // 0x9C look_dn_abs
                return result;
            }
            if (id < 0x9f) {                                                         // 0x9E turn_right_abs
                actor.look_yaw_30 = actor.look_yaw_30 - uvalue;
                return result;
            }
            if (id > 0xa0) {
                if (id > 0xa1) { globals.pulse_c23a08 = 1; return result; }          // 0xA2 Prone
                globals.pulse_c23a04 = 1;                                            // 0xA1 Crouch
                return result;
            }
            if (id == 0x9f) { globals.control_c23a00 |= 1; return result; }          // 0x9F NoRun
            return result;                                                           // 0xA0 Slide: no branch
        }
        if (id > 0xa3) {
            if (id >= 0xb4) { call(calls,DispatchCallKind::weapon_select,0x483dd0,{id - 0xb4}); return result; } // 0xB4..0xB9
            if (id == 0xa4) { globals.pulse_c23a0c = 1; return result; }             // 0xA4 Stand
            return result;                                                           // 0xA5..0xB3
        }
        globals.control_c23a00 = 0;                                                  // 0xA3
        return result;
    }
    if (id < 0xbb) { call(calls,DispatchCallKind::weapon_select,0x483dd0,{id - 0xb4}); return result; } // 0xBA
    if (id < 0x180) {
        if (id < 0x168) {
            if (id < 0xbe) { call(calls,DispatchCallKind::weapon_select,0x483dd0,{id - 0xb4}); return result; } // 0xBB..0xBD
            if (id < 0xbf) { call(calls,DispatchCallKind::weapon_reload,0x483bd0,{}); return result; } // 0xBE magazine
            if (id < 0xc0) { call(calls,DispatchCallKind::weapon_cycle,0x484b70,{1}); return result; } // 0xBF cycle
            if (id == 0xc0) { call(calls,DispatchCallKind::weapon_cycle,0x484b70,{dispatch_arg_timeout}); return result; } // 0xC0 cycleb
            return result;                                                           // 0xC1..0x167
        }
        if (id < 0x169) {                                                            // 0x168 zoomin
            globals.fov_c23a28 = std::uint32_t(x87_truncate(std::int32_t(globals.fov_c23a28),0.9));
            if (std::int32_t(globals.fov_c23a28) < 0x4000) globals.fov_c23a28 = 0x4000;
            return result;
        }
        if (id < 0x16a) {                                                            // 0x169 zoomout
            globals.fov_c23a28 = std::uint32_t(x87_truncate(std::int32_t(globals.fov_c23a28),1.1));
            if (double(std::int32_t(globals.fov_c23a28)) > 235929.6) globals.fov_c23a28 = 0x39999;
            return result;
        }
        if (id < 0x16b) {                                                            // 0x16A mapterrain
            if (globals.map_elevation_c23a30 != 0) {
                globals.map_elevation_c23a30 = 0;
                globals.map_terrain_c23a2c = 1;
                return result;
            }
            toggle_low_byte(globals.map_terrain_c23a2c,1);
            return result;
        }
        if (id < 0x17c) {                                                            // 0x16B mapelevation
            if (id != 0x16b) return result;                                          // 0x16C..0x17B
            if (globals.map_elevation_c23a30 != 1) {
                globals.map_elevation_c23a30 = 1;
                globals.map_terrain_c23a2c = 1;
                return result;
            }
            toggle_low_byte(globals.map_terrain_c23a2c,1);
            return result;
        }
        if (id < 0x17d) { toggle_low_byte(globals.camera_mode_bd0040,1); return result; } // 0x17C debug_hud
        if (id < 0x17e) { if (globals.camera_mode_bd0040 != 0) call(calls,DispatchCallKind::camera_picture,0x477270,{}); return result; } // 0x17D
        if (id < 0x17f) { if (globals.camera_mode_bd0040 != 0) call(calls,DispatchCallKind::camera_picture,0x477280,{}); return result; } // 0x17E
        if (globals.camera_mode_bd0040 != 0) call(calls,DispatchCallKind::camera_picture,0x4772d0,{}); // 0x17F
        return result;
    }
    if (id < 0x181) {                                                                // 0x180
        if (globals.camera_mode_bd0040 != 0) call(calls,DispatchCallKind::camera_picture,0x4772e0,{});
        return result;
    }
    if (id < 0x192) {
        if (id < 0x183) {
            if (id < 0x182) {                                                        // 0x181
                if (globals.camera_mode_bd0040 != 0) call(calls,DispatchCallKind::camera_picture,0x4772b0,{});
            } else if (globals.camera_mode_bd0040 != 0) {                            // 0x182
                call(calls,DispatchCallKind::camera_picture,0x477290,{});
            }
            return result;
        }
        if (id < 0x184) {                                                            // 0x183
            if (globals.camera_mode_bd0040 != 0) call(calls,DispatchCallKind::camera_picture,0x4772f0,{});
            return result;
        }
        if (id > 0x18f) {                                                            // 0x190/0x191
            call(calls,DispatchCallKind::camera_mode,0x42ac10,{dispatch_arg_local_actor,id - 0x190});
            return result;
        }
        if (id == 0x184 && globals.camera_mode_bd0040 != 0) {                        // 0x184
            call(calls,DispatchCallKind::camera_picture,0x477300,{});
        }
        return result;                                                               // 0x185..0x18F
    }
    if (id < 0x193) { call(calls,DispatchCallKind::camera_mode,0x42ac10,{dispatch_arg_local_actor,2}); return result; } // 0x192
    if (id < 0x19c) {
        if (id < 0x19a) {
            if (id == 0x193) call(calls,DispatchCallKind::camera_mode,0x42ac10,{dispatch_arg_local_actor,3}); // 0x193
            return result;                                                           // 0x194..0x199
        }
        if (id < 0x19b) {                                                            // 0x19A CameraLeft
            if (globals.camera_mode_bd0040 == 0) globals.camera_6ba424 -= 0x2000000;
            return result;
        }
        if (globals.camera_mode_bd0040 == 0) {                                       // 0x19B CameraRight
            // Byte at 0x6BA427 is the high byte of the 0x6BA424 dword.
            const std::uint32_t high = (globals.camera_6ba424 >> 24) + 2;
            globals.camera_6ba424 = (globals.camera_6ba424 & 0x00ffffffu) | ((high & 0xffu) << 24);
        }
        return result;
    }
    if (id < 0x19e) {                                                                // 0x19C/0x19D
        if (globals.camera_mode_bd0040 == 0) {
            const std::uint32_t delta = id == 0x19c ? 0xfe000000u : 0x2000000u;
            const std::uint32_t sum = globals.camera_6ba428 + delta;
            globals.camera_6ba428 = sum;
            if (sum <= 0x80000000u) {
                if (sum > 0x40000000u) globals.camera_6ba428 = 0x40000000u;
            } else if (sum < 0xd0000000u) {
                globals.camera_6ba428 = 0xd0000000u;
            }
        }
        return result;
    }
    if (id < 0x19f) {                                                                // 0x19E CameraZoomIn
        if (globals.camera_mode_bd0040 == 0) {
            const std::uint32_t sum = globals.camera_6ba42c - 0x2000u;
            globals.camera_6ba42c = sum;
            if (std::int32_t(sum) < 0x10000) globals.camera_6ba42c = 0x10000;
        }
        return result;
    }
    if (id == 0x19f && globals.camera_mode_bd0040 == 0) {                            // 0x19F CameraZoomOut
        const std::uint32_t sum = globals.camera_6ba42c + 0x2000u;
        globals.camera_6ba42c = sum;
        if (std::int32_t(sum) > 0x1f40000) globals.camera_6ba42c = 0x1f40000;
    }
    return result;
}
} // namespace dfre
