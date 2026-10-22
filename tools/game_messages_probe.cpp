// Probe for the game message bus port. Reads scenario commands on stdin and
// prints one JSON object per command for tools/check_game_messages_original.py.
//
// Commands:
//   table                              sorted 0x4F8D48 runtime table
//   reset                              fresh stage, default text config
//   setsingle <0|1>  setnet <0|1>      [0x67F520] / [0x67F528]
//   settick <n>                        [0x67F538]
//   setdelay <u16>                     [0x50321C]
//   setlocal <index>  setflag53c <n>   local player index / [0x67F53C]
//   settext <a> <b> <c> <d> <e> <f>    0x67DB40/48/4C/50/54/5C
//   enqueue <id> <payload_hex>         0x46F3E0
//   worker <ticks>                     0x46F4B0 per tick
//   serialize <id> <seq> <payload_hex> 0x4610E0
//   flush                              4611C0 step (uses flush_single/sp)
//   dispatch <frame_hex>               0x46D9F0
//   recv <frame_hex>                   queue a frame for receive_local_frame
//   pump                               drain receive, 0x461050 loop
//   update <x> <y> <z> <pitch> <yaw> <f7c> <f94> <item> <flag> 0x46F5C0
//   produce shot|detonate|trace
//   placement <a> <b> <c> <d> <e>      0x470F60
#include "dfre/game_messages.hpp"
#include <map>

#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using dfre::GameMessageAction;
using dfre::GameMessageHost;
using dfre::GameMessageStage;
using dfre::GameRecordArray;

std::vector<std::uint8_t> from_hex(const std::string& text) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < text.size() + 1 && i + 2 <= text.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::string to_hex(const std::uint8_t* data, std::size_t size) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0xF]);
    }
    return out;
}

std::string record_hex(const dfre::GameMessageRecord& record) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(dfre::kGameMessageRecordSize);
    auto push16 = [&bytes](std::uint16_t v) {
        bytes.push_back(static_cast<std::uint8_t>(v & 0xFF));
        bytes.push_back(static_cast<std::uint8_t>(v >> 8));
    };
    push16(record.sequence);
    push16(record.repeats);
    push16(record.length);
    push16(record.delay);
    bytes.push_back(record.id);
    bytes.push_back(record.active);
    bytes.insert(bytes.end(), record.payload.begin(), record.payload.end());
    return to_hex(bytes.data(), bytes.size());
}

const char* array_name(GameRecordArray array) {
    switch (array) {
    case GameRecordArray::players_7cca80: return "players_7cca80";
    case GameRecordArray::actors_7dde80: return "actors_7dde80";
    case GameRecordArray::squad_867e80: return "squad_867e80";
    case GameRecordArray::effects_8ace80: return "effects_8ace80";
    default: return "none";
    }
}

// Deterministic metadata stubs. The replay checker implements the same
// formulas for the original x86 hooked functions.
class ProbeHost : public GameMessageHost {
public:
    std::deque<std::vector<std::uint8_t>> incoming;
    std::function<void()> on_sincos;
    std::vector<std::uint8_t> sent;
    std::uint32_t name_calls = 0;
    std::uint32_t weapons_ticks = 0;

