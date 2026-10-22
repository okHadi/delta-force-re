// Event stage probe. Reads cases from stdin, runs dfre::EventStage, prints one
// state line per case. tools/check_events_original.py compares the lines with
// the original x86 functions emulated from Df.exe.
//
// Case format (whitespace separated):
//   n_events n_triggers n_actions
//   events: 6 words each, triggers: 8 words each, actions: 8 words each
//   n_markers, then n_markers * (type_id raw_4a x y)
//   n_dwords, then n_dwords * (address index value)   object index dwords
//   world_67f520 world_67f549 loader_flag cursor frame_counter
//   n_sets, then n_sets * (which a b [c])
//     which 0: 0x6e0050(a,b) 1: 0x6d7e50(a,b) 2: 0x6d8050(a,b,c)
//     which 3: 0x6e0250(a)   4: 0x6e0254(a)   5: 0x67dbec(a)
//   n_330 answer...   n_560 answer...
//   n_steps, then n_steps * (t|b|c|f|p)   tick, bit1, bit2, frame, process(0)
//   n_mutations, then six words each: site occurrence table index word value
//   table0=events,1=triggers,2=actions; occurrence starts at1.
// Output: event/trigger/action words, cursor/counter, flags and ordered calls.
#include "dfre/events.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>

namespace {
std::string hex8(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(8) << value;
    return out.str();
}

struct ProbeHost : dfre::EventHost {
    struct Dword {
        std::uint32_t address;
        std::uint32_t index;
        std::uint32_t value;
    };
    dfre::EventStage* stage{};
    std::vector<std::array<std::uint32_t,6>> mutations;
    std::map<std::uint32_t,std::uint32_t> calls;
    void called(std::uint32_t site){
        const auto occurrence=++calls[site];
        for(const auto& m:mutations)if(m[0]==site&&m[1]==occurrence){
            if(m[2]==0)stage->event(m[3]).words.at(m[4])=m[5];
            else if(m[2]==1)stage->trigger(m[3]).at(m[4])=m[5];
            else if(m[2]==2)stage->action(m[3]).at(m[4])=m[5];
            else throw std::runtime_error("Bad mutation table");
        }
    }
    std::uint32_t external_event_fired(std::size_t index)override{
        // Original497F60 on the checker's fresh, aligned heap:16-byte headers.
        const auto align=[](std::size_t n){return (n+15)&~std::size_t(15);};
        const auto offset=index*24+20;
        const auto triggers=align(stage->event_count()*24)+16;
        const auto actions=triggers+align(stage->trigger_count()*32)+16;
        const auto word=[&](std::size_t relative,bool action){
            const auto value=action?stage->action(relative/32).at((relative%32)/4):stage->trigger(relative/32).at((relative%32)/4);
            return (value>>((relative%4)*8))&0xffu;
        };
        std::uint32_t value;
        if(offset>=triggers&&offset<triggers+stage->trigger_count()*32)value=word(offset-triggers,false);
        else if(offset>=actions&&offset<actions+stage->action_count()*32)value=word(offset-actions,true);
        else throw std::runtime_error("External fired byte exceeds the fixture's table payloads");
        log.push_back("external:"+std::to_string(index)+":"+std::to_string(value));return value;
    }
    std::vector<Dword> dwords;
    std::vector<std::uint32_t> answers_330;
    std::vector<std::uint32_t> answers_560;
    std::size_t at_330 = 0;
    std::size_t at_560 = 0;
    bool world_zero = true;
    bool world_bit1 = false;
    std::vector<std::string> log;

