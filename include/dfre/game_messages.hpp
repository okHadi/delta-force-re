#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace dfre {

// Original in-game action/message queue for Delta Force (1998), shared by
// local simulation and multiplayer. Addresses refer to Df.exe SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
//
// Traced original functions:
//   0x46F3E0  enqueue: pick first free slot, fill the record, copy payload
//   0x46F4B0  frame worker: begin frame, repeat-delay send, flush, compact
//   0x461250  begin frame (cursor 0x8EA1C4 = 0)
//   0x4610E0  serialize one message into frame 0x8E81C4
//   0x4611C0  flush frame: single player -> peer send, network -> packet
//   0x461050  received frame getter (single player peer / network buffer)
//   0x46D9F0  frame dispatch: dedup ring, per-id decode, follow-up enqueues
//   0x46F5C0  game update: drain receive, weapons tick, player update, flush
//   0x470F40/0x470F50/0x470F60/0x470FB0  producers
//
// Record layout (0x264 bytes, 64 slots at 0xBC5D24):
//   +0x00 u16 sequence (0xBC5D1C counter, wraps to 1 after 32000)
//   +0x02 u16 repeats (1, 45 = 0x2D, or 1001 = 0x3E9 from the descriptor)
//   +0x04 u16 payload length
//   +0x06 u16 send delay frames (0x50321C, decremented once per worker call)
//   +0x08 u8  message id
//   +0x09 u8  active flag
//   +0x0A     payload (602 bytes)
//
// Frame stream (0x8E81C4, cursor 0x8EA1C4, capacity 0x2000):
//   [id][u16 sequence when descriptor flags bit 1/2][payload]
//
// Descriptor table (0x4F8D48, 0x51 entries of 4 bytes):
//   +0 id, +1 flags (bit0 self length, bit1 repeat 1001, bit2 repeat 45),
//   +2 u16 fixed payload length. Message 0x01 patches 128 bytes per entry.

inline constexpr std::uint32_t game_message_enqueue_address = 0x46F3E0;
inline constexpr std::uint32_t game_message_worker_address = 0x46F4B0;
inline constexpr std::uint32_t game_message_serialize_address = 0x4610E0;
inline constexpr std::uint32_t game_message_flush_address = 0x4611C0;
inline constexpr std::uint32_t game_message_receive_address = 0x461050;
inline constexpr std::uint32_t game_message_dispatch_address = 0x46D9F0;
inline constexpr std::uint32_t game_message_update_address = 0x46F5C0;

inline constexpr std::size_t kGameMessageQueueSlots = 64;
inline constexpr std::size_t kGameMessageRecordSize = 0x264;
inline constexpr std::size_t kGameMessagePayloadSize = 0x25A;
inline constexpr std::size_t kGameMessageFrameCapacity = 0x2000;
inline constexpr std::size_t kGameMessageDedupSlots = 0x100;
inline constexpr std::size_t kGameMessageDescriptorCount = 0x51;
// 0x4F8D48 stores the descriptors packed without the reserved ids 0x1F and
// 0x31. Game init (0x4014E9) calls 0x460DE0 to sort them into id order; the
// runtime table has holes at 0x1F and 0x31 and a real entry at 0x4F and 0x50.
inline constexpr std::size_t kGameMessagePackedCount = 0x4F;

// Descriptor byte +1 flags.
enum GameMessageDescriptorFlags : std::uint8_t {
    game_message_self_length = 0x01,   // payload starts with its own u16 length
    game_message_repeat_1001 = 0x02,   // enqueue with repeats 1001, has sequence
    game_message_repeat_45 = 0x04,     // enqueue with repeats 45, has sequence
};

struct GameMessageDescriptor {
    std::uint8_t flags = 0;
    std::uint16_t length = 0;
};

// 0x4F8D48 table. 0x460DE0 sorts the packed startup image into id order
// (0x4014E9 calls it); message 0x01 patches 32 entries (128 bytes) at a time.
class GameMessageTable {
public:
    GameMessageTable();

