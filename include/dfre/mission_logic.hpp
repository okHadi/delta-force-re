#pragma once

// Mission event processor and explicit event/result inspection helpers.
// Original Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.

#include "dfre/assets.hpp"
#include "dfre/events.hpp"
#include "dfre/game_messages.hpp"
#include "dfre/mission.hpp"
#include "dfre/mission_results.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dfre {

struct MissionLogicFrameBindings {
    std::function<std::uint32_t(std::uint32_t address)> cell;
    std::function<void(std::uint32_t address,std::uint32_t value)> store;
    std::function<std::uint32_t(std::size_t slot,std::uint32_t offset,std::uint32_t width)> client;
    std::function<void(std::size_t slot,std::uint32_t offset,std::uint32_t width,std::uint32_t value)> client_store;
    // Current client actor's +6C byte; nullopt means no actor.
    std::function<std::optional<std::uint8_t>(std::size_t slot)> client_actor_flags;
    std::function<std::uint32_t(std::uint32_t group,std::size_t slot,std::uint32_t offset,std::uint32_t width)> actor;
    std::function<void(std::uint32_t group,std::size_t slot,std::uint32_t offset,std::uint8_t value)> actor_byte_store;
    // Execute each reached original body before subsequent reads.
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>& args)> call;
    std::function<void(std::uint32_t id,const std::vector<std::uint8_t>& payload)> send; // 442630
    // Explicit caller-owned bytes for original stack padding in 34/37 packets.
    std::function<std::array<std::uint8_t,3>(std::uint32_t id)> packet_padding;
};
// Complete 443C50. Canonical cells remain with their host. 46F5C0 belongs to
// the caller's outer frame; 4421D0 remains a distinct reached body.
void mission_logic_frame(const MissionLogicFrameBindings&);

using MissionLogicResultPacket=std::array<std::uint8_t,0xce>;
struct MissionLogicResultBindings {
    MissionLogicFrameBindings state;
    // Resolve canonical actor identity and original wire-table address.
    std::function<std::uint32_t(std::uint32_t group,std::size_t slot)> actor_identity;
    std::function<std::uint32_t(std::uint32_t identity)> actor_source_address;
    std::function<std::optional<std::uint32_t>()> local_actor;
    std::function<std::uint32_t(std::uint32_t identity,std::uint32_t offset,std::uint32_t width)> actor_word;
    // Read the live person name or execute the reached 404CF0 fallback.
    std::function<std::string(std::uint32_t identity)> actor_name;
    std::function<void(std::uint32_t address,const std::string&)> store_name;
    // Execute real 441750 for this current client outbox.
    std::function<void(std::size_t client,std::uint32_t id,MissionLogicResultPacket&)> publish;
};
// Packet is caller-owned source stack input; untouched bytes remain current.
void mission_logic_build_result(const MissionLogicResultBindings&,MissionLogicResultPacket&); // 4403C0
void mission_logic_request_result(const MissionLogicResultBindings&,std::uint32_t mode,MissionLogicResultPacket&); // 444CA0
void mission_logic_receive_result(const MissionLogicResultBindings&,std::uint8_t* payload,std::size_t length); // 471010

// Action record words[1] values that request a mission end (0x42E810).
inline constexpr std::uint32_t mission_action_win = 8;    // -> 0x444CA0(1)
inline constexpr std::uint32_t mission_action_fail = 9;   // -> 0x444CA0(2)
inline constexpr std::uint32_t mission_action_neutral = 10; // -> 0x444CA0(0)
// Action record words[1] values for the display masks.
inline constexpr std::uint32_t mission_action_subgoal = 14; // 0x6E0254 bit
inline constexpr std::uint32_t mission_action_subfail = 15; // 0x6E0250 bit
inline constexpr std::uint32_t mission_action_text = 6;     // 0x404DD0(ID %03i)
inline constexpr std::uint32_t mission_action_objective = 5; // 0x42EE50 bit

// Message carried by the result chain.
inline constexpr std::uint8_t mission_result_message_id = 0x1E;
inline constexpr std::uint16_t mission_result_payload_length = 0xCE;
inline constexpr std::size_t mission_result_max_clients = 17;
inline constexpr std::size_t mission_result_max_rows = mission_result_max_clients;
inline constexpr std::size_t mission_result_stat_rows = 3;

// Result modes (0x444CA0 argument, stored as payload[0]).
enum class MissionResultMode : std::uint32_t {
    neutral = 0, // action 10
    win = 1,     // action 8
    fail = 2,    // action 9
};