    std::uint32_t object_index_dword(std::uint32_t address, std::uint32_t index) override {
        for (const auto& entry : dwords) {
            if (entry.address == address && entry.index == index) return entry.value;
        }
        return 0;
    }
    std::uint32_t probe_460330(std::size_t index) override {
        log.push_back("p330:" + std::to_string(index));
        const std::uint32_t value = answers_330.empty() ? 0 : answers_330[at_330 % answers_330.size()];
        ++at_330;called(0x460330);
        return value;
    }
    std::uint32_t probe_460560(std::uint32_t w3, std::uint32_t w4) override {
        log.push_back("p560:" + std::to_string(w3) + ":" + std::to_string(w4));
        const std::uint32_t value = answers_560.empty() ? 0 : answers_560[at_560 % answers_560.size()];
        ++at_560;called(0x460560);
        return value;
    }
    bool world_67f520_is_zero() override { return world_zero; }
    bool world_67f549_bit1() override { return world_bit1; }
    void action_45f670(std::uint32_t a, std::uint32_t b) override {
        log.push_back("d45f670:" + std::to_string(a) + ":" + std::to_string(b)); called(0x45f670);
    }
    void action_45f4c0(std::uint32_t a) override { log.push_back("d45f4c0:" + std::to_string(a)); called(0x45f4c0); }
    void action_4601a0(std::size_t index) override { log.push_back("d4601a0:" + std::to_string(index)); called(0x4601a0); }
    void action_45f5b0(std::uint32_t a) override { log.push_back("d45f5b0:" + std::to_string(a)); called(0x45f5b0); }
    void action_404dd0(std::uint32_t a) override { log.push_back("d404dd0:" + std::to_string(a)); called(0x404dd0); }
    void action_43f500(std::uint32_t a) override { log.push_back("d43f500:" + std::to_string(a)); called(0x43f500); }
    void action_444ca0(std::uint32_t mode) override { log.push_back("d444ca0:" + std::to_string(mode)); called(0x444ca0); }
    void action_460640(std::uint32_t a, std::uint32_t b) override {
        log.push_back("d460640:" + std::to_string(a) + ":" + std::to_string(b)); called(0x460640);
    }
    void action_460280(std::size_t index) override { log.push_back("d460280:" + std::to_string(index)); called(0x460280); }
};

std::string state_line(const dfre::EventStage& stage, const std::vector<std::string>& log) {
    std::ostringstream out;
    out << "E:";
    for (std::size_t index = 0; index < stage.event_count(); ++index) {
        for (auto word : stage.event(index).words) out << hex8(word);
    }
    out << "|T:";for(std::size_t i=0;i<stage.trigger_count();++i)for(auto w:stage.trigger(i))out<<hex8(w);
    out << "|A:";for(std::size_t i=0;i<stage.action_count();++i)for(auto w:stage.action(i))out<<hex8(w);
    out << "|C:"<<hex8(stage.cursor())<<":"<<hex8(stage.frame_counter());
    out << "|F:";
    const auto dump = [&](const char* name, auto word, std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint32_t value = word(index);
            if (value != 0) out << name << "=" << index << ":" << hex8(value) << ",";
        }
    };
    dump("6e0050", [&](std::size_t index) { return stage.flag_word_6e0050(index); },
         dfre::EventStage::flag_2d_words);
    dump("6d7e50", [&](std::size_t index) { return stage.flag_word_6d7e50(index); },
         dfre::EventStage::flag_2d_words);
    dump("6d8050", [&](std::size_t index) { return stage.flag_word_6d8050(index); },
         dfre::EventStage::flag_3d_words);
    out << "6e0250:" << hex8(stage.flags_6e0250()) << ",6e0254:" << hex8(stage.flags_6e0254())
        << ",67dbec:" << hex8(stage.flags_67dbec()) << ",4f2be0:" << hex8(stage.loader_flag());
    out << "|L:";
    for (std::size_t at = 0; at < log.size(); ++at) {
        if (at) out << ';';
        out << log[at];
    }
    return out.str();
}
} // namespace