    std::size_t count() const noexcept { return descriptors_.size(); }
    const GameMessageDescriptor& descriptor(std::uint8_t id) const noexcept;
    // Code switch in 0x46D9F0; independent of table patches.
    static bool dispatchable(std::uint8_t id) noexcept;
    // 0x46D9F0 case 0x01: copy 128 bytes to descriptors[index * 32].
    void patch(std::uint32_t index, const std::uint8_t* bytes);
    const std::array<GameMessageDescriptor, kGameMessageDescriptorCount>& descriptors() const noexcept {
        return descriptors_;
    }

private:
    std::array<GameMessageDescriptor, kGameMessageDescriptorCount> descriptors_{};
};

// One queue record (0xBC5D24 slot).
struct GameMessageRecord {
    std::uint16_t sequence = 0;  // +0x00
    std::uint16_t repeats = 0;   // +0x02
    std::uint16_t length = 0;    // +0x04
    std::uint16_t delay = 0;     // +0x06
    std::uint8_t id = 0;         // +0x08
    std::uint8_t active = 0;     // +0x09
    std::array<std::uint8_t, kGameMessagePayloadSize> payload{};  // +0x0A
};

// One serialized send the worker handed to 0x4610E0.
struct GameMessageSend {
    std::uint8_t id = 0;
    std::uint16_t sequence = 0;
    std::uint16_t length = 0;
    std::array<std::uint8_t, kGameMessagePayloadSize> payload{};
};

// 0x8E81C4 frame builder (0x461250 begin, 0x4610E0 append, 0x4611C0 flush).
class GameMessageFrame {
public:
    struct Error {
        std::uint8_t id = 0;
        std::uint16_t length = 0;  // length the sender passed
        std::uint16_t expected = 0;
    };

    void begin() noexcept { bytes_.clear(); }
    // 0x4610E0. Returns false when the fixed-length descriptor does not match
    // length (the original reports com_ClientAddToMessageQueue and still
    // appends the bytes).
    bool append(const GameMessageTable& table, std::uint8_t id, std::uint16_t sequence,
                const std::uint8_t* payload, std::uint16_t length);
    // 0x2000 capacity rule: cursor + length + 0x10 > 0x2000 drops the message.
    bool overflowed() const noexcept { return overflowed_; }
    const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    const std::vector<Error>& errors() const noexcept { return errors_; }
    void clear() noexcept {
        bytes_.clear();
        errors_.clear();
        overflowed_ = false;
    }

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<Error> errors_;
    bool overflowed_ = false;
};

// 0xBC5D24 queue plus the 0x46F3E0/0x46F4B0 rules.
class GameMessageQueue {
public:
    // 0x46F3E0. Returns false when the queue is full, the id is zero, or the
    // length is oversized (the original would overrun into the next slot).
    // The runtime descriptor table supplies the repeat count.
    bool enqueue(const GameMessageTable& table, std::uint8_t id, const std::uint8_t* payload,
                 std::uint16_t length);
    // 0x46F4B0 pass 1: delay ticks, repeat sends, free finished records.
    // Sends go to the frame builder through serialize (0x4610E0).
    void step(GameMessageFrame& frame, const GameMessageTable& table,
              std::vector<GameMessageSend>& sends);
    // 0x46F4B0 pass 2: move active records to the front, keep order.
    void compact();

    const std::array<GameMessageRecord, kGameMessageQueueSlots>& records() const noexcept {
        return records_;
    }
    const GameMessageRecord& record(std::size_t slot) const noexcept { return records_[slot]; }
    // Slot written by the most recent successful enqueue.
    std::size_t last_slot() const noexcept { return last_slot_; }

    std::uint32_t sequence_counter() const noexcept { return sequence_counter_; }  // 0xBC5D1C
    void set_sequence_counter(std::uint32_t value) noexcept { sequence_counter_ = value; }
    std::uint16_t send_delay() const noexcept { return send_delay_; }  // 0x50321C
    void set_send_delay(std::uint16_t value) noexcept { send_delay_ = value; }

