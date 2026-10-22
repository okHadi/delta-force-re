#include "dfre/assets.hpp"
#include "dfre/mission_logic.hpp"

#include <array>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
using dfre::MissionResultMode;

std::string hex(const std::uint8_t* data, std::size_t length) {
    std::ostringstream out;
    for (std::size_t i = 0; i < length; ++i) {
        out << std::hex << std::setw(2) << std::setfill('0') << unsigned(data[i]);
    }
    return out.str();
}

std::string quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << char(c);
        else if (c < 32 || c >= 127) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else out << char(c);
    }
    out << '"';
    return out.str();
}

// Scripted single-client host used for the corpus chain runs. Real client and
// actor state is not built yet; the fields the result chain reads are filled
// with the documented fixture values so the message and result paths run.
struct ProbeHost : dfre::MissionLogicHost {
    std::vector<dfre::MissionLogicClient> clients_data{{true, 3, 0, 0, 0x1234, 0x11, 0x22}};
    std::vector<dfre::MissionLogicSquadRow> squad_data{{1, 2, 3, 0x40000, 9}};
    std::vector<std::string> forwarded;
    std::vector<std::uint32_t> sounds;

    std::size_t clients(dfre::MissionLogicClient* out, std::size_t capacity) override {
        const std::size_t count = std::min(capacity, clients_data.size());
        for (std::size_t i = 0; i < count; ++i) out[i] = clients_data[i];
        return count;
    }
    void set_client_state(std::size_t slot, std::uint16_t state) override {
        if (slot < clients_data.size()) clients_data[slot].state = state;
    }
    std::size_t squad_rows(dfre::MissionLogicSquadRow* out, std::size_t capacity) override {
        const std::size_t count = std::min(capacity, squad_data.size());
        for (std::size_t i = 0; i < count; ++i) out[i] = squad_data[i];
        return count;
    }
    bool local_player_present() override { return true; }
    std::uint32_t local_player_actor_6e() override { return (7u << 16) | 3; }
    std::int32_t local_actor_index() override { return 0; }
    std::string client_name(std::uint32_t actor_index) override {
        return actor_index == 0 ? "BRAVO TWO" : std::string{};
    }
    std::uint32_t client_actor_6e(std::uint32_t actor_index) override {
        return actor_index == 0 ? ((7u << 16) | 3) : 0;
    }
    void weapons_tick_482b00() override {} // Explicit inspection fixture.
    void play_sound_43e720(std::uint32_t id) override { sounds.push_back(id); }
    bool world_67f520_is_zero() override { return true; }
    bool world_67f549_bit1() override { return false; }
    void action_404dd0(std::uint32_t id) override { forwarded.push_back("404dd0:" + std::to_string(id)); }
    void action_45f670(std::uint32_t a, std::uint32_t b) override {
        forwarded.push_back("45f670:" + std::to_string(a) + ":" + std::to_string(b));
    }
    void action_45f4c0(std::uint32_t a) override { forwarded.push_back("45f4c0:" + std::to_string(a)); }
    void action_4601a0(std::size_t index) override { forwarded.push_back("4601a0:" + std::to_string(index)); }
    void action_45f5b0(std::uint32_t a) override { forwarded.push_back("45f5b0:" + std::to_string(a)); }
    void action_43f500(std::uint32_t a) override { forwarded.push_back("43f500:" + std::to_string(a)); }
    void action_460640(std::uint32_t a, std::uint32_t b) override {
        forwarded.push_back("460640:" + std::to_string(a) + ":" + std::to_string(b));
    }
    void action_460280(std::size_t index) override { forwarded.push_back("460280:" + std::to_string(index)); }
};

dfre::AssetBytes synthetic_profile() {
    dfre::AssetBytes profile(0xa90, 0);
    profile[0] = 'P';
    profile[1] = 'L';
    profile[2] = 'Y';
    profile[3] = 0x1b;
    profile[0x28] = 2; // campaign 2 for the report
    return profile;
}

bool is_end_kind(std::uint32_t kind) {
    return kind == dfre::mission_action_win || kind == dfre::mission_action_fail ||
           kind == dfre::mission_action_neutral;
}

void deliver(dfre::MissionLogic& logic) {
    logic.inspection_step();
    logic.inspection_step();
}

