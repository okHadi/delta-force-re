// Frozen-vector tests for the game message bus port. Run directly:
//   clang++ -std=c++17 -Iinclude tests/native_game_messages.cpp src/game_messages.cpp
// Vectors follow the original Df.exe behavior verified by
// tools/check_game_messages_original.py.
#include "dfre/game_messages.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

namespace {

int failures = 0;

#define CHECK(condition)                                                                     \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                 \
            ++failures;                                                                      \
        }                                                                                    \
    } while (false)

std::string hex(const std::uint8_t* data, std::size_t size) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0xF]);
    }
    return out;
}

std::string record_hex(const dfre::GameMessageRecord& record) {
    std::vector<std::uint8_t> bytes;
    auto push16 = [&bytes](std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    };
    push16(record.sequence);
    push16(record.repeats);
    push16(record.length);
    push16(record.delay);
    bytes.push_back(record.id);
    bytes.push_back(record.active);
    bytes.insert(bytes.end(), record.payload.begin(), record.payload.end());
    return hex(bytes.data(), bytes.size());
}

// Metadata stubs matching tools/game_messages_probe.cpp and the replay checker.
class TestHost : public dfre::GameMessageHost {
public:
    std::vector<std::vector<std::uint8_t>> sent;
    std::vector<std::vector<std::uint8_t>> incoming;
    int weapons_ticks = 0;
    std::function<void()> on_tick,on_aim;

    std::int32_t lookup_4828c0(std::int32_t value) override { return (value * 7 + 1) & 0x3FF; }
    std::uint32_t actor_record_base() override { return 0x1500000; }
    std::int32_t allocate_44a0c0(dfre::GameRecordArray, std::int32_t) override {
        return 0x7F000000;
    }
    std::int32_t actor_record_dword(std::int32_t index, std::uint32_t offset) override {
        return static_cast<std::int32_t>(0x5A000000u | ((static_cast<std::uint32_t>(index) & 0xFFF) << 12) |
                                        ((offset >> 2) & 0xFFF));
    }
    std::uint16_t actor_record_word(std::int32_t index, std::uint32_t offset) override {
        return static_cast<std::uint16_t>((index * 3 + static_cast<std::int32_t>(offset)) & 0xFFFF);
    }
    std::int32_t lookup_42cce0(std::int32_t value) override { return (value ^ 0x1234) & 0xFFFF; }
    std::int32_t terrain_height_47df40(std::uint32_t x, std::uint32_t y) override {
        return static_cast<std::int32_t>(((x >> 8) * 7 + (y >> 8) * 13) & 0xFFFFFF);
    }
    std::function<void()> on_sincos;
    void sincos_477820(std::int32_t angle, std::int32_t& out_40, std::int32_t& out_3c) override {
        if(on_sincos)on_sincos();
        out_40 = (angle >> 8) * 2;
        out_3c = (angle >> 8) * 3;
    }
    std::int32_t weapon_list_46c180() override { return 0x00112233; }
    std::uint32_t session_counter_c2382c() override { return 0x55667788; }
    std::uint32_t weapon_aim_c23b70() override {if(on_aim)on_aim();return 0x10203040; }
    std::uint32_t weapon_aim_c23b74() override { return 0x50607080; }
    std::uint32_t weapon_table_field(std::uint32_t index, std::uint32_t offset,
                                     std::uint32_t width) override {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < width; ++i) {
            value |= static_cast<std::uint32_t>((index * 0x21 + (offset + i)) & 0xFF) << (8 * i);
        }
        return value;
    }
    std::uint32_t class_table_field(std::uint32_t index, std::uint32_t offset,
                                    std::uint32_t width) override {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < width; ++i) {
            value |= static_cast<std::uint32_t>((index * 0x31 + (offset + i) * 3) & 0xFF) << (8 * i);
        }
        return value;
    }
    std::uint32_t slot_map_word(std::uint32_t slot) override { return (slot * 0x111) & 0xFFFF; }
    std::int32_t record_field_110(dfre::GameRecordArray, std::int32_t) override { return 0x490000; }
    void send_local_frame(const std::uint8_t* frame, std::size_t length) override {
        sent.emplace_back(frame, frame + length);
    }
    std::size_t receive_local_frame(const std::uint8_t** frame) override {
        if (incoming.empty()) {
            return 0;
        }
        current = incoming.front();
        incoming.erase(incoming.begin());
        *frame = current.data();
        return current.size();
    }
    std::vector<std::uint8_t> current;
    void weapons_tick_482b00() override { ++weapons_ticks;if(on_tick)on_tick(); }
};

