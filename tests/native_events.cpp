#include "dfre/events.hpp"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <functional>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

dfre::MissionEvent event(std::uint32_t w0, std::uint32_t trigger_index, std::uint32_t action_index,
                         std::uint32_t w3, std::uint32_t w4, std::uint32_t trigger_count,
                         std::uint32_t action_count, std::uint32_t fired = 0) {
    dfre::MissionEvent result;
    result.words = {w0, trigger_index, action_index, w3, w4,
                    (fired & 0xffu) | ((trigger_count & 0xffu) << 8) | ((action_count & 0xffu) << 16)};
    return result;
}

dfre::EventTableRecord trigger(std::uint32_t kind, std::uint32_t subject, std::uint32_t a = 0,
                               std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    return {0, kind, subject, a, b, c, d, 0};
}

dfre::MissionObject marker(std::uint32_t type_id, std::uint8_t raw_4a, std::int32_t x, std::int32_t y) {
    dfre::MissionObject object;
    object.type_id = type_id;
    object.raw[0x4a] = raw_4a;
    object.position_raw = {x, y, 0};
    return object;
}

// Host with scripted answers and a call log. The log proves argument routing.
struct ScriptHost : dfre::EventHost {
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> dwords;
    std::uint32_t answer_460330 = 0;
    std::uint32_t answer_460560 = 0;
    bool world_zero = true;
    bool world_bit1 = false;
    std::vector<std::string> log;

    std::uint32_t object_index_dword(std::uint32_t address, std::uint32_t index) override {
        const auto found = dwords.find({address, index});
        return found == dwords.end() ? 0 : found->second;
    }
    std::uint32_t probe_460330(std::size_t index) override {
        log.push_back("460330:" + std::to_string(index));
        return answer_460330;
    }
    std::uint32_t probe_460560(std::uint32_t w3, std::uint32_t w4) override {
        log.push_back("460560:" + std::to_string(w3) + ":" + std::to_string(w4));
        return answer_460560;
    }
    bool world_67f520_is_zero() override { return world_zero; }
    bool world_67f549_bit1() override { return world_bit1; }
    void action_45f670(std::uint32_t a, std::uint32_t b) override {
        log.push_back("45f670:" + std::to_string(a) + ":" + std::to_string(b));
    }
    void action_45f4c0(std::uint32_t a) override { log.push_back("45f4c0:" + std::to_string(a)); }
    void action_4601a0(std::size_t index) override { log.push_back("4601a0:" + std::to_string(index)); }
    void action_45f5b0(std::uint32_t a) override { log.push_back("45f5b0:" + std::to_string(a)); }
    void action_404dd0(std::uint32_t a) override { log.push_back("404dd0:" + std::to_string(a)); }
    void action_43f500(std::uint32_t a) override { log.push_back("43f500:" + std::to_string(a)); }
    void action_444ca0(std::uint32_t mode) override { log.push_back("444ca0:" + std::to_string(mode)); }
    void action_460640(std::uint32_t a, std::uint32_t b) override {
        log.push_back("460640:" + std::to_string(a) + ":" + std::to_string(b));
    }
    void action_460280(std::size_t index) override { log.push_back("460280:" + std::to_string(index)); }
};