void report_mission(const dfre::MissionData& mission, const std::string& name, std::ostream& out) {
    std::array<std::size_t, 16> kind_counts{};
    for (const auto& action : mission.actions) {
        const std::uint32_t kind = action.at(1);
        if (kind < kind_counts.size()) ++kind_counts[kind];
    }
    out << "{\"name\":" << quote(name) << ",\"events\":" << mission.events.size()
        << ",\"actions\":" << mission.actions.size() << ",\"kinds\":{";
    bool first = true;
    for (std::size_t kind = 0; kind < kind_counts.size(); ++kind) {
        if (kind_counts[kind] == 0) continue;
        if (!first) out << ",";
        first = false;
        out << "\"" << kind << "\":" << kind_counts[kind];
    }
    out << "},\"records\":[";
    first = true;
    for (std::size_t i = 0; i < mission.actions.size(); ++i) {
        const auto& action = mission.actions[i];
        const std::uint32_t kind = action.at(1);
        const bool flag_kind = kind == dfre::mission_action_objective || kind == dfre::mission_action_subgoal ||
                               kind == dfre::mission_action_subfail;
        const bool text_kind = kind == dfre::mission_action_text;
        if (!flag_kind && !text_kind && !is_end_kind(kind)) continue;
        dfre::MissionLogic logic;
        ProbeHost host;
        logic.set_host(&host);
        logic.begin_tables({}, {}, mission.actions, true);
        logic.set_result_context(2, 3);
        logic.dispatch_action(i);
        if (!first) out << ",";
        first = false;
        out << "{\"index\":" << i << ",\"kind\":" << kind << ",\"w3\":" << action.at(3)
            << ",\"w4\":" << action.at(4);
        if (kind == dfre::mission_action_objective) {
            out << ",\"dbec\":" << logic.events().flags_67dbec();
        } else if (kind == dfre::mission_action_subgoal) {
            out << ",\"mask\":" << logic.events().flags_6e0254();
        } else if (kind == dfre::mission_action_subfail) {
            out << ",\"mask\":" << logic.events().flags_6e0250();
        } else if (text_kind) {
            out << ",\"text\":" << (logic.outputs().triggered_texts.empty()
                                        ? 0
                                        : logic.outputs().triggered_texts.front())
                << ",\"forwarded\":" << host.forwarded.size();
        } else {
            const auto mode = logic.outputs().end_requests.empty()
                                  ? MissionResultMode::neutral
                                  : logic.outputs().end_requests.front();
            out << ",\"mode\":" << static_cast<std::uint32_t>(mode)
                << ",\"messages\":" << logic.outputs().messages_sent.size()
                << ",\"states_reset\":" << logic.outputs().client_states_reset.size();
            if (!logic.outputs().messages_sent.empty()) {
                const auto& record = logic.messages().queue().record(logic.messages().queue().last_slot());
                out << ",\"payload_head\":" << quote(hex(record.payload.data(), 16))
                    << ",\"record\":{\"id\":" << unsigned(record.id) << ",\"length\":" << record.length
                    << ",\"repeats\":" << record.repeats << ",\"sequence\":" << record.sequence << "}";
            }
            deliver(logic);
            out << ",\"flag\":" << logic.result_block().flag_67f718
                << ",\"win\":" << (logic.profile_win() ? 1 : 0);
            auto profile = synthetic_profile();
            const auto leave = logic.leave_and_apply(profile);
            const auto completion_offset = dfre::CampaignProfile::completion_offset(2, 3);
            const auto mask_offset = dfre::CampaignProfile::mask_offset(2);
            const auto word = [&profile](std::size_t at) {
                return std::uint32_t(profile[at]) | (std::uint32_t(profile[at + 1]) << 8) |
                       (std::uint32_t(profile[at + 2]) << 16) | (std::uint32_t(profile[at + 3]) << 24);
            };
            out << ",\"profile_applied\":" << (leave.profile_applied ? 1 : 0)
                << ",\"profile_word\":" << word(completion_offset)
                << ",\"profile_mask\":" << word(mask_offset);
        }
        out << "}";
    }
    out << "],\"ends_one_shot\":";
    // One-shot: dispatch every end action of this mission in one run. Only the
    // first may produce a message (0x67DBC0 guard).
    {
        dfre::MissionLogic logic;
        ProbeHost host;
        logic.set_host(&host);
        logic.begin_tables({}, {}, mission.actions, true);
        logic.set_result_context(2, 3);
        std::size_t end_total = 0, accepted = 0, messages = 0;
        for (std::size_t i = 0; i < mission.actions.size(); ++i) {
            if (!is_end_kind(mission.actions[i].at(1))) continue;
            ++end_total;
            logic.clear_outputs();
            logic.dispatch_action(i);
            if (!logic.outputs().end_requests.empty()) ++accepted;
            messages += logic.outputs().messages_sent.size();
        }
        out << "{\"end_actions\":" << end_total << ",\"accepted\":" << accepted
            << ",\"messages\":" << messages << "}";
    }
    out << "}";
}