void test_table() {
    dfre::GameMessageTable table;  // 0x460DE0 sorted at construction
    CHECK(table.descriptor(0x00).length == 0 && table.descriptor(0x00).flags == 0);
    CHECK(table.descriptor(0x09).flags == 0x00 && table.descriptor(0x09).length == 0x0F);
    CHECK(table.descriptor(0x0E).flags == 0x00 && table.descriptor(0x0E).length == 0x12);
    CHECK(table.descriptor(0x22).flags == 0x02 && table.descriptor(0x22).length == 0x28);
    CHECK(table.descriptor(0x28).flags == 0x02 && table.descriptor(0x28).length == 0);
    CHECK(table.descriptor(0x2A).flags == 0x02 && table.descriptor(0x2A).length == 0x0F);
    CHECK(table.descriptor(0x2C).flags == 0x02 && table.descriptor(0x2C).length == 0);
    CHECK(table.descriptor(0x2F).flags == 0x00 && table.descriptor(0x2F).length == 4);
    CHECK(table.descriptor(0x12).flags == 0x03 && table.descriptor(0x12).length == 0);
    CHECK(table.descriptor(0x40).flags == 0x03 && table.descriptor(0x40).length == 0);
    CHECK(table.descriptor(0x4C).length == 9 && table.descriptor(0x4E).length == 3);
    CHECK(table.descriptor(0x50).flags == 0x00 && table.descriptor(0x50).length == 8);
    // reserved ids are holes after the sort
    CHECK(table.descriptor(0x1F).flags == 0 && table.descriptor(0x1F).length == 0);
    CHECK(table.descriptor(0x31).flags == 0 && table.descriptor(0x31).length == 0);
    CHECK(table.descriptor(0x4F).flags == 0x02 && table.descriptor(0x4F).length == 0x10);
    CHECK(dfre::GameMessageTable::dispatchable(0x09));
    CHECK(!dfre::GameMessageTable::dispatchable(0x28));
    // message 0x01 patches 32 descriptors at once
    std::vector<std::uint8_t> patch(0x80);
    for (std::size_t i = 0; i < 32; ++i) {
        patch[i * 4] = static_cast<std::uint8_t>(i);
        patch[i * 4 + 1] = 0x00;
        patch[i * 4 + 2] = 0x10;
        patch[i * 4 + 3] = 0x00;
    }
    table.patch(0, patch.data());
    CHECK(table.descriptor(0x09).flags == 0x00 && table.descriptor(0x09).length == 0x10);
}

