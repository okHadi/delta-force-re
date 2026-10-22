#include "dfre/mission_logic.hpp"

#include <iostream>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

using dfre::EventTableRecord;
using dfre::MissionEvent;
using dfre::MissionLogicClient;
using dfre::MissionLogicSquadRow;
using dfre::MissionResultMode;

MissionEvent event(std::uint32_t w0, std::uint32_t trigger_index, std::uint32_t action_index,
                   std::uint32_t trigger_count, std::uint32_t action_count) {
    MissionEvent result;
    result.words = {w0, trigger_index, action_index, 0, 0,
                    (trigger_count & 0xffu) << 8 | (action_count & 0xffu) << 16};
    return result;
}

EventTableRecord trigger(std::uint32_t kind, std::uint32_t subject, std::uint32_t a = 0,
                         std::uint32_t b = 0) {
    // (1, *) trigger kinds read object counters; kind 5 answers through the
    // host 0x67F520/0x67F549 hooks and is the always-true fixture here.
    return {0, kind, subject, a, b, 0, 0, 0};
}

EventTableRecord action(std::uint32_t kind, std::uint32_t word3 = 0, std::uint32_t word4 = 0) {
    return {0, kind, 0, word3, word4, 0, 0, 0};
}

// Host with one local client and scripted fields; records forwarded actions.
struct TestHost : dfre::MissionLogicHost {
    std::vector<MissionLogicClient> clients_data{{true, 3, 0, 0, 0x1234, 0x11, 0x22}};
    std::vector<MissionLogicSquadRow> squad_data{{1, 2, 3, 0x40000, 9}};
    bool local_present = true;
    std::uint32_t actor_6e = 1u << 16; // >> 16 == 1, equals the win flag
    std::int32_t local_index = 0;
    std::vector<std::string> log;
    std::vector<std::pair<std::size_t, std::uint16_t>> state_writes;
    std::vector<std::uint32_t> sounds;

    std::size_t clients(MissionLogicClient* out, std::size_t capacity) override {
        const std::size_t count = std::min(capacity, clients_data.size());
        for (std::size_t i = 0; i < count; ++i) out[i] = clients_data[i];
        return count;
    }
    void set_client_state(std::size_t slot, std::uint16_t state) override {
        state_writes.emplace_back(slot, state);
        if (slot < clients_data.size()) clients_data[slot].state = state;
    }
    std::size_t squad_rows(MissionLogicSquadRow* out, std::size_t capacity) override {
        const std::size_t count = std::min(capacity, squad_data.size());
        for (std::size_t i = 0; i < count; ++i) out[i] = squad_data[i];
        return count;
    }
    bool local_player_present() override { return local_present; }
    std::uint32_t local_player_actor_6e() override { return actor_6e; }
    std::int32_t local_actor_index() override { return local_index; }
    std::string client_name(std::uint32_t actor_index) override {
        return actor_index == 0 ? "BRAVO TWO" : std::string{};
    }
    std::uint32_t client_actor_6e(std::uint32_t actor_index) override {
        return actor_index == 0 ? (1u << 16) : 0;
    }
    void weapons_tick_482b00() override {} // Explicit inspection fixture.
    void play_sound_43e720(std::uint32_t id) override { sounds.push_back(id); }
    bool world_67f520_is_zero() override { return true; }
    bool world_67f549_bit1() override { return false; }
    void action_404dd0(std::uint32_t id) override { log.push_back("404dd0:" + std::to_string(id)); }
};

dfre::AssetBytes empty_profile() {
    dfre::AssetBytes profile(0xa90, 0);
    profile[0] = 'P';
    profile[1] = 'L';
    profile[2] = 'Y';
    profile[3] = 0x1b;
    profile[0x28] = 2; // campaign 2
    return profile;
}

std::uint32_t word(const dfre::AssetBytes& data, std::size_t at) {
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8) |
           (std::uint32_t(data[at + 2]) << 16) | (std::uint32_t(data[at + 3]) << 24);
}