    // 0x46D9F0 case 0x14: clear every active record with this sequence.
    std::size_t cancel_sequence(std::uint16_t sequence);
    std::size_t active_count() const noexcept;

private:
    std::array<GameMessageRecord, kGameMessageQueueSlots> records_{};
    std::uint32_t sequence_counter_ = 0;
    std::uint16_t send_delay_ = 0;
    std::size_t last_slot_ = 0;
};

// Record arrays the dispatch writes into. The stage only decodes the writes;
// the game/actor/world modules own the memory.
enum class GameRecordArray : std::uint8_t {
    none = 0,
    players_7cca80 = 1,  // 0x7CCA80, 256 x 0x114
    actors_7dde80 = 2,   // 0x7DDE80, 256 x 0x114
    squad_867e80 = 3,    // 0x867E80, 256 x 0x114
    effects_8ace80 = 4,  // 0x8ACE80, 256 x 0x114
};

inline constexpr std::size_t kGameRecordSize = 0x114;

struct GameRecordWrite {
    // Record offset for array actions; absolute address when array is none.
    std::uint32_t offset = 0;
    std::uint8_t width = 0;  // 1, 2 or 4
    std::uint32_t value = 0;
    friend bool operator==(const GameRecordWrite& a,const GameRecordWrite& b) {
        return a.offset==b.offset && a.width==b.width && a.value==b.value;
    }
};

// Inspection result. One entry per dispatched message; sequenced messages
// that fail dedup produce one entry with duplicate = true and no effect.
enum class GameMessageActionKind : std::uint16_t {
    descriptor_patch,   // 0x01  args[0] = index, bytes = 128
    client_state,       // 0x03  bytes = 0x10C copy, args[0] = +0x74 dword
    player_create,      // 0x07  0x7CCA80 create
    actor_create,       // 0x08  0x7DDE80 create
    projectile_spawn,   // 0x09  0x462F20(pos, dcb, class)
    world_command,      // 0x0B  0x470D50(payload)
    actor_batch_create, // 0x0C  0x867E80 batch
    flag_d,             // 0x0D  0xBC5D14 = 1
    player_move,        // 0x0E  0x7CCA80 update
    actor_move,         // 0x0F  0x7CCA80 move
    player_reset,       // 0x10  0x45DA30
    player_weapon_flag, // 0x11  +0x6C |= 4, +0x7C
    hud_text,           // 0x12  0x479F00
    local_respawn,      // 0x13  0x4848C0 + 0x48DD60 + 0x42CEF0
    queue_cancel,       // 0x14  clear queue by sequence
    scoreboard,         // 0x15  0xBCF664 table
    player_replace,     // 0x16  0x7CCA80 create + 0x45D6F0
    player_state,       // 0x18  0x7CCA80 update + 0x45D6F0
    flag_19,            // 0x19  0xBC5D18 = 1
    set_send_delay,     // 0x1A  0x50321C (single player)
    set_value_67f548,   // 0x1B  (single player)
    local_death,        // 0x1C  0xBCFC2C/30/34 + local player
    mission_state,      // 0x1D  0x4848C0 + HUD string + 0x43E720
    world_1e,           // 0x1E  0x471010
    world_20,           // 0x20  0x470400
    control_flags,      // 0x23  0xBCFC14 + 0x455AF0/30/10
    set_net_flag,       // 0x24  0x67DBE8 (single player)
    entity_action,      // 0x25  0x43F5A0
    effect_batch_create,// 0x26  0x8ACE80 batch
    hud_mode,           // 0x27  0x4606B0/0x4607E0/0x460950
    actor_state,        // 0x29  0x7DDE80 update
    actor_place,        // 0x2B  0x7DDE80 create + 0x43E7D0 sounds
    mission_ok,         // 0x2D
    mission_denied,     // 0x2E
    set_byte_c23b78,    // 0x30
    set_flag_67dbbc,    // 0x33 (single player)
    actor_update_34,    // 0x34
    actor_update_35,    // 0x35
    actor_update_36,    // 0x36  + 0x477820
    actor_update_37,    // 0x37
    actor_update_38,    // 0x38
    entity_callback,    // 0x39  call [record + 0x110] (single player)
    sound_pair,         // 0x3A  0x45DC70 + 0x43E7D0 (single player)
    actor_state_dcb,    // 0x3B
    local_text,         // 0x3C  0x4560C0 (single player)
    copy_blob,          // 0x3D  0x50352C copy
    input_ints,         // 0x3E  0x471570 (single player)
    input_blob,         // 0x3F  0x4715B0 (single player)
    input_none,         // 0x41  0x471600 (single player)
    session_strings,    // 0x42  0xBCF624 + 0x67F438 (single player)
    input_43,           // 0x43  0x471760
    hud_44,             // 0x44  0x479FA0(3) (single player)
    input_46,           // 0x46  0x471810
    hud_47,             // 0x47  0x479FA0(0x2E) (single player)
    input_48,           // 0x48  0x471870
    weapon_list_request,// 0x4A  0x46C180 + enqueue 0x50 (single player)
    slot_report,        // 0x4B  enqueue 0x4C (single player)
    class_report,       // 0x4D  enqueue 0x4E (single player)
    notify_45d6f0,      // 0x16/0x18 player record change notification
    reset_45da30,       // 0x10/0x18 record reset
    mbp_error,          // occupied record create attempt
    unknown,            // default case, skipped by length
    undispatchable,     // default case with zero length: fails closed
};