void test_enqueue() {
    dfre::GameMessageTable table;
    dfre::GameMessageQueue queue;
    const std::uint8_t payload[2] = {0x34, 0x12};
    CHECK(queue.enqueue(table, 0x10, payload, 2));
    const dfre::GameMessageRecord& record = queue.record(0);
    CHECK(record.active == 1 && record.id == 0x10 && record.sequence == 0 && record.length == 2);
    CHECK(record.repeats == 0x3E9);  // flags bit 1 -> x1001
    CHECK(record.delay == 0);
    CHECK(record.payload[0] == 0x34 && record.payload[1] == 0x12);
    CHECK(queue.sequence_counter() == 1);
    CHECK(record_hex(record).substr(0, 20) == "0000e903020000001001");
    // id 0 is ignored
    CHECK(!queue.enqueue(table, 0x00, payload, 2));
    // repeats 1 and 45 for the other descriptor classes
    CHECK(queue.enqueue(table, 0x14, payload, 2) && queue.record(1).repeats == 1);
    CHECK(queue.enqueue(table, 0x0A, payload, 2) && queue.record(2).repeats == 1);
    CHECK(queue.enqueue(table, 0x05, payload, 2) == false || queue.record(3).repeats == 0x3E9);
    // sequence wrap at 32000
    queue.set_sequence_counter(32000);
    CHECK(queue.enqueue(table, 0x14, payload, 2));
    CHECK(queue.sequence_counter() == 1);
    CHECK(queue.record(4).sequence == 32000);
    // full queue drops the 65th message
    dfre::GameMessageQueue full;
    for (int i = 0; i < 64; ++i) {
        CHECK(full.enqueue(table, 0x0A, nullptr, 0));
    }
    CHECK(!full.enqueue(table, 0x0A, nullptr, 0));
    CHECK(full.active_count() == 64);
    // cancel by sequence clears matching records (message 0x14)
    CHECK(full.cancel_sequence(10) == 1);
    CHECK(full.active_count() == 63);
}

void test_worker_and_flush() {
    dfre::GameMessageTable table;
    dfre::GameMessageQueue queue;
    dfre::GameMessageFrame frame;
    std::vector<dfre::GameMessageSend> sends;
    std::uint8_t payload[0x0F];
    for (std::size_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = static_cast<std::uint8_t>(i);
    }
    CHECK(queue.enqueue(table, 0x28, nullptr, 0));
    CHECK(queue.enqueue(table, 0x2A, payload, 0x0F));
    queue.set_send_delay(2);  // 0x50321C delays the third message
    CHECK(queue.enqueue(table, 0x0A, payload, 0x0F));
    frame.begin();
    sends.clear();
    queue.step(frame, table, sends);
    // first tick: only the two undelayed messages go out
    CHECK(sends.size() == 2);
    CHECK(hex(frame.bytes().data(), frame.bytes().size()) ==
          "280000" "2a0100" "000102030405060708090a0b0c0d0e");
    CHECK(sends[0].id == 0x28 && sends[0].sequence == 0 && sends[0].length == 0);
    CHECK(sends[1].id == 0x2A && sends[1].sequence == 1 && sends[1].length == 0x0F);
    CHECK(sends[1].payload[0] == 0x00 && sends[1].payload[0x0E] == 0x0E);
    CHECK(queue.record(2).delay == 1);
    frame.begin();
    sends.clear();
    queue.step(frame, table, sends);
    CHECK(sends.empty());  // delay 1 -> 0, nothing sent
    CHECK(queue.record(2).delay == 0);
    frame.begin();
    sends.clear();
    queue.step(frame, table, sends);
    // retransmit tick: 0x28 and 0x2A send again (odd repeat counts), 0x0A sends once
    CHECK(sends.size() == 3);
    CHECK(sends[0].id == 0x28 && sends[1].id == 0x2A && sends[2].id == 0x0A);
    CHECK(queue.record(2).active == 0);
    CHECK(queue.active_count() == 2);
    queue.compact();
    CHECK(queue.record(0).id == 0x28 && queue.record(1).id == 0x2A);
}