int live_result_mode(){
    using U=std::uint32_t;const std::vector<U> addresses{0x67f520,0x67f528,0x67dbc0,0x67f534,0x6e558c,0x67f52c,0x67f53c,0x6e4758,0x6e5774,0x6e576c,0x6e5770,0xc23820};
    std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);
        auto next=[&]{std::uint64_t n;if(!(input>>n))throw std::runtime_error("Short live result row");return U(n);};
        auto bytes=[&](std::size_t n){std::string s;if(!(input>>s)||s.size()!=n*2)throw std::runtime_error("Bad live result image");std::vector<std::uint8_t> r(n);for(std::size_t i=0;i<n;++i)r[i]=std::uint8_t(std::stoul(s.substr(i*2,2),nullptr,16));return r;};
        const auto mode=next(),flag=next(),mutation=next();std::map<U,U> cells;for(auto at:addresses)cells[at]=next();
        auto block=bytes(0x514);auto squads=bytes(0x138);std::array<std::vector<std::uint8_t>,17> clients,actors;for(auto& c:clients)c=bytes(512);for(auto& a:actors)a=bytes(0x114);auto raw=bytes(0xce);dfre::MissionLogicResultPacket packet;std::copy(raw.begin(),raw.end(),packet.begin());
        auto read=[](const auto& a,U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(a.at(at+i))<<(i*8);return n;};auto put=[](auto& a,U at,U n,U width){for(U i=0;i<width;++i)a.at(at+i)=std::uint8_t(n>>(i*8));};
        auto load=[&](U at){if(at>=0x67f714&&at<0x67fc28)return read(block,at-0x67f714,4);if(at>=0x6e80c8&&at<0x6e8200)return read(squads,at-0x6e80c8,4);if(!cells.count(at))throw std::runtime_error("Missing result cell "+std::to_string(at));return cells.at(at);};
        auto save=[&](U at,U n){if(at>=0x67f714&&at<0x67fc28)put(block,at-0x67f714,n,4);else{if(!cells.count(at))throw std::runtime_error("Missing result store "+std::to_string(at));cells.at(at)=n;}};
        std::ostringstream trace;dfre::MissionLogicResultBindings b;b.state.cell=load;b.state.store=save;
        b.state.client=[&](auto slot,U at,U width){return read(clients.at(slot),at,width);};b.state.client_store=[&](auto slot,U at,U width,U n){put(clients.at(slot),at,n,width);};
        b.actor_source_address=[](U actor){return actor;};b.actor_identity=[](U group,auto slot){if(group!=3)throw std::runtime_error("Bad result actor group");return 0x7cca80u+U(slot)*0x114u;};
        b.local_actor=[&]()->std::optional<U>{if(!cells.at(0xc23820))return std::nullopt;return cells.at(0xc23820);};b.actor_word=[&](U identity,U at,U width){return read(actors.at((identity-0x7cca80u)/0x114u),at,width);};
        b.store_name=[&](U at,const std::string& name){for(std::size_t i=0;i<=name.size();++i)block.at(at-0x67f714+i)=i==name.size()?0:std::uint8_t(name[i]);};
        b.actor_name=[&](U actor){if(read(actors.at((actor-0x7cca80u)/0x114u),0xc,4))return std::string("PERSON");trace<<"N,"<<actor<<';';if(mutation&8){cells[0xc23820]=actor;save(0x67f71c,1);packet[0x25]^=0x80;packet[0x8b]^=0x80;}return std::string("CLIENT");};
        b.state.call=[&](U site,const std::vector<U>& args){trace<<"C,"<<site;for(auto n:args)trace<<','<<n;trace<<';';
            if(site==0x446100&&(mutation&1))cells[0x67dbc0]=0;
            if(site==0x446da0&&(mutation&2)){cells[0x67f53c]=(cells[0x67f53c]+1)%6;clients[16][0x18c]^=1;}
            if(site==0x448130){const auto a=read(clients.at(args.at(0)),0x1a0,4),c=read(clients.at(args.at(1)),0x1a0,4);const U result=a<c?0xffffffffu:a>c?1u:0u;if(mutation&16){put(clients.at(args.at(1)),0x1a0,a+7,4);put(clients.at(args.at(0)),0x1a8,254,4);cells[0x67f53c]=(cells[0x67f53c]+1)%6;}return result;}return 0u;};
        b.publish=[&](auto slot,U id,auto& p){trace<<"P,"<<slot<<','<<id<<','<<hex(p.data(),p.size())<<';';if(mutation&4){clients.at((slot+1)%17)[0x18c]^=1;cells[0x67f53c]=(cells[0x67f53c]+1)%6;p[2]^=1;}};
        bool fault=false;try{if(mode==0)dfre::mission_logic_build_result(b,packet);else if(mode==1)dfre::mission_logic_request_result(b,flag,packet);else dfre::mission_logic_receive_result(b,packet.data(),packet.size());}catch(const std::runtime_error& e){if(std::string(e.what())!="Mission result ratio divides outside signed range")throw;fault=true;}
        std::cout<<fault;for(auto at:addresses)std::cout<<' '<<cells.at(at);std::cout<<' '<<hex(block.data(),block.size())<<' '<<hex(squads.data(),squads.size());for(const auto& c:clients)std::cout<<' '<<hex(c.data(),c.size());std::cout<<' '<<hex(packet.data(),packet.size())<<' '<<trace.str()<<'\n';
    }return 0;
}