    std::int32_t lookup_4828c0(std::int32_t value) override {
        return (value * 7 + 1) & 0x3FF;
    }
    std::uint32_t actor_record_base() override { return 0x1500000; }
    std::int32_t allocate_44a0c0(GameRecordArray, std::int32_t) override {
        const std::int32_t pointer = static_cast<std::int32_t>(0x7F000000 + name_calls * 0x100);
        ++name_calls;
        return pointer;
    }
    std::int32_t actor_record_dword(std::int32_t index, std::uint32_t offset) override {
        return static_cast<std::int32_t>(0x5A000000u | ((static_cast<std::uint32_t>(index) & 0xFFF) << 12) |
                                        ((offset >> 2) & 0xFFF));
    }
    std::uint16_t actor_record_word(std::int32_t index, std::uint32_t offset) override {
        return static_cast<std::uint16_t>((index * 3 + static_cast<std::int32_t>(offset)) & 0xFFFF);
    }
    std::int32_t lookup_42cce0(std::int32_t value) override {
        return (value ^ 0x1234) & 0xFFFF;
    }
    std::int32_t terrain_height_47df40(std::uint32_t x, std::uint32_t y) override {
        return static_cast<std::int32_t>(((x >> 8) * 7 + (y >> 8) * 13) & 0xFFFFFF);
    }
    void sincos_477820(std::int32_t angle, std::int32_t& out_40, std::int32_t& out_3c) override {
        if(on_sincos)on_sincos();
        out_40 = (angle >> 8) * 2;
        out_3c = (angle >> 8) * 3;
    }
    std::int32_t weapon_list_46c180() override { return 0x00112233; }
    std::uint32_t session_counter_c2382c() override { return 0x55667788; }
    std::uint32_t current_weapon_item_c239b8_c239c0() override { return 0; }
    std::uint32_t weapon_aim_c23b70() override { return 0x10203040; }
    std::uint32_t weapon_aim_c23b74() override { return 0x50607080; }
    // Byte-wise tables, so both the probe and the replay harness agree on
    // every width: byte i = (index * step + i * scale) & 0xFF.
    std::uint32_t table_field(std::uint32_t index, std::uint32_t offset, std::uint32_t width,
                              std::uint32_t step, std::uint32_t scale) {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < width; ++i) {
            value |= static_cast<std::uint32_t>((index * step + (offset + i) * scale) & 0xFF)
                     << (8 * i);
        }
        return value;
    }
    std::uint32_t weapon_table_field(std::uint32_t index, std::uint32_t offset,
                                     std::uint32_t width) override {
        return table_field(index, offset, width, 0x21, 1);
    }
    std::uint32_t class_table_field(std::uint32_t index, std::uint32_t offset,
                                    std::uint32_t width) override {
        return table_field(index, offset, width, 0x31, 3);
    }
    std::uint32_t slot_map_word(std::uint32_t slot) override { return (slot * 0x111) & 0xFFFF; }
    std::int32_t record_field_110(GameRecordArray array, std::int32_t index) override {
        if (array == GameRecordArray::none) {
            return 0;
        }
        if (array == GameRecordArray::players_7cca80) {
            // mirrors the seeded record + 0x110 pointer (0 when vacant)
            if (dispatch == nullptr ||
                index >= static_cast<std::int32_t>(dispatch->player_slots()->size())) {
                return 0;
            }
            return (*dispatch->player_slots())[static_cast<std::size_t>(index)]
                       ? static_cast<std::int32_t>(0x490000)
                       : 0;
        }
        return static_cast<std::int32_t>(0x490000);
    }
    dfre::GameMessageDispatch* dispatch = nullptr;
    void send_local_frame(const std::uint8_t* frame, std::size_t length) override {
        sent.assign(frame, frame + length);
    }
    std::size_t receive_local_frame(const std::uint8_t** frame) override {
        if (incoming.empty()) {
            return 0;
        }
        static std::vector<std::uint8_t> current;
        current = incoming.front();
        incoming.pop_front();
        *frame = current.data();
        return current.size();
    }
    void weapons_tick_482b00() override { ++weapons_ticks; }
};