void test_serialize() {
    dfre::GameMessageTable table;
    dfre::GameMessageFrame frame;
    // sequenced message inserts the sequence word
    frame.begin();
    CHECK(frame.append(table, 0x2A, 7, nullptr, 0x0F));
    CHECK(hex(frame.bytes().data(), frame.bytes().size()).substr(0, 6) == "2a0700");
    // self length message carries its own skip
    const std::uint8_t hud[6] = {6, 0, 1, 'T', 0, 0};
    frame.begin();
    CHECK(frame.append(table, 0x12, 8, hud, 6));
    CHECK(hex(frame.bytes().data(), frame.bytes().size()) == "120800" "060001540000");
    // fixed length mismatch records an error and still appends
    const std::uint8_t five[5] = {1, 2, 3, 4, 5};
    frame.begin();
    CHECK(!frame.append(table, 0x2A, 9, five, 5));
    CHECK(frame.errors().size() == 1);
    CHECK(frame.errors()[0].id == 0x2A && frame.errors()[0].expected == 0x0F);
    CHECK(frame.bytes().size() == 1 + 2 + 5);
    // capacity rule: cursor + length + 0x10 > 0x2000 drops the message
    frame.begin();
    std::vector<std::uint8_t> big(0x100, 0x5A);
    bool dropped = false;
    for (int i = 0; i < 0x20; ++i) {
        if (!frame.append(table, 0x3D, 0, big.data(), 0x100)) {
            dropped = true;
        }
    }
    CHECK(dropped && frame.overflowed());
}

void test_producers() {
    dfre::GameMessageTable table;
    dfre::GameMessageStage stage;
    TestHost host;
    stage.set_host(&host);stage.set_inspection_mode(true);
    stage.enqueue_shot();                                       // 470F40 -> 0x28
    stage.enqueue_detonate();                                   // 470F50 -> 0x2C
    stage.enqueue_placement(0x1234, 0x11223344, 0x55667788, 0x99AABBCC, 0xDD000000);  // 470F60
    CHECK(stage.queue().active_count() == 3);
    CHECK(stage.queue().record(0).id == 0x28 && stage.queue().record(0).length == 0);
    CHECK(stage.queue().record(1).id == 0x2C && stage.queue().record(1).length == 0);
    const dfre::GameMessageRecord& placement = stage.queue().record(2);
    CHECK(placement.id == 0x2A && placement.length == 0x0F);
    CHECK(hex(placement.payload.data(), 0x0F) == "34124433221188776655ccbbaa99dd");
    CHECK(placement.repeats == 0x3E9);
    stage.enqueue_weapon_trace();                               // 470FB0 -> 0x2F
    const dfre::GameMessageRecord& trace = stage.queue().record(3);
    CHECK(trace.id == 0x2F && trace.length == 4);
    CHECK(hex(trace.payload.data(), 4) == "20106050");
}

