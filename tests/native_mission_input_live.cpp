#include "dfre/mission_input_live.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace dfre;
namespace {
void check(bool value,const char* error){if(!value)throw std::runtime_error(error);}
void run(){
    AiActorTables actors;actors.groups[3].resize(1);actors.groups[3][0].active=true;actors.groups[3][0].definition_index=1;
    MissionTickConfig config;config.local_actor=0;MissionTickState state;init_mission_tick(state,config,std::move(actors),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},1);
    state.weapons.loadout({6,9,1});state.weapons.select_slot(3);state.globals.mouse_scale_67f500=4;
    std::map<std::uint32_t,std::uint32_t> cells;bool active=true;MissionInputLiveBindings b;
    b.read=[&](auto at,auto){return cells.at(at);};b.window_active=[&]{return active;};
    cells={{0xbd4f78,0},{0xbd482c,0},{0x4fcbde,0x70000},{0xd09944,0x1234},{0xd096bc,0},{0xbd4820,'Y'},{0xbd4824,'N'}};
    auto source=mission_input_live_sources(state,b);check(source.window_active&&source.live_input&&source.mouse_scale==4&&source.priority_key==7&&source.joystick_buttons==0x1234,"Current input sources differ");
    cells[0xbd482c]=1;cells[0xd096bc]=1;active=false;state.globals.mouse_disabled_503130=2;state.globals.toggle_67f4fc=0x100;state.globals.type_flag_67db6c=0x100;
    source=mission_input_live_sources(state,b);check(!source.live_input&&!source.window_active&&source.windowed&&source.mouse_flip&&source.keyboard_ring_blocked&&source.global.type_flag==1,"Source mutation or full-word flag was lost");
    GlobalKeyResult result;result.effect=GlobalKeyEffect::clear_typing;mission_input_live_global(state,b,result);check(!state.globals.type_flag_67db6c,"Typing clear missed canonical cell");
    state.globals.toggle_67db90=9;result.effect=GlobalKeyEffect::digit;result.argument=3;b.digit=[&](auto digit){check(digit==3&&state.globals.toggle_67db90==9,"Digit callback order differs");state.globals.toggle_67db90=77;};mission_input_live_global(state,b,result);check(!state.globals.toggle_67db90,"Digit clear must follow callback");
    result.effect=GlobalKeyEffect::enter_flag;mission_input_live_global(state,b,result);check(state.globals.menu_state_67dbbc==1,"Enter flag missed canonical cell");
    result.effect=GlobalKeyEffect::dispatch_exit;result.dispatched_id=3;bool failed=false;try{mission_input_live_global(state,b,result);}catch(const MissionTickError&){failed=true;}check(failed,"Missing immediate dispatch silently succeeded");unsigned dispatches=0;b.dispatch=[&](auto id){check(id==3,"Immediate command differs");++dispatches;};mission_input_live_global(state,b,result);check(dispatches==1,"Immediate dispatch count differs");
    cells[0xbd482c]=0;cells[0xd096bc]=0;cells[0x4fcbde]=0;cells[0xd09944]=0;active=true;state.globals.mouse_disabled_503130=0;state.globals.toggle_67f4fc=0;state.globals.menu_flag_67dbc0=0;state.globals.toggle_67db7c=0;
    cells[0xbd4f78]=0x100;check(mission_input_live_sources(state,b).input_mode==2,"Full-word input mode was truncated to game mode");cells[0xbd4f78]=0;
    auto profile=default_action_bindings();InputBridge input(profile);input.set_live_sources([&]{return mission_input_live_sources(state,b);},[&](const auto& r){mission_input_live_global(state,b,r);});
    const auto test_key=[&](std::string_view name){const auto row=input.command_table().find_by_name(name);check(row!=std::string_view::npos,"Profile command is absent");return input.command_table().rows().at(row).key;};
    check(test_key("move_forward")==38&&test_key("slide_left")==37,"Shipped movement profile is not the arrow profile");
    input.feed(InputEvent{InputKind::key_down,int(test_key("move_forward"))});auto frame=input.frame(1);state.globals.control_c23a00=0;for(const auto& command:frame.commands)dispatch_game_command(command.id,std::int32_t(command.value),state.globals,state.dispatch_actor,input.command_table());check(state.globals.control_c23a00&2,"Forward input missed current control word");
    input.feed(InputEvent{InputKind::key_up,int(test_key("move_forward"))});input.feed(InputEvent{InputKind::pointer_delta,0,2,3});frame=input.frame(2);const auto yaw=state.dispatch_actor.look_yaw_30,pitch=state.dispatch_actor.look_pitch_34;for(const auto& command:frame.commands)dispatch_game_command(command.id,std::int32_t(command.value),state.globals,state.dispatch_actor,input.command_table());check(state.dispatch_actor.look_yaw_30!=yaw&&state.dispatch_actor.look_pitch_34!=pitch,"Mouse axes did not change current view");
    for(const auto name:{"Crouch","Prone","Stand"}){const auto key=test_key(name);input.feed(InputEvent{InputKind::key_down,int(key)});frame=input.frame(3);for(const auto& command:frame.commands)dispatch_game_command(command.id,std::int32_t(command.value),state.globals,state.dispatch_actor,input.command_table());input.feed(InputEvent{InputKind::key_up,int(key)});}check(state.globals.pulse_c23a04&&state.globals.pulse_c23a08&&state.globals.pulse_c23a0c,"Stance pulses missed canonical state");
    for(const auto entry:{std::pair<const char*,std::uint32_t>{"attack_1",0x483c70},{"magazine",0x483bd0},{"weapon3",0x483dd0},{"cycle",0x484b70}}){input.feed(InputEvent{InputKind::key_down,int(test_key(entry.first))});frame=input.frame(4);bool found=false;for(const auto& command:frame.commands){const auto dispatch=dispatch_game_command(command.id,std::int32_t(command.value),state.globals,state.dispatch_actor,input.command_table());for(const auto& call:dispatch.calls)if(call.address==entry.second)found=true;}check(found,"Loaded control did not reach its owned weapon body");input.feed(InputEvent{InputKind::key_up,int(test_key(entry.first))});}
    profile[2].key='W';input.apply_profile(profile);check(test_key("move_forward")=='W',"Custom profile binding was lost");
    cells[0xbd4820]=0x10059;state.globals.type_flag_67db6c=1;failed=false;try{mission_input_live_sources(state,b);}catch(const MissionTickError&){failed=true;}check(failed,"Unrepresentable typing key was truncated");
}
}
int main(){try{run();std::cout<<"Mission live input sources and canonical command checks passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