void begin(dfre::MissionLogic& logic, TestHost& host, std::vector<EventTableRecord> actions,
           std::vector<MissionEvent> events = {}, std::vector<EventTableRecord> triggers = {}) {
    logic.set_host(&host);
    logic.begin_tables(std::move(events), std::move(triggers), actions, true);
    logic.set_result_context(2, 3);
}

// The result message is flushed in the same frame that the events stage ran
// (0x4421D0) and dispatched by the next update (0x46F5C0), like the original.
void deliver(dfre::MissionLogic& logic) {
    logic.inspection_step();
    logic.inspection_step();
}

void test_win_chain_and_profile() {
    dfre::MissionLogic logic;
    TestHost host;
    begin(logic, host, {action(dfre::mission_action_win)});
    logic.dispatch_action(0);
    require(logic.mission_ended(), "win action ends the mission once");
    require(logic.outputs().end_requests.size() == 1 &&
                logic.outputs().end_requests[0] == MissionResultMode::win,
            "win mode recorded");
    require(logic.outputs().messages_sent.size() == 1 && logic.outputs().messages_sent[0] == 0x1E,
            "one result message enqueued");
    require(logic.outputs().client_states_reset.size() == 1 && host.state_writes.size() == 1 &&
                host.state_writes[0].second == 2,
            "client state 3 rewritten to 2");

    const auto& record = logic.messages().queue().record(logic.messages().queue().last_slot());
    require(record.id == 0x1E && record.length == 0xCE && record.repeats == 0x3E9,
            "result record id/length/repeats");
    require(record.payload[0] == 1 && record.payload[1] == 1 && record.payload[3] == 0,
            "result payload mode/count/actor row");
    require(record.payload[0x25] == 0x34 && record.payload[0x26] == 0x12,
            "row score at +0x25");
    require(record.payload[0x69] == 0x11 && record.payload[0x8B] == 0x22, "row bytes");
    require(record.payload[0xAD] == 1 && record.payload[0xB9] == 3 && record.payload[0xCB] == 9,
            "squad row fields");

    // A second end action is ignored by the 0x67DBC0 guard.
    logic.clear_outputs();
    auto copy = logic.events().action(0);
    logic.events().action(0) = action(dfre::mission_action_fail);
    logic.dispatch_action(0);
    require(logic.outputs().end_requests.empty() && logic.outputs().messages_sent.empty(),
            "second end action ignored");
    logic.events().action(0) = copy;

    require(!logic.outputs().result_dispatched, "result not delivered before the next update");
    require(!logic.profile_win(), "flag stays 0 until the bus delivers");
    deliver(logic);
    require(logic.profile_win(), "0x67F718 becomes 1 after the bus round trip");
    require(logic.result_block().flag_67f718 == 1 && logic.result_block().count_67f71c == 1,
            "result block header");
    require(logic.result_block().local_found && logic.result_block().local_index_67f720 == 0,
            "local row matched");
    require(logic.result_block().rows.size() == 1 && logic.result_block().rows[0].name == "BRAVO TWO" &&
                logic.result_block().rows[0].score == 0x1234 &&
                logic.result_block().rows[0].byte_69 == 0x11 &&
                logic.result_block().rows[0].byte_8b == 0x22 &&
                logic.result_block().rows[0].client_high == 1,
            "decoded row fields");
    require(logic.result_block().stats[0].a == 1 && logic.result_block().stats[0].b == 2 &&
                logic.result_block().stats[0].c == 3 && logic.result_block().stats[0].byte_cb == 9 &&
                logic.result_block().stats[0].ratio == std::int32_t((0x40000ll * 65536) / 3),
            "decoded stat row and ratio");
    require(host.sounds.size() == 1 && host.sounds[0] == 499, "0x43E720(499) when flag equals local id");

    // Leave applies the completion word and mask through mission_results.
    auto profile = empty_profile();
    logic.events().set_flag_42ee50(5);
    logic.events().set_flag_42ee50(8);
    const auto leave = logic.leave_and_apply(profile);
    require(leave.profile_win && leave.profile_applied, "leave reports and applies the win");
    require(word(profile, dfre::CampaignProfile::completion_offset(2, 3)) == 1, "completion word written");
    require(word(profile, dfre::CampaignProfile::mask_offset(2)) == (1u << 5 | 1u << 8),
            "objective mask written");
}