void test_dispatch() {
    dfre::GameMessageStage stage;
    TestHost host;
    stage.set_host(&host);stage.set_inspection_mode(true);
    stage.set_single_player(true);  // 0x10 / 0x14 run only in the single player path
    stage.set_networked(false);
    stage.set_local_player_index(3);
    // message 0x0A is an explicit no-op case (17 byte fixed length, no action)
    std::uint8_t frame_0a[0x11] = {0x0A};
    auto actions = stage.dispatch().dispatch(frame_0a, sizeof(frame_0a));
    CHECK(actions.empty());
    // sequenced dedup: second frame with the same sequence is a duplicate
    const std::uint8_t sequenced_0a[5] = {0x10, 0x34, 0x12, 0x03, 0x00};
    actions = stage.dispatch().dispatch(sequenced_0a, sizeof(sequenced_0a));
    CHECK(actions.size() == 1 && !actions[0].duplicate);
    CHECK(stage.dispatch().followups().size() == 1);  // 0x14 cancel queued
    CHECK(stage.dispatch().followups()[0].id == 0x14 && stage.dispatch().followups()[0].length == 2);
    CHECK(stage.dispatch().followups()[0].payload[0] == 0x34 &&
          stage.dispatch().followups()[0].payload[1] == 0x12);
    actions = stage.dispatch().dispatch(sequenced_0a, sizeof(sequenced_0a));
    CHECK(actions.size() == 1 && actions[0].duplicate);
    CHECK(stage.dispatch().dedup_ring()[0] == 0x1234);
    // message 0x14 cancels queue records with the payload sequence
    dfre::GameMessageStage cancel_stage;
    cancel_stage.set_host(&host);cancel_stage.set_inspection_mode(true);
    std::uint8_t two[2] = {0x05, 0x00};
    cancel_stage.queue().set_sequence_counter(5);
    CHECK(cancel_stage.queue().enqueue(cancel_stage.table(), 0x14, two, 2));
    const std::uint8_t cancel_frame[3] = {0x14, 0x05, 0x00};
    actions = cancel_stage.dispatch().dispatch(cancel_frame, sizeof(cancel_frame));
    CHECK(actions.size() == 1 && actions[0].kind == dfre::GameMessageActionKind::queue_cancel);
    CHECK(actions[0].args[0] == 1);
    CHECK(cancel_stage.queue().active_count() == 0);
    // message 0x12 HUD text with the mode colour
    dfre::GameMessageTextConfig config;
    config.color_67db4c = 0x22;
    stage.dispatch().set_text_config(config);
    std::uint8_t hud[9] = {0x12, 0x07, 0x00, 0x06, 0x00, 0x01, 'T', 'X', 0x00};
    actions = stage.dispatch().dispatch(hud, sizeof(hud));
    CHECK(actions.size() == 1 && actions[0].kind == dfre::GameMessageActionKind::hud_text);
    CHECK(actions[0].args[0] == 1 && actions[0].args[1] == 0x22);
    CHECK(std::string(actions[0].bytes.begin(), actions[0].bytes.end()) == std::string("TX\0", 3));
    // message 0x0F actor move on a vacant slot enqueues 0x17 with payload[1]
    std::uint8_t move[1 + 0x0E] = {0x0F};
    move[2] = 0x03;  // payload[1] = slot index
    actions = stage.dispatch().dispatch(move, sizeof(move));
    CHECK(actions.size() == 1);
    CHECK(actions[0].kind == dfre::GameMessageActionKind::actor_move);
    CHECK(stage.dispatch().followups().back().id == 0x17);
    CHECK(stage.dispatch().followups().back().payload[0] == 0x03);
    // transport-only ids fail closed (sequence 9 is not in the ring)
    const std::uint8_t unknown[3] = {0x28, 0x09, 0x00};
    actions = stage.dispatch().dispatch(unknown, sizeof(unknown));
    CHECK(actions.size() == 1 &&
          actions[0].kind == dfre::GameMessageActionKind::undispatchable);
}

void test_update() {
    dfre::GameMessageStage stage;
    TestHost host;
    stage.set_host(&host);stage.set_inspection_mode(true);
    stage.set_single_player(false);   // [0x67F520] != 0
    stage.set_networked(false);       // [0x67F528] == 0 -> player update path
    stage.set_tick_limit(2);          // [0x67F538]
    dfre::GameMessageUpdateInput input;
    input.local_present = true;
    input.x = 0x1000;
    input.y = 0x2000;
    input.z = 0x3000;
    input.pitch = 0x7F000000;
    input.yaw = 0x81000000;
    input.field_7c = 0x12;
    input.field_94 = 0x34;
    input.current_weapon_item = 0x1B;
    input.flag_67dbc0 = true;
    auto actions = stage.update(input);
    CHECK(actions.empty());
    CHECK(host.weapons_ticks == 1);
    CHECK(stage.tick_counter() == 1);
    CHECK(stage.queue().active_count() == 0);  // early out on the first tick
    actions = stage.update(input);
    CHECK(stage.tick_counter() == 0);
    CHECK(stage.queue().active_count() == 0);  // flushed in the same update
    CHECK(stage.player_update_counter() == 1);
    CHECK(host.sent.size() == 1);
    const std::vector<std::uint8_t>& frame = host.sent[0];
    // 0x0A (16 payload bytes) + 0x2F (4 payload bytes) + 0x32 (3 byte message)
    CHECK(frame.size() == 1 + 0x10 + 1 + 4 + 1);
    CHECK(frame[0] == 0x0A && frame[17] == 0x2F && frame[22] == 0x32);
    const std::uint32_t terrain =
        static_cast<std::uint32_t>(host.terrain_height_47df40(0x1000, 0x2000));
    const std::uint32_t dz = 0x3000 - terrain;
    CHECK(frame[1] == static_cast<std::uint8_t>((dz >> 16) & 0xFF));
    CHECK(frame[5] == static_cast<std::uint8_t>((dz >> 8) & 0xFF));
    CHECK(frame[9] == 0x7F && frame[10] == 0x81);
    CHECK(frame[11] == 0x12 && frame[12] == 0x34);
    CHECK(frame[18] == 0x20 && frame[19] == 0x10 && frame[20] == 0x60 && frame[21] == 0x50);
    // single player mode skips the player update and sends through the host
    dfre::GameMessageStage sp;
    sp.set_host(&host);sp.set_inspection_mode(true);
    sp.set_single_player(true);
    sp.set_networked(false);
    host.sent.clear();
    sp.enqueue_shot();  // 0x28 is sequenced and keeps retransmitting
    actions = sp.update(input);
    CHECK(sp.queue().active_count() == 1);
    CHECK(host.sent.size() == 1);
    CHECK(hex(host.sent[0].data(), host.sent[0].size()) == "280000" "2f20106050" "32");
    CHECK(sp.player_update_counter() == 0);
    // network mode appends the frame to the packet buffer instead
    dfre::GameMessageStage mp;
    mp.set_host(&host);mp.set_inspection_mode(true);
    mp.set_single_player(true);
    mp.set_networked(true);
    mp.enqueue_detonate();
    actions = mp.update(input);
    CHECK(hex(mp.network_packet().data(), mp.network_packet().size()) == "2c0000" "2f20106050");
}