const char* kind_name(dfre::GameMessageActionKind kind) {
    switch (kind) {
    case dfre::GameMessageActionKind::descriptor_patch: return "descriptor_patch";
    case dfre::GameMessageActionKind::client_state: return "client_state";
    case dfre::GameMessageActionKind::player_create: return "player_create";
    case dfre::GameMessageActionKind::actor_create: return "actor_create";
    case dfre::GameMessageActionKind::projectile_spawn: return "projectile_spawn";
    case dfre::GameMessageActionKind::world_command: return "world_command";
    case dfre::GameMessageActionKind::actor_batch_create: return "actor_batch_create";
    case dfre::GameMessageActionKind::flag_d: return "flag_d";
    case dfre::GameMessageActionKind::player_move: return "player_move";
    case dfre::GameMessageActionKind::actor_move: return "actor_move";
    case dfre::GameMessageActionKind::player_reset: return "player_reset";
    case dfre::GameMessageActionKind::player_weapon_flag: return "player_weapon_flag";
    case dfre::GameMessageActionKind::hud_text: return "hud_text";
    case dfre::GameMessageActionKind::local_respawn: return "local_respawn";
    case dfre::GameMessageActionKind::queue_cancel: return "queue_cancel";
    case dfre::GameMessageActionKind::scoreboard: return "scoreboard";
    case dfre::GameMessageActionKind::player_replace: return "player_replace";
    case dfre::GameMessageActionKind::player_state: return "player_state";
    case dfre::GameMessageActionKind::flag_19: return "flag_19";
    case dfre::GameMessageActionKind::set_send_delay: return "set_send_delay";
    case dfre::GameMessageActionKind::set_value_67f548: return "set_value_67f548";
    case dfre::GameMessageActionKind::local_death: return "local_death";
    case dfre::GameMessageActionKind::mission_state: return "mission_state";
    case dfre::GameMessageActionKind::world_1e: return "world_1e";
    case dfre::GameMessageActionKind::world_20: return "world_20";
    case dfre::GameMessageActionKind::control_flags: return "control_flags";
    case dfre::GameMessageActionKind::set_net_flag: return "set_net_flag";
    case dfre::GameMessageActionKind::entity_action: return "entity_action";
    case dfre::GameMessageActionKind::effect_batch_create: return "effect_batch_create";
    case dfre::GameMessageActionKind::hud_mode: return "hud_mode";
    case dfre::GameMessageActionKind::actor_state: return "actor_state";
    case dfre::GameMessageActionKind::actor_place: return "actor_place";
    case dfre::GameMessageActionKind::mission_ok: return "mission_ok";
    case dfre::GameMessageActionKind::mission_denied: return "mission_denied";
    case dfre::GameMessageActionKind::set_byte_c23b78: return "set_byte_c23b78";
    case dfre::GameMessageActionKind::set_flag_67dbbc: return "set_flag_67dbbc";
    case dfre::GameMessageActionKind::actor_update_34: return "actor_update_34";
    case dfre::GameMessageActionKind::actor_update_35: return "actor_update_35";
    case dfre::GameMessageActionKind::actor_update_36: return "actor_update_36";
    case dfre::GameMessageActionKind::actor_update_37: return "actor_update_37";
    case dfre::GameMessageActionKind::actor_update_38: return "actor_update_38";
    case dfre::GameMessageActionKind::entity_callback: return "entity_callback";
    case dfre::GameMessageActionKind::sound_pair: return "sound_pair";
    case dfre::GameMessageActionKind::actor_state_dcb: return "actor_state_dcb";
    case dfre::GameMessageActionKind::local_text: return "local_text";
    case dfre::GameMessageActionKind::copy_blob: return "copy_blob";
    case dfre::GameMessageActionKind::input_ints: return "input_ints";
    case dfre::GameMessageActionKind::input_blob: return "input_blob";
    case dfre::GameMessageActionKind::input_none: return "input_none";
    case dfre::GameMessageActionKind::session_strings: return "session_strings";
    case dfre::GameMessageActionKind::input_43: return "input_43";
    case dfre::GameMessageActionKind::hud_44: return "hud_44";
    case dfre::GameMessageActionKind::input_46: return "input_46";
    case dfre::GameMessageActionKind::hud_47: return "hud_47";
    case dfre::GameMessageActionKind::input_48: return "input_48";
    case dfre::GameMessageActionKind::weapon_list_request: return "weapon_list_request";
    case dfre::GameMessageActionKind::slot_report: return "slot_report";
    case dfre::GameMessageActionKind::class_report: return "class_report";
    case dfre::GameMessageActionKind::notify_45d6f0: return "notify_45d6f0";
    case dfre::GameMessageActionKind::reset_45da30: return "reset_45da30";
    case dfre::GameMessageActionKind::mbp_error: return "mbp_error";
    case dfre::GameMessageActionKind::unknown: return "unknown";
    case dfre::GameMessageActionKind::undispatchable: return "undispatchable";
    }
    return "?";
}