void test_fail_and_neutral_write_nothing() {
    for (const auto kind : {dfre::mission_action_fail, dfre::mission_action_neutral}) {
        dfre::MissionLogic logic;
        TestHost host;
        begin(logic, host, {action(kind)});
        logic.dispatch_action(0);
        deliver(logic);
        require(!logic.profile_win(), "fail/neutral is not a profile win");
        auto profile = empty_profile();
        const auto leave = logic.leave_and_apply(profile);
        require(!leave.profile_applied && !leave.profile_win, "no profile write");
        require(word(profile, 0x74 + 4) == 0 && word(profile, 0xb4) == 0, "profile untouched");
        require(logic.result_block().flag_67f718 == (kind == dfre::mission_action_fail ? 2u : 0u),
                "flag stores the mode");
    }
}

void test_event_dispatch_chain() {
    dfre::MissionLogic logic;
    TestHost host;
    // One always-true (kind 5) trigger runs two actions: objective bit 5 and
    // the win request. The event has bit 0 clear, so tick()/process_event is
    // driven directly like the runtime does inside the frame function.
    begin(logic, host, {action(dfre::mission_action_objective, 5), action(dfre::mission_action_win)},
          {event(0, 0, 0, 1, 2)}, {trigger(5, 0)});
    logic.events().process_event(0);
    require((logic.events().flags_67dbec() & (1u << 5)) != 0, "objective bit set by action 5");
    require(logic.outputs().end_requests.size() == 1, "end request from the event chain");
    deliver(logic);
    require(logic.profile_win(), "event chain wins");
    auto profile = empty_profile();
    const auto leave = logic.leave_and_apply(profile);
    require(leave.profile_applied && word(profile, dfre::CampaignProfile::mask_offset(2)) == (1u << 5),
            "event objective bits reach the profile mask");
}

void test_subgoal_fail_and_text() {
    dfre::MissionLogic logic;
    TestHost host;
    begin(logic, host, {action(dfre::mission_action_subgoal, 3), action(dfre::mission_action_subfail, 9),
                        action(dfre::mission_action_text, 0x21)});
    for (std::size_t i = 0; i < 3; ++i) logic.dispatch_action(i);
    require(logic.events().flags_6e0254() == (1u << 3), "SUBGOAL bit is a mask only");
    require(logic.events().flags_6e0250() == (1u << 9), "SUBFAIL bit is a mask only");
    require(!logic.mission_ended(), "masks never end a mission");
    require(logic.outputs().triggered_texts.size() == 1 && logic.outputs().triggered_texts[0] == 0x21,
            "triggered text recorded");
    require(host.log.size() == 1 && host.log[0] == "404dd0:33", "text forwarded to the host");
}

void test_leave_runs_bit2_first() {
    dfre::MissionLogic logic;
    TestHost host;
    // Event words[0] bit 2 -> 0x42EBA0 at leave; the end it requests cannot be
    // delivered before the profile decision.
    auto win = action(dfre::mission_action_win);
    win[0] = 4;
    begin(logic, host, {win}, {event(4, 0, 0, 1, 1)}, {trigger(5, 0)});
    const auto leave = logic.leave();
    require(logic.mission_ended() && !leave.profile_win,
            "leave-time end is queued but not delivered");
    require(leave.mission_ended, "leave reports the guard state");
    auto profile = empty_profile();
    const auto applied = logic.leave_and_apply(profile);
    require(!applied.profile_applied, "queued result cannot win the profile at leave");
}

void test_frame_counters_and_status() {
    dfre::MissionLogic logic;
    TestHost host;
    begin(logic, host, {action(dfre::mission_action_win)});
    for (int i = 0; i < 15; ++i) logic.inspection_step();
    require(logic.events().flags_67dbec() == 0, "events do not tick before the 16th frame");
    logic.dispatch_action(0);
    deliver(logic);
    require(logic.profile_win(), "win stays available after frame counters");
    require(logic.game_status_67dbbc() == 0, "status stays 0 without a 0x33 message");
}