void test_receive_pump() {
    dfre::GameMessageStage stage;
    TestHost host;
    stage.set_host(&host);stage.set_inspection_mode(true);
    stage.set_single_player(false);
    stage.set_networked(false);
    const std::uint8_t frame[0x11] = {0x0A};
    host.incoming.emplace_back(frame, frame + sizeof(frame));
    auto actions = stage.pump_receive();
    CHECK(actions.empty());  // 0x0A is an explicit no-op
    CHECK(host.incoming.empty());
    CHECK(stage.update_counter() == 0);  // reset per dispatched frame
}
void test_live_boundaries(){
    for(unsigned flags=0;flags<4;++flags){
        dfre::GameMessageStage stage;TestHost host;stage.set_host(&host);
        std::array<std::uint16_t,256> ring{};std::uint32_t bytes=0,idle=99;
        dfre::GameMessageLiveBindings b;b.dedup=&ring;b.received_bytes_bcfc24=&bytes;b.update_counter_bc5d20=&idle;
        b.read_global=[&](auto address){if(address==0x67f520)return flags&1;CHECK(address==0x67f528);return (flags>>1)&1;};
        stage.set_live_bindings(b);std::vector<std::uint8_t> frame(17);frame[0]=0x0a;
        stage.submit_local_echo(frame.data(),frame.size());host.incoming={frame};
        CHECK(stage.check_receive());CHECK(stage.dispatch_received().empty());CHECK(bytes==17 && idle==99);
        if(flags==1){CHECK(host.incoming.empty() && stage.pending_local_echo().size()==17);}
        else{CHECK(host.incoming.size()==1 && stage.pending_local_echo().empty());}
        CHECK(!stage.check_receive());
    }
    dfre::GameMessageStage stage;std::array<std::uint16_t,256> ring{};
    std::uint32_t bytes=0xfffffff0,idle=99;std::map<std::uint32_t,std::uint32_t> globals{{0x67f520,0},{0x67f528,0}};
    dfre::GameMessageLiveBindings b;b.dedup=&ring;b.received_bytes_bcfc24=&bytes;b.update_counter_bc5d20=&idle;
    b.read_global=[&](auto address){return globals.at(address);};b.write_global=[&](auto address,auto value){globals[address]=value;};
    unsigned calls=0;std::uint32_t current_callback=0;
    b.record_dword=[&](auto array,auto index,auto offset){CHECK(array==dfre::GameRecordArray::players_7cca80 && index==2 && offset==0x110);CHECK(globals[0xa158c0]==7);return current_callback;};
    b.call=[&](const dfre::GameMessageLiveCall& call){++calls;
        if(call.address==0x455af0){CHECK(call.args[0]==11);call.payload[8]=77;current_callback=0x12345678;}
        else if(call.address==0x455b30){CHECK(call.args[0]==77);}
        else if(call.address==0x455b10){CHECK(call.args[0]==33);}
        else{CHECK(call.address==0x12345678 && call.args[0]==4 && call.index==2);}
    };
    stage.set_live_bindings(b);
    std::vector<std::uint8_t> frame={0x23,1,0, 1,0,0,0, 11,0,0,0, 22,0,0,0, 33,0,0,0, 0,0,0,0,
        0x39,2,0, 2,0,7,0};
    stage.submit_local_echo(frame.data(),frame.size());stage.pump_receive();
    CHECK(calls==4 && globals[0xbcfc14]==1 && ring[0]==2 && ring[1]==1 && idle==0);
    CHECK(bytes==std::uint32_t(0xfffffff0u+frame.size()) && stage.queue().active_count()==2);
    // A duplicate still queues cancellation and executes no body.
    stage.submit_local_echo(frame.data(),frame.size());stage.pump_receive();CHECK(calls==4 && stage.queue().active_count()==4);
    // A live09 body runs before the next message and can change current state.
    b.projectile_09=[&](auto* packet,auto length){CHECK(length==15 && packet[14]==9);globals[0x67f528]=1;};
    stage.set_live_bindings(b);std::vector<std::uint8_t> shot(16);shot[0]=9;shot[15]=9;
    shot.insert(shot.end(),{0x39,3,0,2,0,7,0});stage.submit_local_echo(shot.data(),shot.size());stage.pump_receive();CHECK(calls==4);
    // Echo appended by a reached effect survives the current dispatch.
    globals[0x67f528]=0;b.call=[&](const auto& call){CHECK(call.address==0x470400);std::uint8_t next[17]={0x0a};stage.submit_local_echo(next,sizeof(next));};
    stage.set_live_bindings(b);const std::uint8_t command[]={0x20,4,0,1,2,3};stage.submit_local_echo(command,sizeof(command));stage.pump_receive();
    CHECK(stage.pending_local_echo().empty());
    b.call=[&](const auto&){globals[123]=456;throw std::runtime_error("effect stopped");};stage.set_live_bindings(b);
    const std::uint8_t error[]={0x20,5,0,1,2,3};stage.submit_local_echo(error,sizeof(error));
    bool failed=false;try{stage.pump_receive();}catch(const std::runtime_error&){failed=true;}
    CHECK(failed && globals[123]==456 && ring[0]==5);
    b.call={};stage.set_live_bindings(b);const std::uint8_t missing[]={0x20,6,0,1,2,3};stage.submit_local_echo(missing,sizeof(missing));
    failed=false;try{stage.pump_receive();}catch(const std::runtime_error&){failed=true;}CHECK(failed);
    dfre::GameMessageHost missing_host;failed=false;
    try{missing_host.terrain_height_47df40(0,0);}catch(const std::runtime_error&){failed=true;}CHECK(failed);
    // 46F5C0 reads current weapon state after482B00 and menu state after470FB0.
    dfre::GameMessageStage update;TestHost changing;dfre::GameMessageUpdateInput current;
    changing.on_tick=[&]{current.current_weapon_item=0x1b;};changing.on_aim=[&]{current.flag_67dbc0=true;};
    update.set_host(&changing);b.current_update=[&]{return current;};b.read_global=[](auto){return 0u;};
    update.set_live_bindings(b);idle=0xffffffff;update.update({});
    CHECK(changing.weapons_ticks==1 && idle==0 && changing.sent.size()==1);
    CHECK(hex(changing.sent[0].data(),changing.sent[0].size())=="2f2010605032");
}


