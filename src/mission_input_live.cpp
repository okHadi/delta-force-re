#include "dfre/mission_input_live.hpp"
#include <cstring>
#include <stdexcept>

namespace dfre {namespace {
template<class T> const T& required(const T& callback){if(!callback)throw MissionTickError("Mission input reached a missing provider");return callback;}
std::uint8_t flag(std::uint32_t value){return value?1:0;}
}
InputSourceFlags mission_input_live_sources(const MissionTickState& state,const MissionInputLiveBindings& b){
    const auto& read=required(b.read);InputSourceFlags out;
    const auto mode=read(0xbd4f78,4);out.input_mode=mode<=1?std::uint8_t(mode):std::uint8_t(2);out.live_input=read(0xbd482c,4)==0;
    out.windowed=state.globals.mouse_disabled_503130!=0;out.window_active=required(b.window_active)();
    out.priority_key=std::uint16_t(read(0x4fcbde,4)>>16);out.joystick_buttons=read(0xd09944,4);
    const auto scale=state.globals.mouse_scale_67f500;std::memcpy(&out.mouse_scale,&scale,4);
    out.mouse_flip=state.globals.toggle_67f4fc!=0;out.steady=mission_steady(state)!=0;
    out.keyboard_ring_blocked=read(0xd096bc,4)!=0;
    out.global.type_flag=flag(state.globals.type_flag_67db6c);out.global.digit_flag=flag(state.globals.toggle_67db90);
    out.global.page_flag=flag(state.globals.toggle_67db7c);out.global.menu_flag=flag(state.globals.menu_flag_67dbc0);
    out.global.hud_menu=flag(state.globals.multiplayer_67f520);out.global.hud_menu2=flag(state.game_type_67f53c);
    if(out.global.type_flag){const auto first=read(0xbd4820,4);const auto second=read(0xbd4824,4);
    if(first>0xffff||second>0xffff)throw MissionTickError("Mission input typing keys exceed the current key interface");
    out.global.type_key_a=std::uint16_t(first);out.global.type_key_b=std::uint16_t(second);}
    if(state.config.local_actor){const AiActorRef ref{3,*state.config.local_actor};const auto* actor=state.actor_at(ref);if(!actor)throw MissionTickError("Mission input local actor has no owner");out.global.actor_flags=actor->field_6c;}
    else if(out.global.hud_menu&&!out.global.type_flag&&!out.global.digit_flag&&!out.global.page_flag&&!out.global.menu_flag)
        throw MissionTickError("Mission input global key gate requires the local actor");
    return out;
}
void mission_input_live_global(MissionTickState& state,const MissionInputLiveBindings& b,const GlobalKeyResult& result){
    switch(result.effect){
    case GlobalKeyEffect::none:case GlobalKeyEffect::dispatch_space:return;
    case GlobalKeyEffect::dispatch_exit:required(b.dispatch)(result.dispatched_id);return;
    case GlobalKeyEffect::clear_typing:state.globals.type_flag_67db6c=0;return;
    case GlobalKeyEffect::digit:required(b.digit)(result.argument);state.globals.toggle_67db90=0;return;
    case GlobalKeyEffect::flight_page:required(b.flight_page)(result.argument);return;
    case GlobalKeyEffect::menu_page:required(b.menu_page)(result.argument);return;
    case GlobalKeyEffect::enter_flag:state.globals.menu_state_67dbbc=1;return;
    }
    throw MissionTickError("Mission input global effect is invalid");
}
void mission_input_live_chat(const MissionInputLiveBindings& b,const InputKeyEvent& event){required(b.chat)(event);}
} // namespace dfre