void test_trigger_binding() {
    dfre::MissionData mission;
    // Only slots inside an event run are bound: the original walks event runs.
    mission.events = {event(0, 0, 0, 0, 0, 1, 1), event(0, 1, 1, 0, 0, 2, 1)};
    mission.triggers = {trigger(1, 10, 0, 7), trigger(1, 10, 0, 9), trigger(1, 3, 1)};
    mission.actions = {{0, 0xc, 0, 7, 0, 0, 0, 0}, {0, 5, 0, 0, 0, 0, 0, 0}};
    mission.object_groups[2] = {marker(0x177d, 7, -5, 9), marker(0x177d, 7, 3, 2),
                                marker(0x177d, 9, 4, 4), marker(6014, 7, 100, 100),
                                marker(0x177d, 9, -1, -2)};
    dfre::EventStage stage;
    stage.load(mission);
    require(stage.event_count() == 2 && stage.trigger_count() == 3 && stage.action_count() == 2, "Tables loaded");
    require(stage.trigger(0)[4] == 0xfffffffbu && stage.trigger(0)[5] == 2u && stage.trigger(0)[6] == 3u &&
                stage.trigger(0)[7] == 9u, "Trigger rect min/max over two markers");
    require(stage.trigger(1)[4] == 0xffffffffu && stage.trigger(1)[5] == 0xfffffffeu && stage.trigger(1)[6] == 4u &&
                stage.trigger(1)[7] == 4u, "Trigger rect second id");
    require(stage.trigger(2)[1] == 1 && stage.trigger(2)[2] == 3, "Other trigger kinds keep their words");
    require(stage.action(0)[3] == 0xfffffffbu && stage.action(0)[5] == 2u && stage.action(0)[6] == 3u &&
                stage.action(0)[7] == 9u, "Action rect min/max");
    require(stage.action(1)[1] == 5, "Other action kinds keep their words");
    require(stage.loader_flag() == 1, "Loader flag toggles once per load");
    stage.load(mission);
    require(stage.loader_flag() == 0, "Loader flag toggles again");
}

void test_binding_clears() {
    dfre::MissionData mission;
    mission.events = {event(0, 0, 0, 0, 0, 1, 1)};
    mission.triggers = {trigger(1, 10, 0, 42, 42, 42)};
    mission.actions = {{0, 0xd, 0, 42, 42, 42, 42, 42}};
    mission.object_groups[2] = {marker(0x177d, 5, 1, 1)};
    dfre::EventStage stage;
    stage.load(mission);
    require(stage.trigger(0)[1] == 0 && stage.trigger(0)[2] == 0, "Unmatched trigger kind cleared");
    require(stage.action(0)[1] == 0, "Unmatched action kind cleared");
    require(stage.evaluate_trigger(0) == 0, "Cleared trigger never true");
    ScriptHost host;
    stage.set_host(&host);
    stage.dispatch_action(0);
    require(host.log.empty(), "Cleared action does nothing");
}

void test_reset_and_flags() {
    ScriptHost host;
    dfre::EventStage stage(&host);
    dfre::MissionData mission;
    mission.events = {event(0, 0, 0, 0, 0, 0, 0)};
    stage.load(mission);
    stage.set_cursor(5);
    stage.set_flag_42edd0(1, 3);
    stage.set_flag_42ee00(1, 4);
    stage.set_flag_42ee30(2, 0, 5);
    stage.set_flag_42ee50(6);
    require(stage.flag_42e520(1, 3) && stage.flag_42e550(1, 4) && stage.flag_42e580(2, 0, 5) &&
                stage.flag_42e5b0(6), "Flag setters and tests agree");
    require(!stage.flag_42e520(1, 4) && !stage.flag_42e520(0, 3), "Flag word addressing is exact");
    stage.reset();
    require(stage.event_count() == 0, "Reset drops tables");
    require(!stage.flag_42e520(1, 3) && !stage.flag_42e550(1, 4) && !stage.flag_42e580(2, 0, 5),
            "Reset clears the reload arrays");
    require(stage.flag_42e5b0(6), "Reset keeps 0x67dbec, as 0x42ebe0 does");
    require(stage.cursor() == 5, "Reset keeps the cursor, as 0x42ebe0 does");
    require(stage.flags_6e0250() == 0 && stage.flags_6e0254() == 0, "Reset clears both action words");
}

void test_action_dispatch() {
    ScriptHost host;
    dfre::EventStage stage(&host);
    std::vector<dfre::EventTableRecord> actions;
    for (std::uint32_t kind = 0; kind <= 15; ++kind) actions.push_back({0, kind, 0, 9, 11, 0, 0, 0});
    actions.push_back({0, 16, 0, 0, 0, 0, 0, 0});
    stage.load_tables({}, {}, actions);
    for (std::size_t index = 0; index < 16; ++index) stage.dispatch_action(index);
    stage.dispatch_action(16);
    const std::vector<std::string> expected = {
        "", "45f670:9:11", "45f4c0:9", "4601a0:3", "45f5b0:9", "", "404dd0:9", "43f500:9",
        "444ca0:1", "444ca0:2", "444ca0:0", "460640:9:11", "460280:12", "460280:13", "", ""};
    std::size_t at = 0;
    for (const auto& wanted : expected) {
        if (wanted.empty()) continue;
        require(at < host.log.size() && host.log[at] == wanted, "Action dispatch routing and arguments");
        ++at;
    }
    require(at == host.log.size(), "Action dispatch call count");
    require(stage.flag_42e5b0(9), "Action kind 5 sets 0x67dbec bit 9");
    require(stage.flags_6e0254() == (1u << 9), "Action kind 14 sets 0x6e0254");
    require(stage.flags_6e0250() == (1u << 9), "Action kind 15 sets 0x6e0250");
}