void test_live_replication(){
    using dfre::GameRecordArray;
    dfre::GameMessageStage stage;TestHost host;stage.set_host(&host);
    std::array<std::uint8_t,0x114> record{};std::array<std::uint16_t,256> ring{};
    std::uint32_t received=0,idle=0;std::vector<std::uint32_t> writes;
    const auto store=[&](unsigned at,unsigned width,std::uint32_t value){for(unsigned i=0;i<width;++i)record[at+i]=std::uint8_t(value>>(8*i));};
    const auto read=[&](unsigned at){std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(record[at+i])<<(8*i);return value;};
    dfre::GameMessageLiveBindings b;b.dedup=&ring;b.received_bytes_bcfc24=&received;b.update_counter_bc5d20=&idle;
    b.read_global=[](auto){return 0u;};
    b.record_dword=[&](auto,auto index,auto offset){CHECK(index==3);return read(offset);};
    b.record_write=[&](auto,auto index,auto offset,auto width,auto value){CHECK(index==3);writes.push_back(offset*16+width);store(offset,width,value);};
    stage.set_live_bindings(b);
    const auto deliver=[&](std::uint8_t id,std::vector<std::uint8_t> payload){
        std::vector<std::uint8_t> frame{id};if(stage.table().descriptor(id).flags&6){frame.push_back(7);frame.push_back(0);ring.fill(0);}
        frame.insert(frame.end(),payload.begin(),payload.end());stage.submit_local_echo(frame.data(),frame.size());stage.pump_receive();
    };
    // Player index masks bit15. The low flags byte is cleared before the dword OR.
    store(0,4,1);store(0x6c,4,0xa1b2c3ff);
    std::vector<std::uint8_t> player(18);player[2]=3;player[3]=0x80;player[16]=5;player[17]=9;
    deliver(0x0e,player);CHECK(read(0x6c)==0xa1b2c305 && read(0x7c)==9);
    CHECK(writes[0]==0x6c1 && writes[1]==0x6c4);
    // A vacant player queues the original unmasked row value and needs no writer.
    store(0,4,0);b.record_write={};stage.set_live_bindings(b);deliver(0x0e,player);
    CHECK(stage.queue().record(stage.queue().last_slot()).id==0x17);
    CHECK(stage.queue().record(stage.queue().last_slot()).payload[1]==0x80);
    // 36 reads scale from the current record after the required trig callback.
    store(0,4,1);b.record_write=[&](auto,auto,auto at,auto width,auto value){store(at,width,value);};stage.set_live_bindings(b);
    host.on_sincos=[&]{store(0x7c,4,0x10000000);};
    std::vector<std::uint8_t> actor(28);actor[0]=3;actor[14]=1;actor[15]=2;
    deliver(0x36,actor);CHECK(read(0x3c)==196608 && read(0x40)==131072);
    // Missing reached writes fail before the record changes.
    b.record_write={};stage.set_live_bindings(b);store(0x6c,4,0x12345678);bool failed=false;
    try{deliver(0x29,std::vector<std::uint8_t>{3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});}catch(const std::runtime_error&){failed=true;}
    CHECK(failed && read(0x6c)==0x12345678);
    dfre::GameMessageHost missing_trig;stage.set_host(&missing_trig);
    b.record_write=[&](auto,auto,auto at,auto width,auto value){store(at,width,value);};stage.set_live_bindings(b);
    store(0xec,4,0xffffffff);failed=false;
    try{deliver(0x36,actor);}catch(const std::runtime_error&){failed=true;}
    CHECK(failed && read(0xec)==0);
}

}  // namespace

int main() {
    test_table();
    test_enqueue();
    test_worker_and_flush();
    test_serialize();
    test_producers();
    test_dispatch();
    test_update();
    test_live_boundaries();
    test_live_replication();
    test_receive_pump();
    if (failures != 0) {
        std::printf("%d checks failed\n", failures);
        return 1;
    }
    std::printf("all native game message checks passed\n");
    return 0;
}