// The base host has no clients: the guard still ends the mission, but no
// message is produced and the profile never wins.
void test_no_client_no_message() {
    dfre::MissionLogic logic;
    TestHost host; host.clients_data.clear(); host.squad_data.clear();
    logic.set_host(&host);
    logic.begin_tables({}, {}, {action(dfre::mission_action_win)}, true);
    logic.dispatch_action(0);
    require(logic.mission_ended(), "guard ends even without clients");
    require(logic.outputs().messages_sent.empty(), "no client means no message");
    deliver(logic);
    require(!logic.profile_win(), "no message means no win");
}

void test_default_host_no_result() {
    dfre::MissionLogic logic;
    TestHost host;
    // Begin without a local player: 0x471010 requires 0xC23820 != 0.
    host.local_present = false;
    begin(logic, host, {action(dfre::mission_action_win)});
    logic.dispatch_action(0);
    deliver(logic);
    require(!logic.profile_win(), "no local player means no flag write");
}
void test_complete_frame() {
    using U=std::uint32_t;
    const std::array<U,22> addresses{0x67f520,0x67f528,0x67dbc0,0x67f534,0x6e558c,0x67dbbc,0x67f538,0x67dbe8,0x6e4730,0x6e4734,0x6e4738,0x6e473c,0x6e4740,0x6e4744,0x6e4748,0x6e474c,0x6e4750,0x6e4754,0x6e4758,0x6e475c,0x8cfa18,0x8cfa1c};
    std::map<U,U> cells;for(auto at:addresses)cells[at]=0;cells[0x67f528]=1;cells[0x67dbe8]=0xffffffff;
    std::array<std::array<std::uint8_t,512>,17> clients{};std::array<std::vector<std::array<std::uint8_t,0x114>>,4> actors;actors[0].resize(1);
    auto read=[](const auto& image,U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(image.at(at+i))<<(i*8);return n;};
    auto put=[](auto& image,U at,U width,U n){for(U i=0;i<width;++i)image.at(at+i)=std::uint8_t(n>>(i*8));};
    dfre::MissionLogicFrameBindings b;b.cell=[&](U at){return cells.at(at);};b.store=[&](U at,U n){cells.at(at)=n;};
    bool failed=false;try{dfre::mission_logic_frame(b);}catch(const std::runtime_error&){failed=true;}require(failed,"missing reached frame stage fails");
    cells[0x67f520]=1;cells[0x67f528]=0;dfre::mission_logic_frame(b);require(cells[0x6e4734]==0,"peer gate returns before effects");cells[0x67f520]=0;cells[0x67f528]=1;
    b.client=[&](auto slot,U at,U width){return read(clients.at(slot),at,width);};b.client_store=[&](auto slot,U at,U width,U n){put(clients.at(slot),at,width,n);};b.client_actor_flags=[](auto)->std::optional<std::uint8_t>{return std::nullopt;};
    b.actor=[&](U group,auto slot,U at,U width){return read(actors.at(group).at(slot),at,width);};b.actor_byte_store=[&](U group,auto slot,U at,std::uint8_t n){put(actors.at(group).at(slot),at,1,n);};
    std::vector<U> calls;b.call=[&](U site,const auto&){calls.push_back(site);return 0u;};
    dfre::mission_logic_frame(b);require(calls==std::vector<U>({0x460e40,0x4477f0,0x444e30,0x444f60,0x4421d0}),"full late frame order");require(read(clients[16],0x94,4)==1,"all seventeen client slots are current");
    cells[0x6e4734]=0x7fffffff;cells[0x6e473c]=0x7fffffff;calls.clear();dfre::mission_logic_frame(b);
    require(cells[0x6e4734]==0x80000000&&cells[0x6e473c]==0,"signed event and unsigned maintenance counters");require(std::find(calls.begin(),calls.end(),0x42eaf0)==calls.end(),"signed event edge skips tick");
    cells[0x6e4734]=15;cells[0x6e4738]=62;b.call=[&](U site,const auto&){calls.push_back(site);if(site==0x42eaf0)cells[0x6e4734]=999;if(site==0x44a200)cells[0x6e473c]=310;return 0u;};
    dfre::mission_logic_frame(b);require(cells[0x6e4734]==0&&cells[0x6e473c]==0,"foreign changes observed and completed timer reset follows effect");
    cells[0x67f520]=1;cells[0x67f538]=0;cells[0x8cfa1c]=1;put(actors[0][0],0x110,4,0x429bc0);actors[0][0][0x6d]=1;
    std::vector<U> messages;b.packet_padding=[](U){return std::array<std::uint8_t,3>{7,8,9};};b.send=[&](U id,const auto& bytes){messages.push_back(id);if(id==0x34){require(bytes.size()==24&&bytes[21]==7&&bytes[22]==8&&bytes[23]==9,"explicit source packet padding");actors[0][0][0x6d]=0xf7;actors[0][0][0x6e]=0x20;}else if(id==0x4f)actors[0][0][0x6e]=0xff;};
    dfre::mission_logic_frame(b);require(messages==std::vector<U>({0x34,0x4f})&&actors[0][0][0x6d]==0xf6&&actors[0][0][0x6e]==0xdf,"current replication flags preserved after each delivery");
    actors[0][0][0x6d]=1;b.send=[&](U,const auto&){actors[0][0][0x6d]=0xf7;throw std::runtime_error("delivery failure");};failed=false;
    try{dfre::mission_logic_frame(b);}catch(const std::runtime_error& e){failed=std::string(e.what())=="delivery failure";}require(failed&&actors[0][0][0x6d]==0xf7,"failed delivery preserves current actor writes");
}

