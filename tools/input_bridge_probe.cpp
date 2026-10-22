#include "dfre/gameplay_dispatch.hpp"
#include "dfre/input_bridge.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Line probes for manual checks of the input bridge. Modes:
//   bindings  print the shipped DFDK.KEY binding rows
//   script    drive the bridge from a small script and print ordered records,
//             with an optional GameplayDispatch apply step
// See docs/input-bridge.md for the script grammar.

namespace {

std::string record_text(const dfre::InputRecord& record) {
    return std::to_string(record.id) + ":" + std::to_string(record.tick) + ":" +
           std::to_string(record.value) + ":" + std::to_string(record.extra);
}

std::string records_text(const std::vector<dfre::InputRecord>& records) {
    std::string out;
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i) out += ';';
        out += record_text(records[i]);
    }
    return out;
}

std::string effect_name(dfre::GlobalKeyEffect effect) {
    switch (effect) {
    case dfre::GlobalKeyEffect::none: return "none";
    case dfre::GlobalKeyEffect::dispatch_exit: return "exit";
    case dfre::GlobalKeyEffect::clear_typing: return "clear_typing";
    case dfre::GlobalKeyEffect::digit: return "digit";
    case dfre::GlobalKeyEffect::flight_page: return "flight_page";
    case dfre::GlobalKeyEffect::menu_page: return "menu_page";
    case dfre::GlobalKeyEffect::enter_flag: return "enter_flag";
    case dfre::GlobalKeyEffect::dispatch_space: return "space";
    }
    return "?";
}

} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if(mode=="oracle"){
        std::string line;while(std::getline(std::cin,line)){std::istringstream stream(line);
            std::uint32_t tick,mode_word,live,windowed,active,flip,steady,menu,actor,mutation,count;
            std::int32_t scale;stream>>tick>>mode_word>>live>>windowed>>active>>scale>>flip>>steady>>menu>>actor>>mutation>>count;
            auto bridge=dfre::InputBridge::with_shipped_profile();dfre::InputSourceFlags flags;
            flags.input_mode=std::uint8_t(mode_word);flags.live_input=live!=0;flags.windowed=windowed!=0;flags.window_active=active!=0;
            flags.mouse_scale=scale;flags.mouse_flip=flip!=0;flags.steady=steady!=0;flags.global.menu_flag=std::uint8_t(menu);flags.global.actor_flags=actor;
            if(mutation)flags.global.digit_flag=1;
            bridge.set_live_sources([&]{return flags;},[&](const auto& result){
                if(mutation && result.effect==dfre::GlobalKeyEffect::digit){flags.input_mode=1;flags.global.digit_flag=0;}
            },[&](const auto&){if(mutation){flags.input_mode=0;dfre::InputEvent key{dfre::InputKind::key_down};key.key=34;bridge.feed(key);}});
            bridge.set_source_flags(flags);
            bridge.set_query_row_source([](auto row)->std::optional<dfre::CommandBinding>{if(row==0x8c)return dfre::CommandBinding{};return std::nullopt;});
            for(std::uint32_t i=0;i<count;++i){int kind,a,b;stream>>kind>>a>>b;
                if(kind==0 || kind==1){dfre::InputEvent event{kind?dfre::InputKind::key_up:dfre::InputKind::key_down};event.key=a;event.extended=b!=0;bridge.feed(event);}
                if(kind==2 || kind==3){dfre::InputEvent event{kind==2?dfre::InputKind::pointer_down:dfre::InputKind::pointer_up};event.button=unsigned(a);bridge.feed(event);}
                if(kind==4){dfre::InputEvent event{dfre::InputKind::pointer_delta};event.x=a;event.y=b;bridge.feed(event);}
                if(kind==5){bridge.focus_lost();flags.window_active=false;}
                if(kind==6)bridge.feed_character(std::uint8_t(a));
                if(kind==7)flags.keyboard_ring_blocked=a!=0;
            }
            const auto frame=bridge.frame(tick);
            std::cout<<"{\"calls\":[";bool first=true;
            const auto record=[&](const auto& r){std::cout<<(first?"[":",[")<<r.id<<','<<r.value<<','<<r.extra<<']';first=false;};
            for(const auto& r:frame.immediate)record(r);for(const auto& r:frame.commands)record(r);
            std::cout<<"],\"chat\":[";first=true;for(const auto& e:frame.chat_keys){std::cout<<(first?"[":",[")<<unsigned(e.state)<<','<<unsigned(e.key)<<']';first=false;}
            std::cout<<"],\"state\":["<<std::uint32_t(bridge.pending_delta_x())<<','<<std::uint32_t(bridge.pending_delta_y())<<','<<flags.window_active<<','<<bridge.mouse_buttons()<<','<<bridge.query_active(0x8c)<<"]}\n";
        }return 0;
    }
    if (mode == "bindings") {
        dfre::InputBridge bridge = dfre::InputBridge::with_shipped_profile();
        for (const auto& pair : dfre::profile_command_map()) {
            const auto index = bridge.command_table().find_by_name(pair.command);
            if (index == std::string_view::npos) continue;
            const auto& row = bridge.command_table().at(index);
            std::cout << pair.command << ' ' << pair.action << ' ' << row.id << ' ' << row.key << ' ' << row.mouse
                      << ' ' << row.flags << '\n';
        }
        return 0;
    }
    if (mode == "script") {
        dfre::InputBridge bridge = dfre::InputBridge::with_shipped_profile();
        dfre::DispatchGlobals globals;
        dfre::DispatchActor actor;
        dfre::BridgeFrame last;
        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream stream(line);
            std::string directive;
            if (!(stream >> directive) || directive.empty() || directive[0] == '#') continue;
            if (directive == "flags") {
                int input_mode = 0, live = 1, windowed = 0, active = 1, flip = 0, steady = 0;
                std::int32_t scale = 0;
                std::uint32_t menu = 0, actor_flags = 0;
                stream >> input_mode >> live >> windowed >> active >> scale >> flip >> steady >> menu >> actor_flags;
                dfre::InputSourceFlags flags;
                flags.input_mode = std::uint8_t(input_mode);
                flags.live_input = live != 0;
                flags.windowed = windowed != 0;
                flags.window_active = active != 0;
                flags.mouse_scale = scale;
                flags.mouse_flip = flip != 0;
                flags.steady = steady != 0;
                flags.global.menu_flag = std::uint8_t(menu);
                flags.global.actor_flags = actor_flags;
                bridge.set_source_flags(flags);
            } else if (directive == "keydown" || directive == "keyup") {
                int vk = 0, extended = 0;
                stream >> vk >> extended;
                dfre::InputEvent event{directive == "keydown" ? dfre::InputKind::key_down : dfre::InputKind::key_up};
                event.key = vk;
                event.extended = extended != 0;
                bridge.feed(event);
            } else if (directive == "delta") {
                int dx = 0, dy = 0;
                stream >> dx >> dy;
                dfre::InputEvent event{dfre::InputKind::pointer_delta};
                event.x = dx;
                event.y = dy;
                bridge.feed(event);
            } else if (directive == "down" || directive == "up") {
                unsigned button = 0;
                stream >> button;
                bridge.feed({directive == "down" ? dfre::InputKind::pointer_down : dfre::InputKind::pointer_up,
                             0, 0, 0, {}, button, false});
            } else if (directive == "move") {
                int x = 0, y = 0;
                stream >> x >> y;
                bridge.feed({dfre::InputKind::pointer_move, 0, x, y});
            } else if (directive == "focus") {
                bridge.feed({dfre::InputKind::focus_lost});
            } else if (directive == "tick") {
                std::uint32_t tick = 0;
                stream >> tick;
                last = bridge.frame(tick);
                std::cout << "frame " << tick << " commands [" << records_text(last.commands) << "] immediate ["
                          << records_text(last.immediate) << "] effects [";
                for (std::size_t i = 0; i < last.effects.size(); ++i) {
                    if (i) std::cout << ',';
                    std::cout << effect_name(last.effects[i]);
                }
                std::cout << "] chat " << last.chat_keys.size() << " focus " << (bridge.focus_flag() ? 1 : 0) << '\n';
            } else if (directive == "apply") {
                std::size_t calls = 0, gated = 0;
                const auto run = [&](const dfre::InputRecord& record) {
                    const auto result = dfre::dispatch_game_command(record.id, std::int32_t(record.value), globals,
                                                                    actor, bridge.command_table());
                    calls += result.calls.size();
                    if (result.gated) ++gated;
                };
                for (const auto& record : last.immediate) run(record);
                dfre::dispatch_commands(last.commands, run);
                std::cout << "apply control 0x" << std::hex << globals.control_c23a00 << std::dec << " yaw "
                          << actor.look_yaw_30 << " pitch " << actor.look_pitch_34 << " pulse04 "
                          << globals.pulse_c23a04 << " pulse08 " << globals.pulse_c23a08 << " calls " << calls
                          << " gated " << gated << '\n';
            } else {
                std::cerr << "unknown directive: " << directive << '\n';
                return 2;
            }
        }
        return 0;
    }
    std::cerr << "input_bridge_probe MODE\nModes: bindings script\n";
    return 2;
}