struct GameMessageAction {
    GameMessageActionKind kind = GameMessageActionKind::unknown;
    std::uint8_t id = 0;
    std::uint16_t sequence = 0;
    bool duplicate = false;
    GameRecordArray array = GameRecordArray::none;
    std::int32_t index = -1;
    std::array<std::int32_t, 8> args{};
    std::vector<GameRecordWrite> writes;
    std::vector<std::uint8_t> bytes;
    std::uint32_t address = 0;  // original callee address for call actions

    friend bool operator==(const GameMessageAction& a,const GameMessageAction& b) {
        return a.kind==b.kind && a.id==b.id && a.sequence==b.sequence &&
            a.duplicate==b.duplicate && a.array==b.array && a.index==b.index &&
            a.args==b.args && a.writes==b.writes && a.bytes==b.bytes && a.address==b.address;
    }
};

// Required metadata and transport bodies. Unbound methods throw.
class GameMessageHost {
public:
    virtual ~GameMessageHost() = default;

    // 0x4828C0: object lookup returning the 0xC23810 record index.
    virtual std::int32_t lookup_4828c0(std::int32_t value);
    // 0xC23810 holds the pointer to the 0x13C record table.
    virtual std::uint32_t actor_record_base();
    // 0x44A0C0: allocate a name buffer (pointer), 4 byte header.
    virtual std::int32_t allocate_44a0c0(GameRecordArray array, std::int32_t index);
    // 0xC23810 + index * 0x13C field reads.
    virtual std::int32_t actor_record_dword(std::int32_t index, std::uint32_t offset);
    virtual std::uint16_t actor_record_word(std::int32_t index, std::uint32_t offset);
    // 0x42CCE0: secondary lookup used by player/actor records.
    virtual std::int32_t lookup_42cce0(std::int32_t value);
    // 0x47DF40: terrain height at (x, y).
    virtual std::int32_t terrain_height_47df40(std::uint32_t x, std::uint32_t y);
    // 0x477820: offdiag pair for an angle. The original passes two output
    // pointers; +0x40 uses the second pushed output, +0x3C the first.
    virtual void sincos_477820(std::int32_t angle, std::int32_t& out_40, std::int32_t& out_3c);
    // 0x46C180: weapon list build (returns a pointer/index for payload +0).
    virtual std::int32_t weapon_list_46c180();
    // [0xC2382C]: session counter used by message 0x4A.
    virtual std::uint32_t session_counter_c2382c();
    // 0xC239B8 slot index and 0xC239C0 slot map; weapon update for message 0x2F.
    virtual std::uint32_t current_weapon_item_c239b8_c239c0();
    virtual std::uint32_t weapon_aim_c23b70();
    virtual std::uint32_t weapon_aim_c23b74();
    // 0xAD3ED0 weapon table (+0x74 byte, +0x9C word, +0xA0 byte, +0xA4 word)
    // and 0xAD3EE4 class table (+8 word) reads for messages 0x4B / 0x4D.
    virtual std::uint32_t weapon_table_field(std::uint32_t index, std::uint32_t offset,
                                             std::uint32_t width);
    virtual std::uint32_t class_table_field(std::uint32_t index, std::uint32_t offset,
                                            std::uint32_t width);
    virtual std::uint32_t slot_map_word(std::uint32_t slot);
    // Record `index` in `array` publishes a pointer at 0x110 (0x39 callback).
    virtual std::int32_t record_field_110(GameRecordArray array, std::int32_t index);