// One active client as the result payload builder reads it (0x4403C0).
// The caller maps its client records (original array 0x6E9A90, stride 0x9B84)
// to this shape. state is original +0x1C4 (3/4 are rewritten to 2 by 0x444CA0);
// byte_18d is the second filter byte: clients with it set are skipped by
// 0x4403C0 while [0x67F52C] == 0 (single player keeps 0x67F52C == 1).
struct MissionLogicClient {
    bool active = false;
    std::uint16_t state = 0;
    std::uint8_t byte_18d = 0;
    std::uint32_t actor_index = 0; // (client+8 actor pointer - 0x7CCA80) / 0x114
    std::uint16_t score = 0;       // payload +0x25 + 2i
    std::uint8_t byte_69 = 0;      // payload +0x69 + i
    std::uint8_t byte_8b = 0;      // payload +0x8B + i
};

// Squad statistics rows (original 0x6E80xx, up to 3; payload +0xAD..).
struct MissionLogicSquadRow {
    std::uint16_t a = 0; // payload word +0xAD + 2i
    std::uint16_t b = 0; // payload word +0xB3 + 2i
    std::uint16_t c = 0; // payload word +0xB9 + 2i
    std::uint32_t d = 0; // payload dword +0xBF + 4i
    std::uint8_t e = 0;  // payload byte  +0xCB + i
};

// Result block written by 0x471010 (0x67F714.. and the row arrays).
struct MissionLogicResultRow {   // 0x67F724 + i*0x24
    std::string name;            // actor name buffer copy
    std::uint32_t client_high = 0; // 0x67F734 (actor +0x6E >> 16)
    std::uint32_t score = 0;     // 0x67F738
    std::uint32_t byte_8b = 0;   // 0x67F73C
    std::uint32_t byte_69 = 0;   // 0x67F740
};
struct MissionLogicStatRow {     // 3 rows: 0x67FBEC/FBF8/FC04/FC10/FC1C
    std::uint32_t a = 0;         // 0x67FBEC (payload +0xAD + 2i)
    std::uint32_t b = 0;         // 0x67FBF8 (payload +0xB3 + 2i)
    std::uint32_t c = 0;         // 0x67FC04 (payload +0xB9 + 2i)
    std::uint32_t byte_cb = 0;   // 0x67FC10 (payload +0xCB + i)
    std::int32_t ratio = -1;     // 0x67FC1C ((int32 payload +0xBF+4i * 65536) / c)
};
struct MissionLogicResultBlock {
    std::uint32_t flag_67f718 = 0;      // 0x67F718 = payload[0]
    std::uint32_t local_player_67f714 = 0; // 0x67F714 = local actor +0x6E high
    std::uint32_t count_67f71c = 0;     // 0x67F71C = payload[1]
    std::uint32_t local_index_67f720 = 0; // row of the local player, else 0
    std::vector<MissionLogicResultRow> rows;
    std::array<MissionLogicStatRow, mission_result_stat_rows> stats{};
    bool count_clamped = false;         // payload[1] above 17 was clamped
    bool local_found = false;           // local actor matched a row
};

// Output of one frame or leave pass.
struct MissionLogicOutput {
    std::vector<MissionResultMode> end_requests; // accepted action 8/9/10
    std::vector<std::uint32_t> triggered_texts;  // action 6 ids
    std::vector<std::size_t> client_states_reset; // client slots 3/4 -> 2
    std::vector<std::uint8_t> messages_sent;     // ids enqueued by the result chain
    std::vector<GameMessageAction> message_actions; // decoded bus actions this pass
    bool result_dispatched = false;              // world_1e seen this pass
};

// Leave decision (typed output; nothing is written unless asked).
struct MissionLogicLeave {
    bool single_player = true;
    bool mission_ended = false;   // 0x67DBC0 guard state
    bool profile_win = false;     // 0x67F718 == 1
    std::uint32_t objective_mask = 0; // live 0x67DBEC bits
    MissionResultUpdate profile_update; // filled by apply_profile_win
    bool profile_applied = false;
};

// Inspection hooks. Derive from EventHost and GameMessageHost; the extra
// methods below supply mission-level fixture data. Reached defaults throw.
class MissionLogicHost : public EventHost, public GameMessageHost {
public:
    // Active client records in slot order (0x4403C0 / 0x444CA0). Return the
    // accepted count; the original keeps at most 17.
    virtual std::size_t clients(MissionLogicClient* clients, std::size_t capacity);
    // Client state rewrite requested by 0x444CA0 (state 3/4 -> 2).
    virtual void set_client_state(std::size_t slot, std::uint16_t state);
    // Squad statistics rows for the payload (+0xAD..). Up to 3.
    virtual std::size_t squad_rows(MissionLogicSquadRow* rows, std::size_t capacity);
    // Local player actor pointer presence (0xC23820 != 0), +0x6E value, and the
    // actor array index used to match result rows; -1 when absent.
    virtual bool local_player_present();
    virtual std::uint32_t local_player_actor_6e();
    virtual std::int32_t local_actor_index();
    // Per-row values read by 0x471010 from the client actor record:
    // name buffer text and actor +0x6E high word.
    virtual std::string client_name(std::uint32_t actor_index);
    virtual std::uint32_t client_actor_6e(std::uint32_t actor_index);
    // 0x43E720(499) sound request when the result flag equals the local id.
    virtual void play_sound_43e720(std::uint32_t id);
};