void test_trigger_kinds() {
    ScriptHost host;
    dfre::EventStage stage(&host);
    // Object fields: index 2 live 7 start 5 flag 2; index 4 live -3 start -2.
    host.dwords[{dfre::event_object_field_7936a4, 2}] = 2;
    host.dwords[{dfre::event_object_field_7936a8, 2}] = 7;
    host.dwords[{dfre::event_object_field_7936ac, 2}] = 5;
    host.dwords[{dfre::event_object_field_7936a8, 4}] = 0xfffffffdu;
    host.dwords[{dfre::event_object_field_7936ac, 4}] = 0xfffffffeu;
    host.answer_460330 = 1;
    host.answer_460560 = 0;
    stage.load_tables({event(0, 0, 0, 0, 0, 0, 0), event(0, 0, 0, 0, 0, 0, 0, 1)},
                      {trigger(1, 1, 1, 3),  trigger(1, 2, 1, 4),   trigger(1, 3, 2),      trigger(1, 3, 5),
                       trigger(1, 4, 2),  trigger(1, 4, 4),   trigger(1, 5, 2),      trigger(1, 5, 4),
                       trigger(1, 6, 2, 2), trigger(1, 6, 2, 3), trigger(1, 7, 2, 0, 5), trigger(1, 9, 2),
                       trigger(1, 9, 5),  trigger(1, 10, 2),  trigger(1, 11, 2, 3),  trigger(1, 12, 2, 5),
                       trigger(1, 12, 2, 6), trigger(2, 0, 1), trigger(3, 0, 6),      trigger(4, 0),
                       trigger(5, 0),     trigger(0, 0),     trigger(6, 0),         trigger(1, 8, 0)},
                      {});
    stage.set_flag_42edd0(1, 3);
    stage.set_flag_42ee00(1, 4);
    stage.set_flag_42ee30(2, 0, 5);
    stage.set_flag_42ee50(6);
    stage.set_loader_flag(1);
    require(stage.trigger_count() == 24, "Fixture size");
    const std::vector<std::uint32_t> expected = {
        1, 1, 1, 0, // (1,1) (1,2) (1,3) hit, (1,3) miss
        0, 1, 1, 0, // (1,4) start 5 -> false, start -2 -> true; (1,5) 5 -> true, -2 -> false
        1, 0, 1,    // (1,6) live - start 2 >= 2 true, >= 3 false; (1,7) bit set
        0, 1,       // (1,9) 7 != 5 false, 0 == 0 true
        1, 0,       // (1,10) probe true; (1,11) probe false
        1, 0,       // (1,12) 5 <= 5 true, 6 <= 5 false
        1, 1, 1, 1, // kind 2 event 1 fired; kind 3 bit 6; kind 4 loader flag; kind 5 world
        0, 0, 0};   // kind 0, kind 6, subject 8 unknown
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(stage.evaluate_trigger(index) == expected[index], "Trigger kind result");
    }
    require(host.log.size() == 2 && host.log[0] == "460330:13" && host.log[1] == "460560:2:3",
            "Trigger probes receive record data");
}