    // 0x46122D single player: deliver the frame to the peer object.
    virtual void send_local_frame(const std::uint8_t* frame, std::size_t length);
    // 0x46108F single player: fetch the next received frame. Return 0 for none.
    virtual std::size_t receive_local_frame(const std::uint8_t** frame);
    // 0x482B00 weapons tick, run inside update().
    virtual void weapons_tick_482b00();
};

// 0x67DB40/48/4C/50 text color values and message config read by HUD cases.
struct GameMessageTextConfig {
    std::uint32_t color_67db40 = 0;         // message 0x12 mode 0
    std::uint32_t color_67db4c = 0;         // modes 1, 4, 5
    std::uint32_t color_67db48 = 0;         // mode 2
    std::uint32_t color_67db50 = 0;         // mode 3
    std::uint32_t request_config_67db54 = 0;  // messages 0x2D / 0x2E
    std::uint32_t mission_config_67db5c = 0;  // message 0x1D
};

// 0x46D9F0 frame dispatch plus the bus state it owns.
class GameMessageDispatch {
public:
    GameMessageDispatch(GameMessageTable& table, GameMessageQueue& queue, GameMessageHost* host);

    void set_host(GameMessageHost* host) noexcept { host_ = host; }
    void set_text_config(const GameMessageTextConfig& config) noexcept { text_config_ = config; }

    // Parses one received frame in order. Sequenced messages:
    //   1. enqueue 0x14 with the sequence (cancel pending repeats),
    //   2. skip when the sequence is in the 256 entry 0xBCFC3C ring,
    //   3. otherwise shift the ring and process the case.
    // Frames containing a zero-length message with no case fail closed with an
    // undispatchable action instead of the original stream under-run.
    std::vector<GameMessageAction> dispatch(const std::uint8_t* frame, std::size_t length);

    // 0xBCFC3C dedup ring, newest sequence first.
    const std::array<std::uint16_t, kGameMessageDedupSlots>& dedup_ring() const noexcept {
        return dedup_ring_;
    }

