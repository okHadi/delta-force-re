#include "dfre/gameplay_dispatch.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
using dfre::CommandTable;
using dfre::DispatchActor;
using dfre::DispatchCall;
using dfre::DispatchGlobals;
using dfre::DispatchResult;

void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}

struct Case {
    DispatchGlobals g;
    DispatchActor a;
    DispatchResult result;
};

Case run(const CommandTable& table,std::uint32_t id,std::int32_t value) {
    Case c;
    c.result=dfre::dispatch_game_command(id,value,c.g,c.a,table);
    return c;
}
void expect_call(const DispatchCall& call,dfre::DispatchCallKind kind,std::uint32_t address,
                 std::initializer_list<std::uint32_t> args,bool threaded=false) {
    check(call.kind==kind,"call kind changed");
    check(call.address==address,"call address changed");
    if(call.arguments!=std::vector<std::uint32_t>(args)){
        std::cerr<<"call 0x"<<std::hex<<address<<" actual args:";
        for(auto v:call.arguments)std::cerr<<" 0x"<<v;
        std::cerr<<" expected:";
        for(auto v:args)std::cerr<<" 0x"<<v;
        std::cerr<<'\n';
        throw std::runtime_error("call arguments changed");
    }
    check(call.threaded_result==threaded,"call threading changed");
}
// Row index == command id: the original gate reads flags at 0x4FB326 + id*0x2C.
void set_row(CommandTable& table,std::uint32_t id,std::uint16_t flags) {
    check(id<table.rows().size(),"row outside the shipped table");
    table.at(id).flags=flags;
}
}
int main(){try{
    CommandTable table;

    // Gate: actor flags bit 2, pause, menu+multiplayer, single-player flag.
    {
        CommandTable gated;
        set_row(gated,0x1d,1);
        auto c=run(gated,0x1d,0);
        check(!c.result.gated&&c.g.toggle_67f510==1,"flagged command did not pass the open gate");
        c.a.flags_6c=4;
        c.result=dfre::dispatch_game_command(0x1d,0,c.g,c.a,gated);
        check(c.result.gated&&c.g.toggle_67f510==1,"actor gate did not block");
        c.a.flags_6c=0;c.g=DispatchGlobals{};c.g.pause_6e4714=1;
        c.result=dfre::dispatch_game_command(0x1d,0,c.g,c.a,gated);
        check(c.result.gated,"pause gate did not block");
        c.g=DispatchGlobals{};c.g.menu_flag_67dbc0=1;c.g.multiplayer_67f520=1;
        c.result=dfre::dispatch_game_command(0x1d,0,c.g,c.a,gated);
        check(c.result.gated,"menu/multiplayer gate did not block");
        // Flags bit 4 is the single-player-only gate.
        CommandTable spicy;
        set_row(spicy,0x44,0x10);
        Case c3;
        c3.result=dfre::dispatch_game_command(0x44,0,c3.g,c3.a,spicy);
        check(!c3.result.gated,"single-player flag blocked single player");
        c3.g.multiplayer_67f520=1;
        c3.result=dfre::dispatch_game_command(0x44,0,c3.g,c3.a,spicy);
        check(c3.result.gated,"single-player gate did not block in multiplayer");
        // An unflagged command passes every gate source.
        CommandTable open;
        set_row(open,0x1d,0);
        Case c4;c4.a.flags_6c=4;c4.g.pause_6e4714=1;c4.g.menu_flag_67dbc0=1;
        c4.g.multiplayer_67f520=1;
        c4.result=dfre::dispatch_game_command(0x1d,0,c4.g,c4.a,open);
        check(!c4.result.gated&&c4.g.toggle_67f510==1,"unflagged command was gated");
    }

    // Movement control bits, 484320 alternative path, jump, look, slide, turn.
    {
        auto c=run(table,0x8e,0);check(c.g.control_c23a00==0x4&&c.result.calls.empty(),"0x8e path changed");
        c=run(table,0x8f,0);check(c.g.control_c23a00==0x2,"0x8f path changed");
        c=run(table,0x90,0);check(c.g.control_c23a00==0x400,"0x90 path changed");
        c=run(table,0x91,0);check(c.g.control_c23a00==0x40,"0x91 path changed");
        c=run(table,0x92,0);check(c.g.control_c23a00==0x20,"0x92 path changed");
        c=run(table,0x93,0);check(c.g.control_c23a00==0x8,"0x93 path changed");
        c=run(table,0x94,0);check(c.g.control_c23a00==0x10,"0x94 path changed");
        c=run(table,0x95,0);check(c.g.control_c23a00==0x100,"0x95 path changed");
        c=run(table,0x96,0);check(c.g.control_c23a00==0x200,"0x96 path changed");
        c=run(table,0x97,0);check(c.g.control_c23a00==0x2000,"0x97 path changed");
        c=run(table,0x99,0);check(c.g.control_c23a00==0x4000,"0x99 path changed");
        c=run(table,0x9f,0);check(c.g.control_c23a00==1,"0x9f path changed");
        c=run(table,0x98,0);check(c.a.look_pitch_34==0&&c.result.calls.empty(),"0x98 path changed");
        c=run(table,0xa1,0);check(c.g.pulse_c23a04==1,"0xa1 path changed");
        c=run(table,0xa2,0);check(c.g.pulse_c23a08==1,"0xa2 path changed");
        c=run(table,0xa4,0);check(c.g.pulse_c23a0c==1,"0xa4 path changed");
        c=run(table,0xa3,0);check(c.g.control_c23a00==0,"0xa3 path changed");
        c=run(table,0xa0,0);check(c.result.calls.empty()&&c.g.control_c23a00==0,"0xa0 was handled");
        // Look additions keep 32-bit wrap units.
        c=run(table,0x9b,25);check(c.a.look_pitch_34==25,"0x9b path changed");
        c=run(table,0x9c,25);check(c.a.look_pitch_34==std::uint32_t(-25),"0x9c path changed");
        c=run(table,0x9d,25);check(c.a.look_yaw_30==25,"0x9d path changed");
        c=run(table,0x9e,25);check(c.a.look_yaw_30==std::uint32_t(-25),"0x9e path changed");
        c=run(table,0x9b,-0x80000000);check(c.a.look_pitch_34==0x80000000u,"look wrap changed");
        // 0x67DBC8 bit 0x20 selects the 484320 helper path.
        {
            Case h;h.g.flags_67dbc8=0x20;
            h.result=dfre::dispatch_game_command(0x8e,0,h.g,h.a,table);
            check(h.result.calls.size()==1&&h.g.control_c23a00==0,"0x8e helper path changed");
            expect_call(h.result.calls[0],dfre::DispatchCallKind::weapon_supply,0x484320,{6});
            for (std::uint32_t id : {0x8fu,0x91u,0x92u,0x93u,0x94u}) {
                Case h2;h2.g.flags_67dbc8=0x20;
                h2.result=dfre::dispatch_game_command(id,0,h2.g,h2.a,table);
                check(h2.result.calls.size()==1,"helper path missing");
                check(h2.g.control_c23a00==0,"helper path wrote control bits");
            }
        }
        // 0x9a dispatches 0x0D when the actor gate or menu is set.
        Case c2;c2.a.flags_6c=4;
        c2.result=dfre::dispatch_game_command(0x9a,0,c2.g,c2.a,table);
        check(c2.result.calls.size()==1,"0x9a nested dispatch missing");
        expect_call(c2.result.calls[0],dfre::DispatchCallKind::game_event,0x46f3e0,
                    {0x13,dfre::dispatch_arg_uninit_payload,0x10});
        c2.a.flags_6c=0;c2.g.menu_flag_67dbc0=1;c2.result=dfre::dispatch_game_command(0x9a,0,c2.g,c2.a,table);
        check(c2.result.calls.size()==1,"0x9a menu dispatch missing");
    }

    // Weapon commands.
    {
        for (std::uint32_t id=0xb4;id<=0xbd;++id) {
            auto c=run(table,id,0);
            check(c.result.calls.size()==1,"weapon select missing");
            expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_select,0x483dd0,{id-0xb4});
        }
        auto c=run(table,0x8c,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_fire,0x483c70,{});
        c=run(table,0x8d,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_fire,0x483c70,{});
        c=run(table,0xbe,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_reload,0x483bd0,{});
        c=run(table,0xbf,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_cycle,0x484b70,{1});
        c=run(table,0xc0,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_cycle,0x484b70,{0xffffffff});
        c=run(table,0x23,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_aim,0x484c20,{});
        c=run(table,0x06,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_unknown,0x4847c0,{});
        c=run(table,0x78,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_supply,0x4848c0,{});
        c=run(table,0x1f,0);check(c.result.calls.empty(),"0x1f acted without the fo flag");
    }

    // Pause, escape cascade, option toggles.
    {
        auto c=run(table,0x22,0);
        check(c.g.pause_6e4714==1&&c.g.type_flag_67db6c==0,"0x22 pause path changed");
        c.g.multiplayer_67f520=1;c.result=dfre::dispatch_game_command(0x22,0,c.g,c.a,table);
        check(c.g.pause_6e4714==1,"0x22 acted in multiplayer");
        c.g=DispatchGlobals{};c.g.pause_6e4714=1;c.g.pause2_6e4718=1;
        c.result=dfre::dispatch_game_command(0x15,0,c.g,c.a,table);
        check(c.g.pause_6e4714==0&&c.g.pause2_6e4718==0,"escape did not clear pause flags");
        c.g=DispatchGlobals{};c.g.toggle_67db90=1;c.g.pause_6e4714=0;
        c.result=dfre::dispatch_game_command(0x15,0,c.g,c.a,table);
        check(c.g.toggle_67db90==0,"escape did not clear the audio flag");
        c.g=DispatchGlobals{};c.result=dfre::dispatch_game_command(0x15,0,c.g,c.a,table);
        check(c.g.flag_67db70==0&&c.g.flag_67db74==0&&c.g.type_flag_67db6c==1,"escape default path changed");
        c=run(table,0x0e,0);check(c.g.toggle_67f504==1,"crosshairs toggle changed");
        c=run(table,0x0a,0);check(c.g.toggle_67f4fc==1,"flipmouse toggle changed");
        c=run(table,0x11,0);check(c.g.toggle_67f514==1,"showgun toggle changed");
        c=run(table,0x24,0);check(c.g.toggle_67f518==1,"turbo toggle changed");
        c=run(table,0x0b,0);check(c.g.hud_color_index_67f508==1&&c.g.hud_color_value_67db58==0,"hud colour path changed");
        c.g.hud_color_index_67f508=5;c.g.hud_color_table_67db40[0]=0x12345678;
        c.result=dfre::dispatch_game_command(0x0b,0,c.g,c.a,table);
        check(c.g.hud_color_index_67f508==0&&c.g.hud_color_value_67db58==0x12345678,"hud colour wrap changed");
        c=run(table,0x1b,0);check(c.g.hud_detail_67f50c==1,"hud detail changed");
        c.g.hud_detail_67f50c=2;c.result=dfre::dispatch_game_command(0x1b,0,c.g,c.a,table);
        check(c.g.hud_detail_67f50c==0,"hud detail wrap changed");
        // Resolution uses a signed comparison of max+1.
        c.g=DispatchGlobals{};c.g.resolution_max_503134=0xffffffffu;c.g.resolution_index_67f4f4=0;
        c.result=dfre::dispatch_game_command(0x08,0,c.g,c.a,table);
        check(c.result.calls.size()==3,"resolution calls changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::screen_mode,0x403480,{0});
        c.g=DispatchGlobals{};c.g.resolution_max_503134=5;c.g.resolution_index_67f4f4=3;
        c.result=dfre::dispatch_game_command(0x08,0,c.g,c.a,table);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::screen_mode,0x403480,{0});
        c.g.resolution_index_67f4f4=6;
        c.result=dfre::dispatch_game_command(0x08,0,c.g,c.a,table);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::screen_mode,0x403480,{6});
        c=run(table,0x07,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::screen_mode,0x403280,{1});
    }

    // Messages and dialogs.
    {
        auto c=run(table,0x28,0);
        check(c.g.toggle_67db88==1&&c.result.calls.size()==2,"0x28 on path changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::dialog,0x404cf0,{0x4cd55c,0x4cd580,0xffffffff});
        expect_call(c.result.calls[1],dfre::DispatchCallKind::message,0x479f00,
                    {dfre::dispatch_arg_created_handle,0xffffffff},true);
        c.g.toggle_67db88=1;c.result=dfre::dispatch_game_command(0x28,0,c.g,c.a,table);
        check(c.g.toggle_67db88==0,"0x28 off path changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::dialog,0x404cf0,{0x4cd55c,0x4cd58c,0xffffffff});
        c=run(table,0x30,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::dialog,0x404cf0,{0x4cd55c,0x4cd598,0xffffffff});
        c.g.toggle_67f564=1;c.result=dfre::dispatch_game_command(0x30,0,c.g,c.a,table);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::dialog,0x404cf0,{0x4cd55c,0x4cd5a4,0xffffffff});
        c=run(table,0x31,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::message,0x479f00,{0x4cd5b0,0xffffffff});
        c.g.toggle_503428=1;c.result=dfre::dispatch_game_command(0x31,0,c.g,c.a,table);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::message,0x479f00,{0x4cd5c4,0xffffffff});
        c=run(table,0x01,0);
        check(c.result.calls.size()==2,"command dialog missing");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::dialog,0x404cf0,
                    {0x4cd55c,0x4cd550,0,0x479830,0x3c});
        expect_call(c.result.calls[1],dfre::DispatchCallKind::dialog,0x4799d0,
                    {dfre::dispatch_arg_created_handle,0,0x479830,0x3c},true);
        // Value dialogs format the stored scalar, not the raw argument.
        c=run(table,0x12,25);
        check(c.g.saturation_67f4f0==25&&c.result.calls.size()==5,"0x12 path changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::screen_mode,0x4027d0,{});
        expect_call(c.result.calls[1],dfre::DispatchCallKind::format_int,0x493a80,
                    {dfre::dispatch_arg_formatted_text,0x4cd564,25});
        c.g=DispatchGlobals{};c.g.saturation_67f4f0=77;
        c.result=dfre::dispatch_game_command(0x12,0,c.g,c.a,table);
        check(c.result.calls.size()==4&&c.result.calls[0].address==0x493a80,"0x12 zero-value path changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::format_int,0x493a80,
                    {dfre::dispatch_arg_formatted_text,0x4cd564,77});
        c=run(table,0x14,9);
        check(c.g.mouse_scale_67f500==9,"0x14 store changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::format_int,0x493a80,
                    {dfre::dispatch_arg_formatted_text,0x4cd564,9});
        // Multiplayer chat dialogs require the mission flags.
        for (std::uint32_t id : {100u,101u,104u,105u}) {
            Case d;d.g.multiplayer_67f520=1;d.g.mission_67f528=1;d.g.command_tick_6e471c=10;
            d.result=dfre::dispatch_game_command(id,0,d.g,d.a,table);
            check(d.result.calls.size()==2,"chat dialog missing");
            check(d.result.calls[1].address==0x4799d0,"chat dialog tail changed");
        }
        Case d;d.g.multiplayer_67f520=1;d.g.mission_67f528=1;d.g.command_tick_6e471c=8;
        d.result=dfre::dispatch_game_command(100,0,d.g,d.a,table);
        check(d.result.calls.empty(),"chat throttle accepted a short interval");
    }

    // Team toggles and cheats.
    {
        auto c=run(table,0x6c,0);
        check(c.g.toggle_6e558c==0&&c.result.calls.empty(),"0x6c acted without multiplayer");
        c.g.multiplayer_67f520=1;c.g.mission_67f528=1;
        c.result=dfre::dispatch_game_command(0x6c,0,c.g,c.a,table);
        check(c.g.toggle_6e558c==1&&c.result.calls.size()==2&&c.result.calls[0].address==0x404cf0,
              "0x6c path changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::dialog,0x404cf0,{0x4cd55c,0x4cd608,0xffffffff});
        c.g.toggle_6e558c=1;
        c.result=dfre::dispatch_game_command(0x6c,0,c.g,c.a,table);
        check(c.g.toggle_6e558c==0,"0x6c off toggle changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::dialog,0x404cf0,{0x4cd55c,0x4cd614,0xffffffff});
        c.g=DispatchGlobals{};c.g.multiplayer_67f520=1;c.g.mission_67f528=1;
        c.result=dfre::dispatch_game_command(0x6d,0,c.g,c.a,table);
        check(c.g.toggle_6e5590==1&&c.result.calls[0].address==0x446630,"0x6d inner call missing");
        c.g=DispatchGlobals{};c.g.multiplayer_67f520=1;c.g.mission_67f528=1;
        c.result=dfre::dispatch_game_command(0x6e,0,c.g,c.a,table);
        check(c.g.toggle_6e5594==1&&c.result.calls[0].address==0x404cf0,"0x6e path changed");
        c=run(table,0x6f,0);check(c.result.calls.empty(),"0x6f acted");
        c=run(table,0x79,0);
        check(c.g.flags_67dbc9==1&&c.result.calls.size()==3,"0x79 path changed");
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_supply,0x4848c0,{});
        expect_call(c.result.calls[1],dfre::DispatchCallKind::dialog,0x404cf0,{0x4cd55c,0x4cd650,0xffffffff});
        expect_call(c.result.calls[2],dfre::DispatchCallKind::message,0x479f00,
                    {dfre::dispatch_arg_created_handle,0xffffffff},true);
        c.g.flags_67dbc9=1;c.result=dfre::dispatch_game_command(0x79,0,c.g,c.a,table);
        check(c.g.flags_67dbc9==0&&c.result.calls.size()==1,"0x79 off path changed");
        c=run(table,0x7b,0);check(c.g.flags_67dbc9==8,"0x7b toggle changed");
        c=run(table,0x7a,0);check(c.a.stance_74==0x4000,"0x7a stance word changed");
        c.g.flags_67dbc9=0;c.g.fo_toggle_c23a34=1;c.result=dfre::dispatch_game_command(0x1f,0,c.g,c.a,table);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::weapon_supply,0x484a50,{});
    }

    // Camera commands and globals.
    {
        for (std::uint32_t id=0x190;id<=0x193;++id) {
            auto c=run(table,id,0);
            expect_call(c.result.calls[0],dfre::DispatchCallKind::camera_mode,0x42ac10,
                        {dfre::dispatch_arg_local_actor,id-0x190});
        }
        Case c;c.g.camera_mode_bd0040=1;
        for (std::uint32_t id=0x17d;id<=0x184;++id) {
            c.result=dfre::dispatch_game_command(id,0,c.g,c.a,table);
            check(c.result.calls.size()==1&&c.result.calls[0].kind==dfre::DispatchCallKind::camera_picture,
                  "picture call missing");
        }
        DispatchGlobals off;
        c.g=off;c.result=dfre::dispatch_game_command(0x17d,0,c.g,c.a,table);
        check(c.result.calls.empty(),"picture call without the mode flag");
        c.g=off;c.result=dfre::dispatch_game_command(0x17c,0,c.g,c.a,table);
        check(c.g.camera_mode_bd0040==1,"0x17c toggle changed");
        c=run(table,0x19a,0);check(c.g.camera_6ba424==std::uint32_t(-0x2000000),"0x19a add changed");
        c.g=DispatchGlobals{};c.g.camera_6ba424=0x22000000u;
        c.result=dfre::dispatch_game_command(0x19b,0,c.g,c.a,table);
        check(c.g.camera_6ba424==0x24000000u,"0x19b byte add changed");
        c.g=DispatchGlobals{};c.g.camera_6ba428=0x10000000u;
        c.result=dfre::dispatch_game_command(0x19c,0,c.g,c.a,table);
        check(c.g.camera_6ba428==0x0e000000u,"0x19c add changed");
        c.g.camera_6ba428=0x60000000u;
        c.result=dfre::dispatch_game_command(0x19c,0,c.g,c.a,table);
        check(c.g.camera_6ba428==0x40000000u,"0x19c high clamp changed");
        c.g.camera_6ba428=0xc0000000u;
        c.result=dfre::dispatch_game_command(0x19d,0,c.g,c.a,table);
        check(c.g.camera_6ba428==0xd0000000u,"0x19d low clamp changed");
        c.g=DispatchGlobals{};c.g.camera_6ba42c=0xffffu;
        c.result=dfre::dispatch_game_command(0x19e,0,c.g,c.a,table);
        check(c.g.camera_6ba42c==0x10000u,"0x19e clamp changed");
        c.g.camera_6ba42c=0x1f40000u;
        c.result=dfre::dispatch_game_command(0x19f,0,c.g,c.a,table);
        check(c.g.camera_6ba42c==0x1f40000u,"0x19f clamp changed");
        c.g.camera_6ba424=0;c.g.camera_mode_bd0040=1;
        c.result=dfre::dispatch_game_command(0x19a,0,c.g,c.a,table);
        check(c.g.camera_6ba424==0,"0x19a acted in the picture mode");
    }

    // Zoom scalar: truncation, integer indefinite and clamps.
    {
        auto zoom=[&](std::uint32_t id,std::uint32_t fov){
            Case c;c.g.fov_c23a28=fov;
            c.result=dfre::dispatch_game_command(id,0,c.g,c.a,table);
            return c.g.fov_c23a28;
        };
        check(zoom(0x168,0x40000000u)==966367641u,"0x168 truncation changed");
        check(zoom(0x168,0x7fffffff)==1932735282u,"0x168 large value changed");
        check(zoom(0x168,0x80000000u)==0x4000u,"0x168 clamp changed");
        check(zoom(0x168,1)==0x4000u,"0x168 minimum changed");
        check(zoom(0x169,123456)==135801u,"0x169 truncation changed");
        check(zoom(0x169,0x7fffffff)==0x80000000u,"0x169 indefinite changed");
        check(zoom(0x169,0x80000000u)==0x80000000u,"0x169 negative indefinite changed");
        check(zoom(0x169,0xffffffffu)==0xffffffffu,"0x169 negative truncation changed");
        check(zoom(0x169,235930)==0x39999u,"0x169 maximum changed");
        check(zoom(0x169,100000)==110000u,"0x169 small value changed");
    }

    // Print screen, exits, event push and unknown ids.
    {
        auto c=run(table,0x02,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::screenshot,0x473eb0,{0x67dbf8,0xbfbb00});
        c=run(table,0x03,0);check(c.g.menu_state_67dbbc==1,"0x03 value changed");
        c=run(table,0x04,0);check(c.g.menu_state_67dbbc==2,"0x04 value changed");
        c=run(table,0x2e,0);check(c.g.menu_state_67dbbc==5,"0x2e value changed");
        c=run(table,0x0d,0);
        expect_call(c.result.calls[0],dfre::DispatchCallKind::game_event,0x46f3e0,
                    {0x13,dfre::dispatch_arg_uninit_payload,0x10});
        c=run(table,0x05,0);check(c.g.flags_67dbc8==4,"0x05 sky toggle changed");
        for (std::uint32_t id : {0x10u,0x13u,0x16u,0x18u,0x19u,0x1au,0x1cu,0x32u,0x63u,
                                 0x6fu,0x70u,0x77u,0x7du,0x7fu,0xbfu+0x40u}) {
            Case u;u.result=dfre::dispatch_game_command(id,0,u.g,u.a,table);
            check(!u.result.gated&&u.result.calls.empty(),"id was not a no-op");
        }
    }

    std::cout<<"gameplay dispatch checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