void test_trigger_kind2_and_combination() {
    ScriptHost host;
    dfre::EventStage stage(&host);
    stage.load_tables({event(0, 0, 0, 0, 0, 0, 0), event(0, 0, 0, 0, 0, 0, 0, 1)},
                      {trigger(2, 0, 1), trigger(1, 5, 2), trigger(1, 4, 2), trigger(1, 5, 2)}, {});
    require(stage.evaluate_trigger(0) == 1, "Kind 2 reads the fired flag");
    require(stage.event_fired(1) == 1 && stage.event_fired(0) == 0, "Fired flag accessor");
    host.dwords[{dfre::event_object_field_7936ac, 2}] = 1; // (1,4) false, (1,5) true
    // AND: result 1 & 1.
    stage.load_tables({event(0, 0, 0, 0, 0, 3, 0)},
                      {trigger(1, 5, 2), trigger(1, 5, 2), trigger(1, 5, 2)}, {});
    require(stage.evaluate_event_triggers(0) == 1, "Combination and");
    // OR: previous word0 bit 1 combines with or, so false stays true.
    stage.load_tables({event(0, 0, 0, 0, 0, 2, 0)}, {trigger(1, 5, 2), trigger(1, 4, 2)}, {});
    stage.trigger(0)[0] = 2;
    require(stage.evaluate_event_triggers(0) == 1, "Combination or");
    // XOR: previous word0 bit 2 combines with xor, so true xors to false.
    stage.load_tables({event(0, 0, 0, 0, 0, 2, 0)}, {trigger(1, 5, 2), trigger(1, 5, 2)}, {});
    stage.trigger(0)[0] = 4;
    require(stage.evaluate_event_triggers(0) == 0, "Combination xor");
    // Word0 bit 0 negates the trigger's own result.
    stage.load_tables({event(0, 0, 0, 0, 0, 2, 0)}, {trigger(1, 5, 2), trigger(1, 5, 2)}, {});
    stage.trigger(1)[0] = 1;
    require(stage.evaluate_event_triggers(0) == 0, "Word0 bit 0 negates");
    stage.trigger(0)[0] = 1;
    require(stage.evaluate_event_triggers(0) == 0, "Negating the first trigger too");
    stage.load_tables({event(0, 0, 0, 0, 0, 0, 0)}, {}, {});
    require(stage.evaluate_event_triggers(0) == 1, "Empty trigger list is true");
}

void test_state_machine() {
    ScriptHost host;
    dfre::EventStage stage(&host);
    const dfre::EventTableRecord always = trigger(5, 0);
    const dfre::EventTableRecord act = {0, 1, 0, 3, 4, 0, 0, 0};
    host.dwords[{dfre::event_object_field_7936ac, 9}] = 1; // (1,4) false for the reset case
    // Fresh fire with no delay: dispatch on the first pass.
    host.log.clear();
    stage.load_tables({event(0, 0, 0, 0, 0, 1, 1)}, {always}, {act});
    stage.process_event(0);
    require(host.log.size() == 1 && host.log[0] == "45f670:3:4", "Fresh fire dispatch");
    require(stage.event_fired(0) == 1, "Fresh fire sets the fired flag");
    stage.process_event(0);
    require(host.log.size() == 1, "Fired event does not repeat without bit 0");
    // Delay reload: words[4] high 16 arms the delay before the first dispatch.
    host.log.clear();
    stage.load_tables({event(0, 0, 0, 0, 0x00020100u, 1, 1)}, {always}, {act});
    stage.process_event(0);
    require(stage.event(0).words[4] == 0x00020080u && host.log.empty(), "Delay reload armed");
    stage.process_event(0);
    require(host.log.size() == 1 && stage.event(0).words[4] == 0x00020000u, "Delay reaching zero fires");
    // Small delay: the first step already goes negative and fires at once.
    // The fire path never sets the fired flag (only the fresh path does), so a
    // delay without a reload dispatches on every pass.
    host.log.clear();
    stage.load_tables({event(0, 0, 0, 0x00030000u, 0x00000040u, 1, 1)}, {always}, {act});
    stage.process_event(0);
    require(host.log.size() == 1 && stage.event_fired(0) == 0, "Small delay fires immediately");
    require(stage.event(0).words[3] == 0x00030000u, "Repeat flag off keeps words[3]");
    stage.process_event(0);
    require(host.log.size() == 2 && stage.event_fired(0) == 1, "Fresh path fires again and sets the flag");
    // Repeat bit 0 with words[3] high 16 = 2: arm, expire, fire again.
    host.log.clear();
    stage.load_tables({event(1, 0, 0, 0x00020000u, 0, 1, 1)}, {always}, {act});
    stage.process_event(0);
    require(host.log.size() == 1 && stage.event(0).words[3] == 0x00020002u, "Repeat armed from high 16");
    stage.process_event(0);
    require(stage.event_fired(0) == 0 && stage.event(0).words[3] == 0x00020000u, "Repeat expiry clears the flag");
    stage.process_event(0);
    require(host.log.size() == 2 && stage.event_fired(0) == 1, "Repeat fires again");
    // Trigger false on the fire path clears the fired flag and both timers.
    host.log.clear();
    stage.load_tables({event(1, 0, 0, 0x00020000u, 0x00000100u, 1, 1)}, {trigger(1, 4, 9, 0)}, {act});
    stage.process_event(0);
    require(host.log.empty() && stage.event(0).words[4] == 0x00000080u, "Delay steps before the fire path");
    stage.process_event(0);
    require(stage.event_fired(0) == 0 && (stage.event(0).words[3] & 0xffffu) == 0 &&
                stage.event(0).words[4] == 0, "False trigger on the fire path resets the event");
    // Fired gate with no timers: the event waits for the flag to clear.
    host.log.clear();
    stage.load_tables({event(0, 0, 0, 0, 0, 1, 1, 1)}, {always}, {act});
    stage.process_event(0);
    require(host.log.empty(), "Fired flag gates the fresh path");
}

