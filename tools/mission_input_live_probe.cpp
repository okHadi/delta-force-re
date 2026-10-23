#include "dfre/mission_input_live.hpp"
#include <iostream>
using namespace dfre;
int main(){std::uint32_t key,type,digit,page,menu,hud,hud2,actor;while(std::cin>>key>>type>>digit>>page>>menu>>hud>>hud2>>actor){
    MissionTickState state;state.config.local_actor=0;state.tables.groups[3].resize(1);state.tables.groups[3][0].field_6c=std::uint8_t(actor);
    state.globals.type_flag_67db6c=type;state.globals.toggle_67db90=digit;state.globals.toggle_67db7c=page;state.globals.menu_flag_67dbc0=menu;state.globals.multiplayer_67f520=hud;state.game_type_67f53c=hud2;
    MissionInputLiveBindings b;b.read=[](auto at,auto){return at==0xbd4820?std::uint32_t('Y'):at==0xbd4824?std::uint32_t('N'):0u;};b.window_active=[](){return true;};b.dispatch=[](auto){};b.digit=[](auto){};b.flight_page=[](auto){};b.menu_page=[](auto){};
    InputStage stage;const auto source=mission_input_live_sources(state,b);const auto result=stage.global_key(std::uint16_t(key),source.global);mission_input_live_global(state,b,result);
    std::cout<<result.handled<<':'<<int(result.effect)<<':'<<result.argument<<':'<<(result.pushed_record?std::int64_t(result.pushed.id):-1)<<':'<<(result.dispatched?std::int64_t(result.dispatched_id):-1)<<':'<<state.globals.type_flag_67db6c<<':'<<state.globals.toggle_67db90<<':'<<state.globals.menu_state_67dbbc<<'\n';
}}