    // Stage values the dispatched messages leave behind for other modules.
    bool flag_bc5d14() const noexcept { return flag_bc5d14_; }             // message 0x0D
    bool flag_bc5d18() const noexcept { return flag_bc5d18_; }             // message 0x19
    std::uint32_t delay_50321c() const noexcept { return delay_50321c_; }  // message 0x1A
    std::uint32_t value_67f548() const noexcept { return value_67f548_; }  // message 0x1B
    std::uint32_t death_bcfc2c() const noexcept { return death_bcfc2c_; }
    std::uint32_t death_bcfc30() const noexcept { return death_bcfc30_; }
    std::uint32_t death_bcfc34() const noexcept { return death_bcfc34_; }
    std::uint32_t control_bcfc14() const noexcept { return control_bcfc14_; }
    std::uint32_t net_flag_67dbe8() const noexcept { return net_flag_67dbe8_; }
    std::uint32_t byte_c23b78() const noexcept { return byte_c23b78_; }
    std::uint32_t flag_67dbbc() const noexcept { return flag_67dbbc_; }
    std::uint32_t entity_callback_a158c0() const noexcept { return entity_callback_a158c0_; }
    std::uint32_t max_players_8cfa18() const noexcept { return max_players_; }
    std::uint32_t max_actors_8cfa1c() const noexcept { return max_actors_; }
    std::uint32_t max_squad_8cf904() const noexcept { return max_squad_; }
    std::uint32_t max_effects_8cf900() const noexcept { return max_effects_; }
    const std::string& session_string_bcf624() const noexcept { return session_string_bcf624_; }
    const std::string& mission_file_67f438() const noexcept { return mission_file_67f438_; }
    const std::array<std::uint8_t, 0x100>& blob_50352c() const noexcept { return blob_50352c_; }
    // 0xBCFBE8 / 0xBCFBE4 and the 0xBCF664 entry table (message 0x15).
    std::uint32_t scoreboard_count() const noexcept { return scoreboard_count_; }
    std::uint32_t scoreboard_field_bcfbe4() const noexcept { return scoreboard_field_bcfbe4_; }
    const std::vector<std::string>& scoreboard_names() const noexcept { return scoreboard_names_; }
    const std::vector<std::int32_t>& scoreboard_values() const noexcept { return scoreboard_values_; }
    const std::array<std::int32_t, 3>& scoreboard_pair_a() const noexcept { return scoreboard_pair_a_; }
    const std::array<std::int32_t, 3>& scoreboard_pair_b() const noexcept { return scoreboard_pair_b_; }
    const std::array<std::int32_t, 3>& scoreboard_pair_c() const noexcept { return scoreboard_pair_c_; }

    // Local player record selection for messages 0x13 and 0x1C and gates that
    // read [0x67F528] (0 = single player paths enabled).
    void set_single_player(bool enabled) noexcept { single_player_ = enabled; }
    void set_local_player_index(std::int32_t index) noexcept { local_player_index_ = index; }
    void set_flag_67f53c(std::uint32_t value) noexcept { flag_67f53c_ = value; }

    // Occupancy of the record arrays (records created earlier are game state).
    std::vector<bool>* player_slots() noexcept { return &player_slots_; }
    std::vector<bool>* actor_slots() noexcept { return &actor_slots_; }

    // Queue messages appended by the dispatch (0x14, 0x17, 0x50, 0x4C, 0x4E).
    const std::vector<GameMessageRecord>& followups() const noexcept { return followups_; }

    // Restores construction state (ring, flags, counters, occupancy, config).
    void reset();

private:
    GameMessageAction& start_action(GameMessageActionKind kind, std::uint8_t id);
    void push_write(std::uint32_t offset, std::uint8_t width, std::uint32_t value);
    void call_action(GameMessageActionKind kind, std::uint8_t id, std::uint32_t address);
    void enqueue_followup(std::uint8_t id, const std::uint8_t* payload, std::uint16_t length);

    GameMessageTable& table_;
    GameMessageQueue& queue_;
    GameMessageHost* host_;
    std::array<std::uint16_t, kGameMessageDedupSlots> dedup_ring_{};
    std::vector<GameMessageAction> actions_;
    std::vector<GameMessageRecord> followups_;
    GameMessageAction* current_ = nullptr;

    bool single_player_ = false;
    std::int32_t local_player_index_ = -1;
    std::uint32_t flag_67f53c_ = 0;
    GameMessageTextConfig text_config_{};