struct ResultFixture {
    using U=std::uint32_t;dfre::MissionLogicResultBindings b;std::map<U,U> cells;
    std::array<std::array<std::uint8_t,512>,17> clients{};std::array<U,17> actor_meta{};
    std::vector<U> calls;std::vector<std::size_t> published;std::map<U,std::string> names;std::optional<U> local{0};
    ResultFixture(){
        b.state.cell=[&](U at){return cells[at];};b.state.store=[&](U at,U n){cells[at]=n;};
        b.state.client=[&](auto slot,U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(clients.at(slot).at(at+i))<<(i*8);return n;};
        b.state.client_store=[&](auto slot,U at,U width,U n){for(U i=0;i<width;++i)clients.at(slot).at(at+i)=std::uint8_t(n>>(i*8));};
        b.state.call=[&](U site,const std::vector<U>&){calls.push_back(site);return 0u;};
        b.actor_identity=[](U group,auto slot){require(group==3,"result group three");return U(slot);};b.actor_source_address=[](U actor){return 0x7cca80u+actor*0x114u;};
        b.local_actor=[&]{return local;};b.actor_word=[&](U actor,U at,U width){require(at==0x6e&&width==4,"result actor metadata field");return actor_meta.at(actor);};
        b.actor_name=[](U){return std::string("CLIENT");};b.store_name=[&](U at,const std::string& name){names[at]=name;};
        b.publish=[&](auto slot,U id,auto&){require(id==0x1e,"result client id");published.push_back(slot);};
    }
    void field(std::size_t slot,U at,U n,U width=4){b.state.client_store(slot,at,width,n);}
};