void test_scheduling() {
    ScriptHost host;
    dfre::EventStage stage(&host);
    const dfre::EventTableRecord always = trigger(5, 0);
    const dfre::EventTableRecord act = {0, 1, 0, 0, 0, 0, 0, 0};
    std::vector<dfre::MissionEvent> events;
    for (int index = 0; index < 9; ++index) events.push_back(event(0, 0, 0, 0, 0, 1, 1));
    stage.load_tables(events, {always}, {act});
    stage.tick(); // cursor 0: events 0 and 8
    require(host.log.size() == 2, "Tick stride is 8");
    require(stage.event_fired(0) == 1 && stage.event_fired(8) == 1 && stage.event_fired(1) == 0, "Cursor bucket 0");
    for (int step = 0; step < 7; ++step) stage.tick();
    require(stage.cursor() == 0, "Cursor wraps mod 8");
    require(stage.event_fired(1) == 1 && stage.event_fired(7) == 1 && stage.event_fired(8) == 1, "All buckets ran");
    // Bits 1 and 2 leave the per-frame tick but run from their own entries.
    stage.load_tables({event(2, 0, 0, 0, 0, 1, 1), event(4, 0, 0, 0, 0, 1, 1)}, {always}, {act});
    stage.tick();
    require(host.log.size() == 9, "Bits 1 and 2 skip the tick");
    stage.run_bit1_events();
    require(stage.event_fired(0) == 1 && stage.event_fired(1) == 0, "Bit 1 pass");
    stage.run_bit2_events();
    require(stage.event_fired(1) == 1, "Bit 2 pass");
    // Frame driver: tick every 16th step.
    stage.load_tables({event(0, 0, 0, 0, 0, 1, 1)}, {always}, {act});
    stage.set_cursor(0);
    host.log.clear();
    for (int step = 0; step < 15; ++step) stage.frame_driver_step();
    require(host.log.empty(), "Frame driver waits 16 steps");
    stage.frame_driver_step();
    require(host.log.size() == 1, "Frame driver ticks on the 16th step");
}