// Event/result inspection state. Live frames use mission_logic_frame.
class MissionLogic {
public:
    MissionLogic();
    ~MissionLogic();
    MissionLogic(const MissionLogic&) = delete;
    MissionLogic& operator=(const MissionLogic&) = delete;

    // Supply every reached inspection host effect.
    void set_host(MissionLogicHost* host) noexcept;
    MissionLogicHost* host() const noexcept { return host_; }

    // Load mission data and set single-player/networked flags. The state is
    // reset; 0x67DBEC objective bits persist like the original reset.
    void begin_mission(const MissionData& mission, bool single_player);
    void begin_tables(std::vector<MissionEvent> events, std::vector<EventTableRecord> triggers,
                      std::vector<EventTableRecord> actions, bool single_player);
    void set_result_context(std::uint32_t campaign, std::uint32_t mission) noexcept;
    // [0x67F52C]; single player missions set it to 1. Kept as an input.
    void set_mission_flag_67f52c(bool value) noexcept { flag_67f52c_ = value; }

    // Event/result inspection with an explicit local message bus fixture.
    // Use mission_logic_frame for the live 443C50 stage.
    MissionLogicOutput inspection_step(const GameMessageUpdateInput& input = {});

    // Leave pass: run bit-2 events (0x42EBA0) then decide the profile win.
    // Applies nothing to a profile here.
    MissionLogicLeave leave();

    // Applies the recorded win decision through mission_results rules when the
    // leave says profile_win; otherwise writes nothing.
    MissionLogicLeave leave_and_apply(AssetBytes& profile);

    // Passthroughs for the game and tests.
    EventStage& events() noexcept { return events_; }
    GameMessageStage& messages() noexcept { return messages_; }
    void dispatch_action(std::size_t action_index); // 0x42E810 through the stage
    // Format the supplied inspection rows in the result packet layout.
    std::vector<std::uint8_t> build_result_payload(MissionResultMode mode) const;

    // Decode one complete inspection result packet.
    void inspect_result_message(const std::uint8_t* payload,std::size_t length);

    // Inspection result state.
    const MissionLogicResultBlock& result_block() const noexcept { return block_; }
    bool mission_ended() const noexcept { return ended_; }        // 0x67DBC0
    bool profile_win() const noexcept { return block_.flag_67f718 == 1; }
    MissionResultMode end_mode() const noexcept { return end_mode_; }
    const MissionLogicOutput& outputs() const noexcept { return outputs_; }
    void clear_outputs() noexcept { outputs_ = {}; }
    // Game status decoded from the bus (message 0x33 writes 4 in single player).
    std::uint32_t game_status_67dbbc() noexcept;

private:
    class EventAdapter;
    class MessageAdapter;

    // Host callbacks (adapters route here).
    void end_mission(std::uint32_t mode);
    void triggered_text(std::uint32_t id);
    void apply_result_message(const std::uint8_t* payload, std::size_t length);
    void loopback_send(const std::uint8_t* frame, std::size_t length);
    std::size_t loopback_receive(const std::uint8_t** frame);
    void consume_actions(const std::vector<GameMessageAction>& actions);
    std::vector<std::uint8_t> build_result_payload(MissionResultMode mode,
                                                   const MissionLogicClient* clients,
                                                   std::size_t count) const;

    MissionLogicHost* host_ = nullptr;
    std::unique_ptr<EventAdapter> event_adapter_;
    std::unique_ptr<MessageAdapter> message_adapter_;
    EventStage events_;
    GameMessageStage messages_;
    MissionLogicResultBlock block_;
    MissionLogicOutput outputs_;
    std::vector<std::uint8_t> loopback_;
    std::vector<std::uint8_t> receive_window_;
    bool single_player_ = true;
    bool ended_ = false;                       // 0x67DBC0
    MissionResultMode end_mode_ = MissionResultMode::neutral;
    bool flag_67f52c_ = true;                  // 0x67F52C
    std::uint32_t campaign_ = 1;
    std::uint32_t mission_index_ = 0;
};

} // namespace dfre