void test_live_results(){
    using U=std::uint32_t;dfre::MissionLogicResultPacket packet{};
    {ResultFixture f;f.cells[0x67dbc0]=1;dfre::mission_logic_request_result(f.b,1,packet);require(f.calls==std::vector<U>{0x446100}&&f.published.empty(),"end cleanup precedes guard");}
    {ResultFixture f;f.cells[0x67dbc0]=1;f.cells[0x67f520]=1;f.cells[0x67f534]=1;f.cells[0x67f53c]=2;f.field(0,0x18c,1,1);f.field(0,0x1c4,3,2);f.field(0,8,0);packet.fill(0xa5);
        f.b.state.call=[&](U site,const std::vector<U>&){f.calls.push_back(site);if(site==0x446100)f.cells[0x67dbc0]=0;return 0u;};
        f.b.publish=[&](auto slot,U id,auto& p){require(id==0x1e&&p[0]==1&&p[2]==1,"live result prefix");f.published.push_back(slot);if(slot==0){f.field(1,0x18c,1,1);f.field(1,8,1);f.field(1,0x1c4,4,2);}};
        dfre::mission_logic_request_result(f.b,1,packet);
        require(f.published==std::vector<std::size_t>{0,1}&&packet[1]==2,"publication changes later client occupancy");require(f.clients[0][0x1c4]==2&&f.clients[1][0x1c4]==2,"result current client state");
        require(f.cells[0x6e4758]==0xf00&&f.cells[0x6e5774]==1&&f.cells[0x6e576c]==1&&f.cells[0x67dbc0]==1,"result final cells");require(packet[0x24]==0xa5,"result retains untouched caller bytes");}
    {ResultFixture f;f.field(0,0x18c,1,1);f.field(0,0x1a8,0xffffffff);f.field(0,0x1ac,255);f.field(0,0x1a0,0x12345678);packet.fill(0xa5);dfre::mission_logic_build_result(f.b,packet);require(packet[0x69]==255&&packet[0x8b]==255&&packet[0x25]==0x78&&packet[0x26]==0x56,"build signed clamp and low word");}
    {ResultFixture f;packet.fill(0);f.cells[0x67f52c]=0;f.cells[0x67f53c]=3;
        for(U i=0;i<2;++i){f.field(i,0x18c,1,1);f.field(i,0x18d,1,1);f.field(i,8,i);f.field(i,0x194,10+i);f.field(i,0x1bc,20+i);}
        const auto read_client=f.b.state.client;const auto read_cell=f.b.state.cell;U filter_reads=0,mode_reads=0;
        f.b.state.call=[&](U site,const auto&){if(site==0x446da0)f.cells[0x67f52c]=1;return 0u;};
        f.b.state.client=[&](auto slot,U at,U width){f.cells[0x67f52c]=0;return read_client(slot,at,width);};
        f.b.state.cell=[&](U at){const auto value=read_cell(at);if(at==0x67f52c)++filter_reads;if(at==0x67f53c){++mode_reads;f.cells[at]=5;}return value;};
        dfre::mission_logic_build_result(f.b,packet);require(packet[1]==2&&filter_reads==1,"filter snapshot follows builder effect and precedes client scan");require(mode_reads==2&&packet[0x25]==10&&packet[0x27]==21,"one current mode snapshot per result row");}
    {ResultFixture f;packet.fill(0);packet[0]=2;packet[1]=1;packet[3]=0;packet[0x25]=0;packet[0x26]=0x80;packet[0xad]=0xfe;packet[0xae]=0xff;packet[0xb9]=0xff;packet[0xba]=0xff;packet[0xbf]=1;f.actor_meta[0]=0xfffe0000;
        dfre::mission_logic_receive_result(f.b,packet.data(),packet.size());require(f.cells[0x67dbc0]==1&&f.cells[0x67f714]==0xfffffffe&&f.cells[0x67f738]==0xffff8000,"receive signed words");require(f.cells[0x67fbec]==0xfffffffe&&f.cells[0x67fc1c]==0xffff0000&&f.cells[0x67fc20]==0xffffffff,"receive signed ratio and zero denominator");require(f.names.at(0x67f724)=="CLIENT"&&f.cells[0x67f720]==0,"receive actor slot zero");}
    {ResultFixture f;packet.fill(0);packet[0]=1;packet[1]=2;packet[3]=0;packet[4]=1;f.actor_meta[0]=0x10000;
        f.b.actor_name=[&](U actor){f.local=actor;f.cells[0x67f71c]=1;packet[0x25]=7;return std::string("NEW");};
        dfre::mission_logic_receive_result(f.b,packet.data(),packet.size());require(f.names.size()==1&&f.cells[0x67f738]==7&&f.calls==std::vector<U>{0x43e720},"name effect updates current packet and loop count");}
    {ResultFixture f;packet.fill(0);packet[1]=1;packet[3]=0;packet[0x8b]=9;f.cells[0x67f720]=99;
        const auto store=f.b.state.store;f.b.state.store=[&](U at,U value){store(at,value);if(at==0x67f740)f.local=1;if(at==0x67f73c)f.local=0;};
        dfre::mission_logic_receive_result(f.b,packet.data(),packet.size());require(f.cells[0x67f73c]==9&&f.cells[0x67f720]==99,"local identity snapshot precedes byte8B display store");}
    {ResultFixture f;packet.fill(0);packet[0xb9]=1;packet[0xbf]=1;
        const auto store=f.b.state.store;f.b.state.store=[&](U at,U value){store(at,value);if(at==0x67fc10)packet[0xb9]=2;};
        dfre::mission_logic_receive_result(f.b,packet.data(),packet.size());require(f.cells[0x67fc04]==1&&f.cells[0x67fc1c]==32768,"ratio rereads denominator after display stores");}
    {ResultFixture f;f.local.reset();packet.fill(0);dfre::mission_logic_receive_result(f.b,packet.data(),packet.size());require(f.cells[0x67dbc0]==1&&f.names.empty()&&!f.cells.count(0x67f718),"missing local actor retains initial end write");}
    {ResultFixture f;packet.fill(0);packet[0xb9]=1;packet[0xbf]=0xff;packet[0xc0]=0xff;packet[0xc1]=0xff;packet[0xc2]=0x7f;bool failed=false;try{dfre::mission_logic_receive_result(f.b,packet.data(),packet.size());}catch(const std::runtime_error&){failed=true;}require(failed&&f.cells[0x67fc04]==1&&!f.cells.count(0x67fc1c),"signed division fault retains earlier writes");}
    {ResultFixture f;packet.fill(0);f.b.publish={};f.field(0,0x18c,1,1);f.field(0,0x1c4,3,2);bool failed=false;try{dfre::mission_logic_request_result(f.b,1,packet);}catch(const std::runtime_error&){failed=true;}require(failed&&f.clients[0][0x1c4]==2&&f.cells[0x67dbc0]==0,"missing outbox body fails after prior writes");}
    for(auto size:{std::size_t(0),std::size_t(1),std::size_t(0x25),std::size_t(0x26),std::size_t(0xad),std::size_t(0xae),std::size_t(0xcd),std::size_t(0xcf)}){ResultFixture f;bool failed=false;try{dfre::mission_logic_receive_result(f.b,packet.data(),size);}catch(const std::runtime_error&){failed=true;}require(failed&&f.cells.empty()&&f.names.empty(),"invalid live result packet has no writes");}
    {ResultFixture f;bool failed=false;try{dfre::mission_logic_receive_result(f.b,nullptr,0xce);}catch(const std::runtime_error&){failed=true;}require(failed&&f.cells.empty(),"null live result packet has no writes");}
}