int frame_mode(){
    using U=std::uint32_t;constexpr std::array<U,22> addresses{0x67f520,0x67f528,0x67dbc0,0x67f534,0x6e558c,0x67dbbc,0x67f538,0x67dbe8,0x6e4730,0x6e4734,0x6e4738,0x6e473c,0x6e4740,0x6e4744,0x6e4748,0x6e474c,0x6e4750,0x6e4754,0x6e4758,0x6e475c,0x8cfa18,0x8cfa1c};
    std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);std::map<U,U> cells;
        auto next=[&]{std::uint64_t n;if(!(input>>n))throw std::runtime_error("Short logic frame row");return U(n);};
        auto bytes=[&](std::size_t size){std::string s;if(!(input>>s)||s.size()!=size*2)throw std::runtime_error("Bad logic frame image");std::vector<std::uint8_t> result(size);for(std::size_t i=0;i<size;++i)result[i]=std::uint8_t(std::stoul(s.substr(i*2,2),nullptr,16));return result;};
        for(auto at:addresses)cells[at]=next();auto peers=next();const auto mutation=next();std::array<std::vector<std::uint8_t>,17> clients;for(auto& c:clients)c=bytes(512);
        std::array<std::vector<std::vector<std::uint8_t>>,4> actors;for(auto group:{3u,0u})for(U i=0,n=cells[group==3?0x8cfa18:0x8cfa1c];i<n;++i)actors[group].push_back(bytes(0x114));
        auto read=[](const auto& a,U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(a.at(at+i))<<(i*8);return n;};auto put=[](auto& a,U at,U n,U width){for(U i=0;i<width;++i)a.at(at+i)=std::uint8_t(n>>(i*8));};
        std::ostringstream trace;dfre::MissionLogicFrameBindings b;
        b.cell=[&](U at){return cells.at(at);};b.store=[&](U at,U n){cells.at(at)=n;};b.client=[&](auto slot,U at,U width){return read(clients.at(slot),at,width);};b.client_store=[&](auto slot,U at,U width,U n){put(clients.at(slot),at,n,width);};
        b.client_actor_flags=[&](auto slot)->std::optional<std::uint8_t>{auto address=read(clients.at(slot),8,4);if(!address)return std::nullopt;const auto index=(address-0x7cca80u)/0x114u;return std::uint8_t(read(actors[3].at(index),0x6c,1));};
        b.actor=[&](U group,auto slot,U at,U width){return read(actors.at(group).at(slot),at,width);};b.actor_byte_store=[&](U group,auto slot,U at,std::uint8_t n){put(actors.at(group).at(slot),at,n,1);};
        b.packet_padding=[](U){return std::array<std::uint8_t,3>{0xa5,0xa5,0xa5};};
        b.call=[&](U site,const std::vector<U>& args){trace<<"C,"<<site;for(auto arg:args)trace<<','<<arg;trace<<';';
            if(site==0x460e40){if(peers){--peers;return 1u;}return 0u;}
            if(site==0x4477f0&&(mutation&1))cells[0x6e4734]=15;
            if(site==0x42eaf0&&(mutation&2)){clients[0][0x18c]^=1;cells[0x6e473c]=310;}
            if(site==0x447fb0&&(mutation&4)){put(clients.at(args.at(0)),0x198,0,4);clients.at((args.at(0)+1)%17)[0x18c]=1;}
            if(site==0x441920&&(mutation&8)&&!actors[0].empty()){actors[0][0][0x6d]|=1;put(actors[0][0],0x24,read(actors[0][0],0x24,4)+99,4);cells[0x8cfa1c]=1;}
            return 0u;};
        b.send=[&](U id,const auto& payload){trace<<"S,"<<id<<','<<hex(payload.data(),payload.size())<<';';
            if(id!=0x24){const U group=id==0xe?3:0;const auto index=read(payload,id==0xe?2:id==0x3b?12:0,2);if(mutation&16){actors.at(group).at(index)[0x6d]=0xff;actors.at(group).at(index)[0x6e]|=0x20;}if(mutation&32)cells[0x67f520]=0;}
        };
        dfre::mission_logic_frame(b);bool first=true;for(auto at:addresses){std::cout<<(first?"":" ")<<cells[at];first=false;}
        for(const auto& c:clients)std::cout<<' '<<hex(c.data(),c.size());for(auto group:{3u,0u})for(const auto& a:actors[group])std::cout<<' '<<hex(a.data(),a.size());
        std::cout<<' '<<trace.str()<<'\n';
    }return 0;
}