int main() {
    try {
        std::size_t cases = 0;
        if (!(std::cin >> cases)) throw std::runtime_error("Missing case count");
        for (std::size_t number = 0; number < cases; ++number) {
            std::size_t n_events = 0, n_triggers = 0, n_actions = 0;
            std::cin >> n_events >> n_triggers >> n_actions;
            std::vector<dfre::MissionEvent> events(n_events);
            for (auto& record : events) for (auto& word : record.words) std::cin >> word;
            std::vector<dfre::EventTableRecord> triggers(n_triggers), actions(n_actions);
            for (auto& record : triggers) for (auto& word : record) std::cin >> word;
            for (auto& record : actions) for (auto& word : record) std::cin >> word;
            std::size_t n_markers = 0;
            std::cin >> n_markers;
            std::vector<dfre::MissionObject> markers(n_markers);
            for (auto& marker : markers) {
                std::uint32_t type = 0, raw = 0;
                std::int32_t x = 0, y = 0;
                std::cin >> type >> raw >> x >> y;
                marker.type_id = type;
                marker.raw[0x4a] = static_cast<std::uint8_t>(raw);
                marker.position_raw = {x, y, 0};
            }
            ProbeHost host;
            std::size_t n_dwords = 0;
            std::cin >> n_dwords;
            for (std::size_t at = 0; at < n_dwords; ++at) {
                ProbeHost::Dword entry{};
                std::cin >> entry.address >> entry.index >> entry.value;
                host.dwords.push_back(entry);
            }
            std::uint32_t world = 0, world_bit1 = 0, loader = 0, cursor = 0, counter = 0;
            std::cin >> world >> world_bit1 >> loader >> cursor >> counter;
            host.world_zero = world == 0; // 0x67f520 raw value, zero means "no mode"
            host.world_bit1 = (world_bit1 & 2u) != 0;
            std::size_t n_sets = 0;
            std::cin >> n_sets;
            std::vector<std::array<std::uint32_t, 4>> sets(n_sets);
            for (auto& entry : sets) std::cin >> entry[0] >> entry[1] >> entry[2] >> entry[3];
            std::size_t n_330 = 0;
            std::cin >> n_330;
            for (std::size_t at = 0; at < n_330; ++at) {
                std::uint32_t value = 0;
                std::cin >> value;
                host.answers_330.push_back(value);
            }
            std::size_t n_560 = 0;
            std::cin >> n_560;
            for (std::size_t at = 0; at < n_560; ++at) {
                std::uint32_t value = 0;
                std::cin >> value;
                host.answers_560.push_back(value);
            }
            dfre::EventStage stage(&host);
            stage.set_loader_flag(loader);
            stage.set_cursor(cursor);stage.set_frame_counter(counter);
            stage.load_tables(events, triggers, actions);
            stage.bind_object_triggers(markers);
            stage.bind_object_actions(markers);
            stage.set_loader_flag(loader ^ 1u); // the original load toggles it
            for (const auto& entry : sets) {
                switch (entry[0]) {
                case 0: stage.set_flag_42edd0(entry[1], entry[2]); break;
                case 1: stage.set_flag_42ee00(entry[1], entry[2]); break;
                case 2: stage.set_flag_42ee30(entry[1], entry[2], entry[3]); break;
                case 3: stage.set_flags_6e0250_bit(entry[1]); break;
                case 4: stage.set_flags_6e0254_bit(entry[1]); break;
                case 5: stage.set_flag_42ee50(entry[1]); break;
                default: throw std::runtime_error("Bad flag set kind");
                }
            }
            std::size_t n_steps = 0;
            std::cin >> n_steps;
            std::vector<std::string> steps(n_steps);for(auto& step:steps)std::cin>>step;
            std::size_t mutation_count;std::cin>>mutation_count;host.mutations.resize(mutation_count);
            for(auto& m:host.mutations)for(auto& v:m)std::cin>>v;
            host.stage=&stage;
            for(const auto& step:steps){
                if (step == "t") stage.tick();
                else if (step == "b") stage.run_bit1_events();
                else if (step == "c") stage.run_bit2_events();
                else if (step == "f") stage.frame_driver_step();
                else if (step == "p") stage.process_event(0);
                else throw std::runtime_error("Bad step");
            }
            if (!std::cin) throw std::runtime_error("Truncated case input");
            std::cout << state_line(stage, host.log) << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "events_probe: " << error.what() << '\n';
        return 1;
    }
}