std::string action_json(const GameMessageAction& action) {
    std::ostringstream out;
    out << "{\"kind\":\"" << kind_name(action.kind) << "\",\"id\":" << static_cast<int>(action.id)
        << ",\"seq\":" << action.sequence << ",\"dup\":" << (action.duplicate ? 1 : 0)
        << ",\"array\":\"" << array_name(action.array) << "\",\"index\":" << action.index
        << ",\"address\":" << action.address << ",\"args\":[";
    for (std::size_t i = 0; i < action.args.size(); ++i) {
        out << action.args[i] << (i + 1 < action.args.size() ? "," : "");
    }
    out << "],\"writes\":[";
    for (std::size_t i = 0; i < action.writes.size(); ++i) {
        const dfre::GameRecordWrite& write = action.writes[i];
        out << "[" << write.offset << "," << static_cast<int>(write.width) << "," << write.value
            << "]" << (i + 1 < action.writes.size() ? "," : "");
    }
    out << "],\"bytes\":\"" << to_hex(action.bytes.data(), action.bytes.size()) << "\"}";
    return out.str();
}

std::string records_json(GameMessageStage& stage) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (std::size_t slot = 0; slot < dfre::kGameMessageQueueSlots; ++slot) {
        const dfre::GameMessageRecord& record = stage.queue().record(slot);
        if (record.active == 0) {
            continue;
        }
        if (!first) {
            out << ",";
        }
        first = false;
        out << "[" << slot << ",\"" << record_hex(record) << "\"]";
    }
    out << "]";
    return out.str();
}

std::string actions_json(const std::vector<GameMessageAction>& actions) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < actions.size(); ++i) {
        out << (i ? "," : "") << action_json(actions[i]);
    }
    out << "]";
    return out.str();
}

std::string state_json(GameMessageStage& stage) {
    const dfre::GameMessageDispatch& dispatch = stage.dispatch();
    std::ostringstream out;
    out << "{\"ring\":[";
    for (std::size_t i = 0; i < dfre::kGameMessageDedupSlots; ++i) {
        out << dispatch.dedup_ring()[i] << (i + 1 < dfre::kGameMessageDedupSlots ? "," : "");
    }
    out << "],\"flag14\":" << (dispatch.flag_bc5d14() ? 1 : 0)
        << ",\"flag18\":" << (dispatch.flag_bc5d18() ? 1 : 0)
        << ",\"delay\":" << dispatch.delay_50321c() << ",\"value548\":" << dispatch.value_67f548()
        << ",\"c2c\":" << dispatch.death_bcfc2c() << ",\"c30\":" << dispatch.death_bcfc30()
        << ",\"c34\":" << dispatch.death_bcfc34() << ",\"ctl\":" << dispatch.control_bcfc14()
        << ",\"dbe8\":" << dispatch.net_flag_67dbe8() << ",\"b78\":" << dispatch.byte_c23b78()
        << ",\"dbbc\":" << dispatch.flag_67dbbc()
        << ",\"a158c0\":" << dispatch.entity_callback_a158c0()
        << ",\"fa18\":" << dispatch.max_players_8cfa18()
        << ",\"fa1c\":" << dispatch.max_actors_8cfa1c()
        << ",\"f904\":" << dispatch.max_squad_8cf904()
        << ",\"f900\":" << dispatch.max_effects_8cf900()
        << ",\"s624\":\""
        << to_hex(reinterpret_cast<const std::uint8_t*>(dispatch.session_string_bcf624().data()),
                  dispatch.session_string_bcf624().size())
        << "\",\"f438\":\""
        << to_hex(reinterpret_cast<const std::uint8_t*>(dispatch.mission_file_67f438().data()),
                  dispatch.mission_file_67f438().size())
        << "\",\"blob\":\"" << to_hex(dispatch.blob_50352c().data(), dispatch.blob_50352c().size())
        << "\",\"sb_count\":" << dispatch.scoreboard_count()
        << ",\"sb_e4\":" << dispatch.scoreboard_field_bcfbe4() << ",\"sb_names\":[";
    for (std::size_t i = 0; i < dispatch.scoreboard_names().size(); ++i) {
        out << (i ? "," : "") << "\""
            << to_hex(reinterpret_cast<const std::uint8_t*>(dispatch.scoreboard_names()[i].c_str()),
                      dispatch.scoreboard_names()[i].size() + 1)
            << "\"";
    }
    out << "],\"sb_values\":[";
    for (std::size_t i = 0; i < dispatch.scoreboard_values().size(); ++i) {
        out << (i ? "," : "") << dispatch.scoreboard_values()[i];
    }
    out << "],\"sb_a\":[";
    for (std::size_t i = 0; i < 3; ++i) {
        out << (i ? "," : "") << dispatch.scoreboard_pair_a()[i];
    }
    out << "],\"sb_b\":[";
    for (std::size_t i = 0; i < 3; ++i) {
        out << (i ? "," : "") << dispatch.scoreboard_pair_b()[i];
    }
    out << "],\"sb_c\":[";
    for (std::size_t i = 0; i < 3; ++i) {
        out << (i ? "," : "") << dispatch.scoreboard_pair_c()[i];
    }
    out << "],\"net\":\"" << to_hex(stage.network_packet().data(), stage.network_packet().size())
        << "\",\"echo\":\"" << to_hex(stage.pending_local_echo().data(), stage.pending_local_echo().size())
        << "\",\"update_counter\":" << stage.update_counter()
        << ",\"tick_counter\":" << stage.tick_counter()
        << ",\"player_counter\":" << stage.player_update_counter() << "}";
    return out.str();
}

}  // namespace