    bool flag_bc5d14_ = false;
    bool flag_bc5d18_ = false;
    std::uint32_t delay_50321c_ = 0;
    std::uint32_t value_67f548_ = 0;
    std::uint32_t death_bcfc2c_ = 0;
    std::uint32_t death_bcfc30_ = 0;
    std::uint32_t death_bcfc34_ = 0;
    std::uint32_t control_bcfc14_ = 0;
    std::uint32_t net_flag_67dbe8_ = 0;
    std::uint32_t byte_c23b78_ = 0;
    std::uint32_t flag_67dbbc_ = 0;
    std::uint32_t entity_callback_a158c0_ = 0;
    std::uint32_t max_players_ = 0;
    std::uint32_t max_actors_ = 0;
    std::uint32_t max_squad_ = 0;
    std::uint32_t max_effects_ = 0;
    std::string session_string_bcf624_;
    std::string mission_file_67f438_;
    std::array<std::uint8_t, 0x100> blob_50352c_{};
    std::uint32_t scoreboard_count_ = 0;
    std::uint32_t scoreboard_field_bcfbe4_ = 0;
    std::vector<std::string> scoreboard_names_;
    std::vector<std::int32_t> scoreboard_values_;
    std::array<std::int32_t, 3> scoreboard_pair_a_{};
    std::array<std::int32_t, 3> scoreboard_pair_b_{};
    std::array<std::int32_t, 3> scoreboard_pair_c_{};
    std::vector<bool> player_slots_;
    std::vector<bool> actor_slots_;
    // Original occupied-record error path reports these and stops the case.
};

// 0x46F5C0 update inputs. The original reads these globals.
struct GameMessageUpdateInput {
    // Local player record fields read for the 0x0A position update.
    bool local_present = false;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
    std::uint32_t pitch = 0;
    std::uint32_t yaw = 0;
    std::uint8_t field_7c = 0;
    std::uint8_t field_94 = 0;
    // 0xC239C0[0xC239B8] == 0x1B runs the 0x2F producer.
    std::uint32_t current_weapon_item = 0;
    bool flag_67dbc0 = false;
};
struct GameMessageLiveCall {
    std::uint32_t address=0;
    std::array<std::uint32_t,4> args{};
    std::uint8_t count=0;
    GameRecordArray array=GameRecordArray::none;
    std::int32_t index=-1;
    std::uint8_t* payload=nullptr;
    std::size_t length=0;
};
struct GameMessageLiveBindings {
    std::array<std::uint16_t,kGameMessageDedupSlots>* dedup=nullptr;
    std::uint32_t* received_bytes_bcfc24=nullptr;
    std::uint32_t* update_counter_bc5d20=nullptr;
    std::function<std::uint32_t(std::uint32_t)> read_global;
    std::function<void(std::uint32_t,std::uint32_t)> write_global;
    std::function<std::uint32_t(GameRecordArray,std::int32_t,std::uint32_t)> record_dword;
    std::function<void(GameRecordArray,std::int32_t,std::uint32_t,std::uint8_t,std::uint32_t)> record_write;
    std::function<void(const GameMessageLiveCall&)> call;
    // Owns the complete decoded 09 body (46EA40), including terrain and 462F20.
    std::function<void(std::uint8_t*,std::size_t)> projectile_09;
    // Required source-order body for an id not owned by the live router.
    std::function<bool(std::uint8_t,std::uint8_t*,std::size_t)> message;
    std::function<GameMessageUpdateInput()> current_update;
    std::function<void()> player_update_0a;
};

// Whole message bus: queue, frame builder, dispatch and the 0x46F5C0 update.
class GameMessageStage {
public:
    GameMessageStage();
    GameMessageStage(const GameMessageStage&)=delete;
    GameMessageStage& operator=(const GameMessageStage&)=delete;

    void set_host(GameMessageHost* host) noexcept;
    GameMessageTable& table() noexcept { return table_; }
    GameMessageQueue& queue() noexcept { return queue_; }
    GameMessageFrame& frame() noexcept { return frame_; }
    GameMessageDispatch& dispatch() noexcept { return dispatch_; }