void test_loader_stage_matches_mission_decode() {
    // The stage must not change the decoded records when nothing binds.
    dfre::MissionData mission;
    mission.events = {event(0, 0, 0, 0, 0, 1, 1), event(2, 1, 1, 0, 0, 0, 0)};
    mission.triggers = {trigger(1, 4, 3), trigger(3, 0, 5)};
    mission.actions = {{0, 11, 0, 1, 2, 0, 0, 0}};
    dfre::EventStage stage;
    stage.load(mission);
    for (std::size_t index = 0; index < mission.events.size(); ++index) {
        require(stage.event(index).words == mission.events[index].words, "Event words untouched");
    }
    for (std::size_t index = 0; index < mission.triggers.size(); ++index) {
        require(stage.trigger(index) == mission.triggers[index], "Trigger words untouched");
    }
    require(stage.action(0) == mission.actions[0], "Action words untouched");
}
void test_current_fields_and_errors(){
    struct Host:ScriptHost {
        std::function<void()> action_effect,trigger_effect;
        void action_45f670(std::uint32_t a,std::uint32_t b)override{ScriptHost::action_45f670(a,b);if(action_effect)action_effect();}
        std::uint32_t probe_460560(std::uint32_t a,std::uint32_t b)override{auto result=ScriptHost::probe_460560(a,b);if(trigger_effect)trigger_effect();return result;}
    }host;
    dfre::EventStage stage(&host);
    const std::vector<dfre::EventTableRecord> actions={{0,1,0,3,4,0,0,0},{0,14,0,5,0,0,0,0},{0,15,0,6,0,0,0,0}};
    stage.load_tables({event(0,0,0,0x06000200,128,0,1)}, {},actions);
    host.action_effect=[&]{stage.event(0).words[3]=0x06000400;};
    stage.process_event(0);require(stage.event(0).words[3]==0x06000380,"Repeat timer reads changes from the action");
    stage.load_tables({event(0,0,0,0,0,0,2)}, {},actions);
    host.action_effect=[&]{stage.event(0).words[0]=1;stage.event(0).words[3]=0x04000000;stage.event(0).words[2]=1;};
    stage.process_event(0);require(stage.event(0).words[3]==0x04000400&&stage.flags_6e0250()==64&&stage.flags_6e0254()==0,"Current action base and repeat flag drive the tail");
    stage.load_tables({event(1,0,0,0,0,0,1)}, {},actions);
    host.action_effect=[&]{stage.event(0).words[3]=123;};stage.process_event(0);
    require(stage.event(0).words[3]==123&&stage.event_fired(0)==0,"Zero reload clears only the fired byte");
    stage.load_tables({event(0,0,0,0,0,0,3)}, {},actions);
    host.action_effect=[&]{stage.event(0).words[3]=99;throw std::runtime_error("action stopped");};
    bool stopped=false;try{stage.process_event(0);}catch(const std::runtime_error& e){stopped=std::string(e.what())=="action stopped";}
    require(stopped&&stage.event_fired(0)==1&&stage.event(0).words[3]==99&&stage.flags_6e0254()==0&&stage.flags_6e0250()==0,"Action error preserves writes and stops later actions");
    stage.load_tables({event(0,0,0,0,0,1,0)}, {trigger(1,11)},{});
    host.trigger_effect=[&]{stage.event(0).words[0]=7;throw std::runtime_error("trigger stopped");};
    stopped=false;try{stage.process_event(0);}catch(const std::runtime_error& e){stopped=std::string(e.what())=="trigger stopped";}
    require(stopped&&stage.event(0).words[0]==7&&stage.event_fired(0)==0,"Trigger error preserves writes before the fire flag");
    dfre::EventStage unbound;unbound.load_tables({}, {},actions);
    bool missing=false;try{unbound.dispatch_action(0);}catch(const dfre::EventError&){missing=true;}
    require(missing,"Missing action host must fail");
    dfre::EventHost empty;unbound.set_host(&empty);missing=false;try{unbound.dispatch_action(0);}catch(const dfre::EventError&){missing=true;}
    require(missing,"Missing action method must fail");
    unbound.load_tables({}, {trigger(5,0),trigger(1,11),trigger(1,3)},{});
    for(std::size_t index=0;index<3;++index){missing=false;try{unbound.evaluate_trigger(index);}catch(const dfre::EventError&){missing=true;}require(missing,"Missing trigger method must fail");}
    unbound.set_host(nullptr);unbound.load_tables({event(0,0,0,0,0,0,1)}, {},{{0,5,0,3,0,0,0,0}});
    unbound.process_event(0);require(unbound.flag_42e5b0(3),"Internal action needs no foreign host");
}
void test_index_read_order_and_bounds(){
    struct Reads:dfre::EventHost {
        dfre::EventStage* stage{};unsigned count{};std::vector<std::uint32_t> addresses,indexes;
        std::uint32_t object_index_dword(std::uint32_t address,std::uint32_t index)override{
            addresses.push_back(address);indexes.push_back(index);++count;
            stage->trigger(0)[3]=3;stage->trigger(0)[4]=2;
            return address==dfre::event_object_field_7936a8?7:5;
        }
    }host;
    dfre::EventStage stage(&host);host.stage=&stage;
    stage.load_tables({}, {trigger(1,6,2,99)},{});
    require(stage.evaluate_trigger(0)==1&&host.indexes==std::vector<std::uint32_t>{2,2},"Count trigger keeps the entry index and reads current threshold");
    host.count=0;host.addresses.clear();host.indexes.clear();stage.load_tables({}, {trigger(1,9,2)},{});
    require(stage.evaluate_trigger(0)==0&&host.indexes==std::vector<std::uint32_t>{2,2}&&host.addresses[0]==dfre::event_object_field_7936a8,"Equality trigger reads live then start for the same index");
    stage.load_tables({}, {trigger(1,12,2,9)},{});
    require(stage.evaluate_trigger(0)==0,"Threshold trigger captures its word before the host read");
    const auto rejects=[](const std::function<void()>& call){bool rejected=false;try{call();}catch(const dfre::EventError&){rejected=true;}require(rejected,"Invalid event index must fail");};
    rejects([&]{stage.flag_42e520(1000,0);});rejects([&]{stage.set_flag_42ee30(1000,0,0);});
    stage.load_tables({event(0,0,0,0,0,1,0)}, {trigger(2,0,1)},{});
    rejects([&]{stage.evaluate_trigger(0);});
    stage.load_tables({event(0,0,0,0,0,0,1)}, {},{});
    rejects([&]{stage.process_event(0);});
    require(stage.event_fired(0)==1,"Run error keeps the reached fired write");
}