int main() {
    GameMessageStage stage;
    ProbeHost host;
    stage.set_host(&host);stage.set_inspection_mode(true);
    host.dispatch = &stage.dispatch();
    stage.set_single_player(true);  // single player paths on, like a mission

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if(command=="receive_flags"){
            unsigned flags;input>>flags;GameMessageStage received_stage;ProbeHost received_host;
            received_stage.set_host(&received_host);received_stage.set_inspection_mode(true);
            received_stage.set_single_player((flags&1)==0);received_stage.set_networked((flags&2)!=0);
            std::vector<std::uint8_t> bytes(17);bytes[0]=0x0a;received_host.incoming.push_back(bytes);
            received_stage.submit_local_echo(bytes.data(),bytes.size());const auto got=received_stage.check_receive();
            std::cout<<"{\"op\":\"receive_flags\",\"state\":["<<got<<','<<received_host.incoming.size()<<','<<received_stage.pending_local_echo().size()<<"]}\n";
        }else if(command=="live"){
            std::string hex_frame;std::uint32_t gate,mutation;input>>hex_frame>>gate>>mutation;
            auto frame=from_hex(hex_frame);GameMessageStage live_stage;
            std::array<std::uint16_t,256> ring{};std::uint32_t received=0,idle=99;
            std::map<std::uint32_t,std::uint32_t> globals{{0x67f520,0},{0x67f528,gate}};
            const std::vector<std::uint32_t> addresses={0xbc5d14,0xbc5d18,0x50321c,0x67f548,0xbcfc14,0xc23b78,0x67dbbc,0xa158c0};
            for(auto address:addresses)globals[address]=0;
            std::vector<std::vector<std::uint32_t>> calls;
            dfre::GameMessageLiveBindings binding;binding.dedup=&ring;binding.received_bytes_bcfc24=&received;binding.update_counter_bc5d20=&idle;
            binding.read_global=[&](auto address){return globals.at(address);};binding.write_global=[&](auto address,auto value){globals[address]=value;};
            std::array<std::uint8_t,0x114> player{},actor{};
            std::vector<std::vector<std::uint32_t>> record_writes;
            const auto set32=[](auto& bytes,unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=std::uint8_t(value>>(8*i));};
            const bool replication=frame[0]==0x0e||frame[0]==0x29||(frame[0]>=0x34&&frame[0]<=0x38)||frame[0]==0x3b;
            for(auto* bytes:{&player,&actor}){set32(*bytes,0,(gate||!replication)?0x04030204:0);set32(*bytes,0x110,0x490000);set32(*bytes,0x6c,0xa1b2c3d4);}
            binding.record_dword=[&](auto array,auto index,auto offset){
                if(index!=3)throw std::runtime_error("live record index");
                auto& bytes=array==GameRecordArray::players_7cca80?player:actor;
                std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[offset+i])<<(8*i);return value;
            };
            binding.record_write=[&](auto array,auto index,auto offset,auto width,auto value){
                if(index!=3)throw std::runtime_error("live record index");
                auto& bytes=array==GameRecordArray::players_7cca80?player:actor;
                record_writes.push_back({array==GameRecordArray::players_7cca80?0x7cca80u:0x7dde80u,std::uint32_t(index),offset,width,value});
                for(unsigned i=0;i<width;++i)bytes[offset+i]=std::uint8_t(value>>(8*i));
            };
            ProbeHost live_host;live_host.on_sincos=[&]{if(mutation)set32(actor,0x7c,0xf1234567);};live_stage.set_host(&live_host);
            binding.call=[&](const dfre::GameMessageLiveCall& call){std::vector<std::uint32_t> args{call.address};
                if(call.array==dfre::GameRecordArray::players_7cca80)args.push_back(0x7cca80u+std::uint32_t(call.index)*0x114u);
                args.insert(args.end(),call.args.begin(),call.args.begin()+call.count);calls.push_back(args);
                if(mutation && call.address==0x455af0){call.payload[8]=77;call.payload[9]=call.payload[10]=call.payload[11]=0;}
                if(mutation && call.address==0x45dc70){call.payload[14]=55;call.payload[15]=0;}
            };
            binding.projectile_09=[&](const auto* packet,auto length){
                if(length!=15)throw std::runtime_error("live09 payload size");
                const auto read=[&](unsigned at,unsigned size){std::uint32_t value=0;for(unsigned i=0;i<size;++i)value|=std::uint32_t(packet[at+i])<<(8*i);return value;};
                const auto x=read(0,4)&0xffffff00u,y=read(4,4)&0xffffff00u;
                auto fraction=(read(0,1)<<16)|(read(4,1)<<8);if(fraction&0x800000)fraction|=0xff000000;
                const auto height=std::uint32_t(host.terrain_height_47df40(x,y));
                calls.push_back({0x462f20,x,y,height+fraction,read(10,2)<<16,read(12,2)<<16,read(14,1)});
            };
            live_stage.set_live_bindings(binding);live_stage.submit_local_echo(frame.data(),frame.size());
            live_stage.pump_receive();
            std::cout<<"{\"op\":\"live\",\"calls\":[";
            for(std::size_t i=0;i<calls.size();++i){std::cout<<(i?",[":"[");for(std::size_t j=0;j<calls[i].size();++j)std::cout<<(j?",":"")<<calls[i][j];std::cout<<']';}
            std::cout<<"],\"globals\":[";for(std::size_t i=0;i<addresses.size();++i)std::cout<<(i?",":"")<<globals[addresses[i]];
            std::cout<<"],\"received\":"<<received<<",\"ring\":[";for(std::size_t i=0;i<ring.size();++i)std::cout<<(i?",":"")<<ring[i];
            std::cout<<"],\"writes\":[";for(std::size_t i=0;i<record_writes.size();++i){std::cout<<(i?",[":"[");for(std::size_t j=0;j<record_writes[i].size();++j)std::cout<<(j?",":"")<<record_writes[i][j];std::cout<<']';}
            std::cout<<"],\"records\":[\""<<to_hex(player.data(),player.size())<<"\",\""<<to_hex(actor.data(),actor.size())<<"\"],\"queue\":"<<records_json(live_stage)<<"}\n";
        }else if (command == "table") {
            std::ostringstream out;
            out << "{\"op\":\"table\",\"descriptors\":[";
            for (std::size_t id = 0; id < stage.table().count(); ++id) {
                const dfre::GameMessageDescriptor& descriptor =
                    stage.table().descriptor(static_cast<std::uint8_t>(id));
                out << (id ? "," : "") << "[" << static_cast<int>(descriptor.flags) << ","
                    << descriptor.length << "]";
            }
            out << "]}";
            std::cout << out.str() << "\n";
        } else if (command == "reset") {
            stage.reset();
            stage.set_single_player(true);
            std::cout << "{\"op\":\"reset\"}\n";
        } else if (command == "setsingle" || command == "setnet") {
            int value = 0;
            input >> value;
            if (command == "setsingle") {
                stage.set_single_player(value != 0);
            } else {
                stage.set_networked(value != 0);
            }
            std::cout << "{\"op\":\"" << command << "\",\"v\":" << value << "}\n";
        } else if (command == "settick") {
            std::uint32_t value = 0;
            input >> value;
            stage.set_tick_limit(value);
            std::cout << "{\"op\":\"settick\",\"v\":" << value << "}\n";
        } else if (command == "setcounter") {
            std::uint32_t value = 0;
            input >> value;
            stage.queue().set_sequence_counter(value);
            std::cout << "{\"op\":\"setcounter\",\"v\":" << value << "}\n";
        } else if (command == "setdelay") {
            int value = 0;
            input >> value;
            stage.queue().set_send_delay(static_cast<std::uint16_t>(value));
            std::cout << "{\"op\":\"setdelay\",\"v\":" << value << "}\n";
        } else if (command == "setlocal") {
            int value = 0;
            input >> value;
            stage.set_local_player_index(value);
            std::cout << "{\"op\":\"setlocal\",\"v\":" << value << "}\n";
        } else if (command == "setflag53c") {
            std::uint32_t value = 0;
            input >> value;
            stage.set_flag_67f53c(value);
            std::cout << "{\"op\":\"setflag53c\",\"v\":" << value << "}\n";
        } else if (command == "occupy") {
            std::string array;
            int index = 0;
            input >> array >> index;
            std::vector<bool>* slots = array == "players" ? stage.dispatch().player_slots()
                                                          : stage.dispatch().actor_slots();
            if (index >= static_cast<int>(slots->size())) {
                slots->resize(static_cast<std::size_t>(index) + 1, false);
            }
            (*slots)[static_cast<std::size_t>(index)] = true;
            std::cout << "{\"op\":\"occupy\",\"array\":\"" << array << "\",\"index\":" << index << "}\n";
        } else if (command == "settext") {
            dfre::GameMessageTextConfig config;
            input >> config.color_67db40 >> config.color_67db4c >> config.color_67db48 >>
                config.color_67db50 >> config.request_config_67db54 >> config.mission_config_67db5c;
            stage.set_text_config(config);
            std::cout << "{\"op\":\"settext\"}\n";
        } else if (command == "enqueue") {
            int id = 0;
            std::string payload_hex;
            input >> id >> payload_hex;
            std::vector<std::uint8_t> payload = from_hex(payload_hex);
            const bool ok = stage.queue().enqueue(
                stage.table(), static_cast<std::uint8_t>(id), payload.data(),
                static_cast<std::uint16_t>(payload.size()));
            std::cout << "{\"op\":\"enqueue\",\"ok\":" << (ok ? 1 : 0)
                      << ",\"seq\":" << stage.queue().sequence_counter() << ",\"records\":"
                      << records_json(stage) << "}\n";
        } else if (command == "worker") {
            int ticks = 0;
            input >> ticks;
            for (int tick = 0; tick < ticks; ++tick) {
                std::ostringstream out;
                out << "{\"op\":\"worker\",\"tick\":" << tick << ",\"sends\":[";
                std::vector<dfre::GameMessageSend> sends;
                stage.queue().step(stage.frame(), stage.table(), sends);
                for (std::size_t i = 0; i < sends.size(); ++i) {
                    out << (i ? "," : "") << "[" << static_cast<int>(sends[i].id) << ","
                        << sends[i].sequence << "," << sends[i].length << ",\""
                        << to_hex(sends[i].payload.data(), sends[i].length) << "\"]";
                }
                const std::string frame_hex =
                    to_hex(stage.frame().bytes().data(), stage.frame().bytes().size());
                // mirror 46F4B0: 4611C0 flush + compact, then report the result
                stage.flush_frame();
                stage.queue().compact();
                out << "],\"frame\":\"" << frame_hex << "\",\"errors\":" << stage.frame().errors().size()
                    << ",\"records\":" << records_json(stage)
                    << ",\"sent\":\"" << to_hex(host.sent.data(), host.sent.size())
                    << "\",\"net\":\"" << to_hex(stage.network_packet().data(), stage.network_packet().size())
                    << "\"}";
                std::cout << out.str() << "\n";
                host.sent.clear();
            }
        } else if (command == "serialize") {
            int id = 0;
            int sequence = 0;
            std::string payload_hex;
            input >> id >> sequence >> payload_hex;
            std::vector<std::uint8_t> payload = from_hex(payload_hex);
            stage.frame().begin();
            const bool ok = stage.frame().append(stage.table(), static_cast<std::uint8_t>(id),
                                                 static_cast<std::uint16_t>(sequence), payload.data(),
                                                 static_cast<std::uint16_t>(payload.size()));
            std::cout << "{\"op\":\"serialize\",\"ok\":" << (ok ? 1 : 0) << ",\"frame\":\""
                      << to_hex(stage.frame().bytes().data(), stage.frame().bytes().size())
                      << "\",\"errors\":" << stage.frame().errors().size() << "}\n";
        } else if (command == "flush") {
            stage.flush_queue();
            std::cout << "{\"op\":\"flush\",\"sent\":\"" << to_hex(host.sent.data(), host.sent.size())
                      << "\",\"net\":\"" << to_hex(stage.network_packet().data(), stage.network_packet().size())
                      << "\",\"records\":" << records_json(stage) << "}\n";
            host.sent.clear();
        } else if (command == "dispatch") {
            std::string frame_hex;
            input >> frame_hex;
            std::vector<std::uint8_t> frame = from_hex(frame_hex);
            std::vector<GameMessageAction> actions = stage.dispatch().dispatch(frame.data(), frame.size());
            std::cout << "{\"op\":\"dispatch\",\"actions\":" << actions_json(actions)
                      << ",\"followups\":" << records_json(stage) << ",\"state\":" << state_json(stage)
                      << "}\n";
        } else if (command == "recv") {
            std::string frame_hex;
            input >> frame_hex;
            host.incoming.push_back(from_hex(frame_hex));
            std::cout << "{\"op\":\"recv\"}\n";
        } else if (command == "pump") {
            std::vector<GameMessageAction> actions = stage.pump_receive();
            std::cout << "{\"op\":\"pump\",\"actions\":" << actions_json(actions)
                      << ",\"state\":" << state_json(stage) << "}\n";
        } else if (command == "update") {
            dfre::GameMessageUpdateInput update;
            int present = 0;
            int flag = 0;
            input >> update.x >> update.y >> update.z >> update.pitch >> update.yaw >>
                update.field_7c >> update.field_94 >> update.current_weapon_item >> present >> flag;
            update.local_present = present != 0;
            update.flag_67dbc0 = flag != 0;
            std::vector<GameMessageAction> actions = stage.update(update);
            std::cout << "{\"op\":\"update\",\"actions\":" << actions_json(actions)
                      << ",\"records\":" << records_json(stage) << ",\"frame\":\""
                      << to_hex(stage.frame().bytes().data(), stage.frame().bytes().size())
                      << "\",\"sent\":\"" << to_hex(host.sent.data(), host.sent.size())
                      << "\",\"state\":" << state_json(stage) << "}\n";
            host.sent.clear();
        } else if (command == "produce") {
            std::string which;
            input >> which;
            if (which == "shot") {
                stage.enqueue_shot();
            } else if (which == "detonate") {
                stage.enqueue_detonate();
            } else {
                stage.enqueue_weapon_trace();
            }
            std::cout << "{\"op\":\"produce\",\"which\":\"" << which << "\",\"records\":"
                      << records_json(stage) << "}\n";
        } else if (command == "placement") {
            int a = 0;
            std::uint32_t b = 0, c = 0, d = 0, e = 0;
            input >> a >> b >> c >> d >> e;
            stage.enqueue_placement(static_cast<std::uint16_t>(a), b, c, d, e);
            std::cout << "{\"op\":\"placement\",\"records\":" << records_json(stage) << "}\n";
        } else if (command == "quit") {
            std::cout << "{\"op\":\"quit\"}\n";
            break;
        } else if (!command.empty()) {
            std::cout << "{\"op\":\"error\",\"cmd\":\"" << command << "\"}\n";
        }
    }
    return 0;
}