    void set_single_player(bool enabled) noexcept;   // [0x67F520] == 0
    void set_networked(bool enabled) noexcept;       // [0x67F528] != 0
    void set_tick_limit(std::uint32_t value) noexcept { tick_limit_ = value; }  // 0x67F538
    void set_local_player_index(std::int32_t index) noexcept;
    void set_flag_67f53c(std::uint32_t value) noexcept;
    void set_text_config(const GameMessageTextConfig& config) noexcept;
    void set_inspection_mode(bool enabled) noexcept { inspection_=enabled; }
    void set_live_bindings(GameMessageLiveBindings bindings){live_=std::move(bindings);}
    bool check_receive(); // 461050, consumes the current echo before dispatch
    std::vector<GameMessageAction> dispatch_received(); // 46D9F0, synchronous

    // 0x46F5C0. Live effects run synchronously. Inspection mode returns actions.
    std::vector<GameMessageAction> update(const GameMessageUpdateInput& input);

    // Restores construction state without touching the wired host.
    void reset();

    // 0x461050 + 0x46D9F0 loop only (receive drain).
    std::vector<GameMessageAction> pump_receive();

    // 0x46F4B0 worker (begin frame, sends, flush, compact).
    void flush_queue();

    // 0x4610E0 single message serialization into the frame (for tests).
    void serialize(std::uint8_t id, std::uint16_t sequence, const std::uint8_t* payload,
                   std::uint16_t length);

    // 0x4611C0 flush of the current frame (transport call or packet append).
    void flush_frame();

    // Producers.
    void enqueue_player_update(const GameMessageUpdateInput& input);  // 0x0A
    void enqueue_shot();                                              // 0x470F40 -> 0x28
    void enqueue_detonate();                                          // 0x470F50 -> 0x2C
    void enqueue_placement(std::uint16_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d,
                           std::uint32_t e);                          // 0x470F60 -> 0x2A
    void enqueue_weapon_trace();                                      // 0x470FB0 -> 0x2F
    void enqueue_peer_ping();                                         // 0x46F5C0 -> 0x32

    // 0x4611C0 flush bookkeeping for the network path: the original appends
    // finished frames to 0x8EC1CC / 0x8EE1CC.
    const std::vector<std::uint8_t>& network_packet() const noexcept { return network_packet_; }
    // 0x8EA1C8 local echo buffer (filled by the effects queue and the network).
    void submit_local_echo(const std::uint8_t* frame, std::size_t length);
    const std::vector<std::uint8_t>& pending_local_echo() const noexcept { return local_echo_; }

    std::uint32_t update_counter() const noexcept { return live_.update_counter_bc5d20?*live_.update_counter_bc5d20:update_counter_; }
    std::uint32_t tick_counter() const noexcept { return tick_counter_; }      // 0xBC5D10
    std::uint32_t player_update_counter() const noexcept { return player_update_counter_; }  // 0xBCFC28

private:
    void dispatch_live();
    std::uint32_t& update_cell();
    std::uint32_t source_global(std::uint32_t) const;
    GameMessageTable table_;
    GameMessageQueue queue_;
    GameMessageFrame frame_;
    GameMessageHost default_host_;
    GameMessageHost* host_ = &default_host_;
    GameMessageDispatch dispatch_;
    bool single_player_ = false;
    bool networked_ = false;
    std::uint32_t tick_limit_ = 1;
    std::uint32_t update_counter_ = 0;
    std::uint32_t tick_counter_ = 0;
    std::uint32_t player_update_counter_ = 0;
    std::vector<std::uint8_t> network_packet_;
    std::vector<std::uint8_t> local_echo_;
    std::vector<std::uint8_t> received_;
    bool received_pending_=false,inspection_=false,dispatching_=false;
    GameMessageLiveBindings live_;
};

}  // namespace dfre