void test_signed_counter_and_external_byte(){
    for(std::uint32_t counter:{0x7fffffffu,0x80000000u,0xffffffffu,15u,16u}){
        dfre::EventStage stage;stage.load_tables({event(0,0,0,0,0,0,1)}, {},{{0,5,0,1,0,0,0,0}});
        stage.set_frame_counter(counter);stage.frame_driver_step();const bool fire=counter==15||counter==16;
        require(stage.flag_42e5b0(1)==fire&&stage.frame_counter()==(fire?0:counter+1),"Frame counter uses the signed source gate");
    }
    struct External:dfre::EventHost{
        std::uint32_t value=0;unsigned calls=0;
        std::uint32_t external_event_fired(std::size_t index)override{require(index==1,"External fired index changes");++calls;return value;}
    }host;
    dfre::EventStage stage(&host);stage.load_tables({event(0,0,0,0,0,1,0)}, {trigger(2,0,1)},{});
    host.value=0x100;require(stage.evaluate_trigger(0)==0,"External fired read keeps only the source byte");
    host.value=0x101;require(stage.evaluate_trigger(0)==1&&host.calls==2,"External fired read requires current host data");
}

void test_action_binding_base(){
    dfre::MissionData mission;mission.events={event(0,1,2,0,0,1,1)};
    mission.triggers={trigger(0,0),trigger(4,0)};mission.actions.resize(3);mission.actions[2]={0,12,0,77,8,0,0,0};
    mission.object_groups[2]={marker(0x177d,77,-10,20),marker(0x177d,77,30,-40)};
    dfre::EventStage stage;stage.load(mission);const auto& a=stage.action(2);
    require(a[3]==0xfffffff6&&a[5]==0xffffffd8&&a[6]==30&&a[7]==20&&a[4]==8,"Action binder uses action run base");
}

} // namespace

int main() {
    try {
        test_current_fields_and_errors();
        test_action_binding_base();
        test_index_read_order_and_bounds();
        test_signed_counter_and_external_byte();
        test_trigger_binding();
        test_binding_clears();
        test_reset_and_flags();
        test_action_dispatch();
        test_trigger_kinds();
        test_trigger_kind2_and_combination();
        test_state_machine();
        test_scheduling();
        test_loader_stage_matches_mission_decode();
        std::cout << "events checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