void test_result_input_boundary(){
    dfre::MissionLogic logic;TestHost host;begin(logic,host,{});std::array<std::uint8_t,0xce> payload{};payload[0]=1;logic.inspect_result_message(payload.data(),payload.size());
    const auto sounds=host.sounds.size();payload[0]=2;payload[1]=17;
    for(const auto length:{std::size_t(0),std::size_t(1),std::size_t(2),std::size_t(0x25),std::size_t(0x26),std::size_t(0xad),std::size_t(0xae),std::size_t(0xcd)}){
        bool failed=false;try{logic.inspect_result_message(payload.data(),length);}catch(const std::runtime_error&){failed=true;}
        require(failed&&logic.result_block().flag_67f718==1&&logic.result_block().count_67f71c==0&&logic.result_block().rows.empty()&&host.sounds.size()==sounds,"truncated result has no partial publication");
    }
    bool failed=false;try{logic.inspect_result_message(nullptr,payload.size());}catch(const std::runtime_error&){failed=true;}require(failed&&logic.result_block().flag_67f718==1,"null result has no partial publication");
}

} // namespace

int main() {
    try {
        test_live_results();
        test_result_input_boundary();
        test_complete_frame();
        test_win_chain_and_profile();
        test_fail_and_neutral_write_nothing();
        test_event_dispatch_chain();
        test_subgoal_fail_and_text();
        test_leave_runs_bit2_first();
        test_frame_counters_and_status();
        test_no_client_no_message();
        test_default_host_no_result();
        std::cout << "mission logic checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