int decode_mode(const std::string& hex_payload, std::ostream& out) {
    std::vector<std::uint8_t> payload;
    for (std::size_t i = 0; i + 1 < hex_payload.size(); i += 2) {
        payload.push_back(std::uint8_t(std::stoul(hex_payload.substr(i, 2), nullptr, 16)));
    }
    if (payload.size() != dfre::mission_result_payload_length) {
        throw std::runtime_error("decode payload must be 206 (0xCE) bytes");
    }
    dfre::MissionLogic logic;
    ProbeHost host;
    logic.set_host(&host);
    logic.begin_tables({}, {}, {}, true);
    // Send the payload through the real bus like 0x444CA0 does, then decode it
    // through the real dispatcher (0x46D9F0 case 0x1E -> 0x471010).
    if (!logic.messages().queue().enqueue(logic.messages().table(), dfre::mission_result_message_id,
                                          payload.data(),
                                          static_cast<std::uint16_t>(payload.size()))) {
        throw std::runtime_error("queue rejected the payload");
    }
    logic.inspection_step();
    logic.inspection_step();
    const auto& block = logic.result_block();
    out << "{\"flag\":" << block.flag_67f718 << ",\"local\":" << block.local_player_67f714
        << ",\"count\":" << block.count_67f71c << ",\"local_index\":" << block.local_index_67f720
        << ",\"local_found\":" << (block.local_found ? 1 : 0) << ",\"rows\":[";
    for (std::size_t i = 0; i < block.rows.size(); ++i) {
        if (i) out << ",";
        out << "{\"name\":" << quote(block.rows[i].name) << ",\"client_high\":" << block.rows[i].client_high
            << ",\"score\":" << block.rows[i].score << ",\"byte_8b\":" << block.rows[i].byte_8b
            << ",\"byte_69\":" << block.rows[i].byte_69 << "}";
    }
    out << "],\"stats\":[";
    for (std::size_t i = 0; i < block.stats.size(); ++i) {
        if (i) out << ",";
        out << "{\"a\":" << block.stats[i].a << ",\"b\":" << block.stats[i].b
            << ",\"c\":" << block.stats[i].c << ",\"byte_cb\":" << block.stats[i].byte_cb
            << ",\"ratio\":" << block.stats[i].ratio << "}";
    }
    out << "],\"sounds\":[";
    for (std::size_t i = 0; i < host.sounds.size(); ++i) {
        if (i) out << ",";
        out << host.sounds[i];
    }
    out << "]}";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if(argc==2&&std::string(argv[1])=="--live-result")return live_result_mode();
        if(argc==2&&std::string(argv[1])=="--frame")return frame_mode();
        if (argc == 3 && std::string(argv[1]) == "--decode") {
            std::ostringstream out;
            decode_mode(argv[2], out);
            std::cout << out.str() << "\n";
            return 0;
        }
        if (argc < 2 || argc > 3) {
            std::cerr << "usage: mission_logic_probe GAME_FOLDER [BMS_MEMBER]\n"
                         "       mission_logic_probe --decode PAYLOAD_HEX\n";
            return 2;
        }
        const dfre::AssetStore assets(argv[1]);
        std::vector<std::string> names;
        if (argc == 3) names.push_back(argv[2]);
        else names = assets.missions();
        std::ostringstream out;
        out << "{\"missions\":[";
        bool first = true;
        for (const auto& name : names) {
            const auto mission = dfre::load_mission(assets, name);
            if (!first) out << ",";
            first = false;
            report_mission(mission, name, out);
        }
        out << "]}";
        std::cout << out.str() << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
