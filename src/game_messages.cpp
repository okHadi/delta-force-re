#include "dfre/game_messages.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace dfre {
namespace {

// Packed 0x4F8D48 image: {id, flags, length}. Ids 0x1F and 0x31 are reserved
// and not stored; 0x4014E9 runs 0x460DE0 to sort these into id order.
struct PackedDescriptor {
    std::uint8_t id;
    std::uint8_t flags;
    std::uint16_t length;
};

constexpr PackedDescriptor kPackedDescriptors[] = {
    {0x00, 0x00, 0x0000}, {0x01, 0x02, 0x0084}, {0x02, 0x02, 0x0000}, {0x03, 0x02, 0x010C},
    {0x04, 0x02, 0x0000}, {0x05, 0x02, 0x0012}, {0x06, 0x02, 0x0036}, {0x07, 0x02, 0x002D},
    {0x08, 0x02, 0x002A}, {0x09, 0x00, 0x000F}, {0x0A, 0x00, 0x0010}, {0x0B, 0x02, 0x00E2},
    {0x0C, 0x02, 0x00C2}, {0x0D, 0x02, 0x0000}, {0x0E, 0x00, 0x0012}, {0x0F, 0x00, 0x000E},
    {0x10, 0x02, 0x0002}, {0x11, 0x02, 0x0004}, {0x12, 0x03, 0x0000}, {0x13, 0x02, 0x0010},
    {0x14, 0x00, 0x0002}, {0x15, 0x00, 0x008F}, {0x16, 0x02, 0x002C}, {0x17, 0x02, 0x0002},
    {0x18, 0x02, 0x002D}, {0x19, 0x02, 0x0000}, {0x1A, 0x02, 0x0004}, {0x1B, 0x02, 0x0004},
    {0x1C, 0x00, 0x0007}, {0x1D, 0x02, 0x0000}, {0x1E, 0x02, 0x00CE}, {0x20, 0x02, 0x0003},
    {0x21, 0x00, 0x000C}, {0x22, 0x02, 0x0028}, {0x23, 0x02, 0x0014}, {0x24, 0x00, 0x0002},
    {0x25, 0x00, 0x0002}, {0x26, 0x02, 0x00EA}, {0x27, 0x02, 0x0012}, {0x28, 0x02, 0x0000},
    {0x29, 0x00, 0x0010}, {0x2A, 0x02, 0x000F}, {0x2B, 0x02, 0x002B}, {0x2C, 0x02, 0x0000},
    {0x2D, 0x02, 0x0000}, {0x2E, 0x02, 0x0000}, {0x2F, 0x00, 0x0004}, {0x30, 0x00, 0x0001},
    {0x32, 0x00, 0x0000}, {0x33, 0x02, 0x0000}, {0x34, 0x00, 0x0018}, {0x35, 0x00, 0x001C},
    {0x36, 0x00, 0x001C}, {0x37, 0x00, 0x0018}, {0x38, 0x00, 0x0014}, {0x39, 0x02, 0x0004},
    {0x3A, 0x00, 0x0010}, {0x3B, 0x00, 0x0012}, {0x3C, 0x02, 0x000A}, {0x3D, 0x02, 0x0100},
    {0x3E, 0x00, 0x0003}, {0x3F, 0x02, 0x000A}, {0x40, 0x03, 0x0000}, {0x41, 0x02, 0x0002},
    {0x42, 0x02, 0x0040}, {0x43, 0x00, 0x0003}, {0x44, 0x02, 0x0000}, {0x45, 0x02, 0x0001},
    {0x46, 0x02, 0x0008}, {0x47, 0x02, 0x0000}, {0x48, 0x00, 0x000F}, {0x49, 0x00, 0x0100},
    {0x4A, 0x00, 0x0000}, {0x4B, 0x00, 0x0001}, {0x4C, 0x00, 0x0009}, {0x4D, 0x00, 0x0001},
    {0x4E, 0x00, 0x0003}, {0x4F, 0x02, 0x0010}, {0x50, 0x00, 0x0008},
};

std::uint16_t load_u16(const std::uint8_t* data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t load_u32(const std::uint8_t* data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

// Reads inside the dispatch cases come from the frame stream; the original can
// read past a short payload into the next message. The port clamps to the
// payload length and reports zero, keeping the replay corpus honest.
class PayloadReader {
public:
    PayloadReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    std::uint8_t u8(std::size_t offset) {
        if (offset + 1 > size_) {
            truncated_ = true;
            return 0;
        }
        return data_[offset];
    }
    std::uint16_t u16(std::size_t offset) {
        if (offset + 2 > size_) {
            truncated_ = true;
            return 0;
        }
        return load_u16(data_, offset);
    }
    std::uint32_t u32(std::size_t offset) {
        if (offset + 4 > size_) {
            truncated_ = true;
            return 0;
        }
        return load_u32(data_, offset);
    }
    std::int16_t i16(std::size_t offset) { return static_cast<std::int16_t>(u16(offset)); }
    std::int32_t i32(std::size_t offset) { return static_cast<std::int32_t>(u32(offset)); }
    std::size_t size() const noexcept { return size_; }
    bool truncated() const noexcept { return truncated_; }

    // Copies the NUL-terminated string at offset; the original copies in two
    // byte steps and stops after the terminating zero.
    bool string(std::size_t offset, std::string& out) {
        for (std::size_t at = offset; at < size_;) {
            std::uint8_t first = data_[at];
            out.push_back(static_cast<char>(first));
            if (first == 0) {
                return true;
            }
            if (at + 1 >= size_) {
                truncated_ = true;
                return false;
            }
            std::uint8_t second = data_[at + 1];
            out.push_back(static_cast<char>(second));
            if (second == 0) {
                return true;
            }
            at += 2;
        }
        truncated_ = true;
        return false;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    bool truncated_ = false;
};

constexpr std::uint32_t kActorTableBase = 0xC23810;
constexpr std::uint32_t kActorTableStride = 0x13C;

constexpr std::uint32_t kPlayerDcbArray = 0x7CCA80;  // 256 x 0x114
constexpr std::uint32_t kActorDcbArray = 0x7DDE80;
[[maybe_unused]] constexpr std::uint32_t kSquadDcbArray = 0x867E80;
[[maybe_unused]] constexpr std::uint32_t kEffectDcbArray = 0x8ACE80;

constexpr std::uint16_t kSequencedMask = 0x06;
constexpr std::uint8_t kSelfLength = 0x01;

bool is_actor_type(std::uint32_t type) {
    return type == 0x3F9 || type == 0x3FA || type == 0x423 || type == 0x424;
}

// 0x46D9F0 collision text at 0x4CCCA0 ("MbP ERROR_ Tried to erase a DCB").
constexpr std::uint32_t kDcbInUseErrorText = 0x4CCCA0;

}  // namespace

// ---------------------------------------------------------------- table ----

GameMessageTable::GameMessageTable() {
    std::array<std::array<std::uint8_t, 4>, 0x100> packed{};
    for (std::size_t i = 0; i < sizeof(kPackedDescriptors) / sizeof(kPackedDescriptors[0]); ++i) {
        packed[i][0] = kPackedDescriptors[i].id;
        packed[i][1] = kPackedDescriptors[i].flags;
        packed[i][2] = static_cast<std::uint8_t>(kPackedDescriptors[i].length & 0xFF);
        packed[i][3] = static_cast<std::uint8_t>(kPackedDescriptors[i].length >> 8);
    }
    // 0x460DE0: for each id 0..0xFE move the first entry with that id byte to
    // its id position. Reserved ids leave the displaced entries behind.
    for (int id = 0; id < 0xFF; ++id) {
        int found = -1;
        for (int slot = 0; slot < 0xFF; ++slot) {
            if (packed[slot][0] == static_cast<std::uint8_t>(id)) {
                found = slot;
                break;
            }
        }
        if (found >= 0) {
            std::swap(packed[static_cast<std::size_t>(found)], packed[static_cast<std::size_t>(id)]);
        }
    }
    for (std::size_t i = 0; i < descriptors_.size(); ++i) {
        descriptors_[i].flags = packed[i][1];
        descriptors_[i].length = static_cast<std::uint16_t>(packed[i][2] | (packed[i][3] << 8));
    }
}

const GameMessageDescriptor& GameMessageTable::descriptor(std::uint8_t id) const noexcept {
    static const GameMessageDescriptor none{};
    if (id >= descriptors_.size()) {
        return none;
    }
    return descriptors_[id];
}

bool GameMessageTable::dispatchable(std::uint8_t id) noexcept {
    switch (id) {
    case 0x01: case 0x03: case 0x07: case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C:
    case 0x0D: case 0x0E: case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
    case 0x15: case 0x16: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D:
    case 0x1E: case 0x20: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x29:
    case 0x2B: case 0x2D: case 0x2E: case 0x30: case 0x33: case 0x34: case 0x35: case 0x36:
    case 0x37: case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E:
    case 0x3F: case 0x41: case 0x42: case 0x43: case 0x44: case 0x46: case 0x47: case 0x48:
    case 0x4A: case 0x4B: case 0x4D:
        return true;
    default:
        return false;
    }
}

void GameMessageTable::patch(std::uint32_t index, const std::uint8_t* bytes) {
    std::size_t first = static_cast<std::size_t>(index) * 32;
    if (first >= descriptors_.size()) {
        return;
    }
    std::size_t count = std::min<std::size_t>(32, descriptors_.size() - first);
    for (std::size_t i = 0; i < count; ++i) {
        descriptors_[first + i].flags = bytes[i * 4 + 1];
        descriptors_[first + i].length = load_u16(bytes, i * 4 + 2);
    }
}

// ---------------------------------------------------------------- queue ----

bool GameMessageQueue::enqueue(const GameMessageTable& table, std::uint8_t id,
                               const std::uint8_t* payload, std::uint16_t length) {
    if (id == 0) {
        return false;  // 0x46F3F2: id zero returns without a record
    }
    std::size_t slot = kGameMessageQueueSlots;
    for (std::size_t i = 0; i < kGameMessageQueueSlots; ++i) {
        if (records_[i].active == 0) {
            slot = i;
            break;
        }
    }
    if (slot == kGameMessageQueueSlots) {
        return false;  // 0x46F3F5..0x46F40E: full queue drops the message
    }
    if (length > kGameMessagePayloadSize) {
        return false;  // port guard: the original overruns into the next slot
    }
    GameMessageRecord& record = records_[slot];
    record = GameMessageRecord{};
    record.active = 1;
    record.length = length;
    record.id = id;
    record.sequence = static_cast<std::uint16_t>(sequence_counter_);
    ++sequence_counter_;
    if (sequence_counter_ > 32000) {
        sequence_counter_ = 1;  // 0x46F43B..0x46F443
    }
    const GameMessageDescriptor& descriptor = table.descriptor(id);
    if ((descriptor.flags & game_message_repeat_1001) != 0) {
        record.repeats = 0x3E9;
    } else if ((descriptor.flags & game_message_repeat_45) != 0) {
        record.repeats = 0x2D;
    } else {
        record.repeats = 1;
    }
    record.delay = send_delay_;  // 0x50321C low word copied at 0x46F465
    if (payload != nullptr && length != 0) {
        std::memcpy(record.payload.data(), payload, length);
    }
    last_slot_ = slot;
    return true;
}

void GameMessageQueue::step(GameMessageFrame& frame, const GameMessageTable& table,
                            std::vector<GameMessageSend>& sends) {
    frame.begin();  // 0x461250
    for (std::size_t slot = 0; slot < kGameMessageQueueSlots; ++slot) {
        GameMessageRecord& record = records_[slot];
        if (record.active == 0) {
            continue;
        }
        if (record.delay > 0) {
            --record.delay;  // 0x46F519 turns the delay down first
            continue;
        }
        if ((record.repeats & 0x01) != 0) {
            GameMessageSend send;
            send.id = record.id;
            send.sequence = record.sequence;
            send.length = record.length;
            send.payload = record.payload;
            sends.push_back(send);
            frame.append(table, record.id, record.sequence, record.payload.data(), record.length);
        }
        --record.repeats;  // 0x46F54B..0x46F557: even counts only decrement
        if (record.repeats == 0) {
            record = GameMessageRecord{};  // 0x46F55D: memset 0x264
        }
    }
}

void GameMessageQueue::compact() {
    // 0x46F4E3..0x46F5BB: move active records to the front, keep order.
    std::size_t target = 0;
    for (std::size_t slot = 0; slot < kGameMessageQueueSlots; ++slot) {
        if (records_[slot].active == 0) {
            continue;
        }
        if (target != slot) {
            records_[target] = records_[slot];
            records_[slot] = GameMessageRecord{};
        }
        ++target;
    }
}

std::size_t GameMessageQueue::cancel_sequence(std::uint16_t sequence) {
    // 0x46DB8D: clear every active record whose sequence matches.
    std::size_t cancelled = 0;
    for (GameMessageRecord& record : records_) {
        if (record.active != 0 && record.sequence == sequence) {
            record = GameMessageRecord{};
            ++cancelled;
        }
    }
    return cancelled;
}

std::size_t GameMessageQueue::active_count() const noexcept {
    std::size_t count = 0;
    for (const GameMessageRecord& record : records_) {
        if (record.active != 0) {
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------- frame ----

bool GameMessageFrame::append(const GameMessageTable& table, std::uint8_t id, std::uint16_t sequence,
                              const std::uint8_t* payload, std::uint16_t length) {
    const GameMessageDescriptor& descriptor = table.descriptor(id);
    bool valid = true;
    if ((descriptor.flags & kSelfLength) == 0 && descriptor.length != length) {
        // 0x4610F6..0x46112E: report the size and continue with the append.
        errors_.push_back(Error{id, length, descriptor.length});
        valid = false;
    }
    // 0x461131..0x461140: cursor + length + 0x10 > 0x2000 drops the message.
    if (bytes_.size() + length + 0x10 > kGameMessageFrameCapacity) {
        overflowed_ = true;
        return false;
    }
    bytes_.push_back(id);
    if ((descriptor.flags & kSequencedMask) != 0) {
        bytes_.push_back(static_cast<std::uint8_t>(sequence & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>(sequence >> 8));
    }
    if (payload != nullptr && length != 0) {
        bytes_.insert(bytes_.end(), payload, payload + length);
    }
    return valid;
}

// --------------------------------------------------------------- host -----

namespace { [[noreturn]] void missing_message_host(){throw std::runtime_error("Game message host body is not bound");} }
std::int32_t GameMessageHost::lookup_4828c0(std::int32_t) { missing_message_host(); }
std::uint32_t GameMessageHost::actor_record_base() { missing_message_host(); }
std::int32_t GameMessageHost::allocate_44a0c0(GameRecordArray, std::int32_t) { missing_message_host(); }
std::int32_t GameMessageHost::actor_record_dword(std::int32_t, std::uint32_t) { missing_message_host(); }
std::uint16_t GameMessageHost::actor_record_word(std::int32_t, std::uint32_t) { missing_message_host(); }
std::int32_t GameMessageHost::lookup_42cce0(std::int32_t) { missing_message_host(); }
std::int32_t GameMessageHost::terrain_height_47df40(std::uint32_t, std::uint32_t) { missing_message_host(); }
void GameMessageHost::sincos_477820(std::int32_t, std::int32_t&, std::int32_t&) {missing_message_host();}
std::int32_t GameMessageHost::weapon_list_46c180() { missing_message_host(); }
std::uint32_t GameMessageHost::session_counter_c2382c() { missing_message_host(); }
std::uint32_t GameMessageHost::current_weapon_item_c239b8_c239c0() { missing_message_host(); }
std::uint32_t GameMessageHost::weapon_aim_c23b70() { missing_message_host(); }
std::uint32_t GameMessageHost::weapon_aim_c23b74() { missing_message_host(); }
std::uint32_t GameMessageHost::weapon_table_field(std::uint32_t, std::uint32_t, std::uint32_t) {
    missing_message_host();
}
std::uint32_t GameMessageHost::class_table_field(std::uint32_t, std::uint32_t, std::uint32_t) {
    missing_message_host();
}
std::uint32_t GameMessageHost::slot_map_word(std::uint32_t) { missing_message_host(); }
std::int32_t GameMessageHost::record_field_110(GameRecordArray, std::int32_t) { missing_message_host(); }
void GameMessageHost::send_local_frame(const std::uint8_t*, std::size_t) {missing_message_host();}
std::size_t GameMessageHost::receive_local_frame(const std::uint8_t**) {missing_message_host();}
void GameMessageHost::weapons_tick_482b00() {missing_message_host();}

// ------------------------------------------------------------ dispatch ----

GameMessageDispatch::GameMessageDispatch(GameMessageTable& table, GameMessageQueue& queue,
                                         GameMessageHost* host)
    : table_(table), queue_(queue), host_(host) {}

GameMessageAction& GameMessageDispatch::start_action(GameMessageActionKind kind, std::uint8_t id) {
    actions_.push_back(GameMessageAction{});
    GameMessageAction& action = actions_.back();
    action.kind = kind;
    action.id = id;
    current_ = &action;
    return action;
}

void GameMessageDispatch::push_write(std::uint32_t offset, std::uint8_t width, std::uint32_t value) {
    if (current_ != nullptr) {
        current_->writes.push_back(GameRecordWrite{offset, width, value});
    }
}

void GameMessageDispatch::call_action(GameMessageActionKind kind, std::uint8_t id,
                                      std::uint32_t address) {
    GameMessageAction& action = start_action(kind, id);
    action.address = address;
}

void GameMessageDispatch::enqueue_followup(std::uint8_t id, const std::uint8_t* payload,
                                           std::uint16_t length) {
    if (queue_.enqueue(table_, id, payload, length)) {
        followups_.push_back(queue_.record(queue_.last_slot()));
    }
}

std::vector<GameMessageAction> GameMessageDispatch::dispatch(const std::uint8_t* frame,
                                                             std::size_t length) {    actions_.clear();
    followups_.clear();
    std::size_t at = 0;
    while (at < length) {
        const std::uint8_t id = frame[at];
        ++at;
        const GameMessageDescriptor descriptor = table_.descriptor(id);
        std::size_t available = length - at;
        std::uint16_t sequence = 0;
        bool duplicate = false;
        if ((descriptor.flags & kSequencedMask) != 0) {
            if (available < 2) {
                // Truncated frame: the original would read a stale sequence.
                start_action(GameMessageActionKind::undispatchable, id);
                return actions_;
            }
            sequence = load_u16(frame, at);
            at += 2;
            available -= 2;
            // 0x46DAF3: cancel pending repeats of this sequence first.
            const std::uint8_t seq_bytes[2] = {static_cast<std::uint8_t>(sequence & 0xFF),
                                               static_cast<std::uint8_t>(sequence >> 8)};
            enqueue_followup(0x14, seq_bytes, 2);
            // 0x46DAF8..0x46DB1D: 256 entry ring, newest first.
            duplicate = std::find(dedup_ring_.begin(), dedup_ring_.end(), sequence) !=
                        dedup_ring_.end();
            if (!duplicate) {
                for (std::size_t i = kGameMessageDedupSlots - 1; i > 0; --i) {
                    dedup_ring_[i] = dedup_ring_[i - 1];
                }
                dedup_ring_[0] = sequence;
            }
        }
        const std::uint8_t* payload = frame + at;
        const std::size_t self_length = (descriptor.flags & kSelfLength) != 0;
        std::size_t payload_length = descriptor.length;
        if (self_length) {
            if (available >= 2) {
                payload_length = load_u16(frame, at);
            } else {
                payload_length = 0;
            }
        }
        std::size_t advance = self_length ? payload_length : descriptor.length;

        if (duplicate) {
            // Sequenced replay: cancel was queued, no case runs.
            GameMessageAction& action = start_action(GameMessageActionKind::unknown, id);
            action.sequence = sequence;
            action.duplicate = true;
        } else if (!GameMessageTable::dispatchable(id)) {
            if (!self_length && descriptor.length == 0) {
                // The original would not advance the stream. Fail closed.
                GameMessageAction& action = start_action(GameMessageActionKind::undispatchable, id);
                action.sequence = sequence;
                return actions_;
            }
            GameMessageAction& action = start_action(GameMessageActionKind::unknown, id);
            action.sequence = sequence;
        } else {
            PayloadReader r(payload, available);
            switch (id) {
            case 0x01: {  // descriptor table patch, 128 bytes per message
                GameMessageAction& a = start_action(GameMessageActionKind::descriptor_patch, id);
                a.sequence = sequence;
                a.args[0] = static_cast<std::int32_t>(r.u32(0));
                a.bytes.resize(0x80);
                for (std::size_t i = 0; i < 0x80; ++i) {
                    a.bytes[i] = r.u8(4 + i);
                }
                table_.patch(static_cast<std::uint32_t>(a.args[0]), a.bytes.data());
                break;
            }
            case 0x03: {  // 0x8CF908 client state copy, then 0x486AF0(+0x74)
                GameMessageAction& a = start_action(GameMessageActionKind::client_state, id);
                a.sequence = sequence;
                a.address = 0x486AF0;
                a.bytes.resize(0x10C);
                for (std::size_t i = 0; i < 0x10C; ++i) {
                    a.bytes[i] = r.u8(i);
                }
                a.args[0] = static_cast<std::int32_t>(load_u32(a.bytes.data(), 0x74));
                break;
            }
            case 0x07: {  // 0x7CCA80 create, 0x45D6F0-less variant
                const std::int32_t index = r.i16(0);
                GameMessageAction& a = start_action(GameMessageActionKind::player_create, id);
                a.sequence = sequence;
                a.array = GameRecordArray::players_7cca80;
                a.index = index;
                if (index < 0 || index >= 256 ||
                    (index < static_cast<std::int32_t>(player_slots_.size()) &&
                     player_slots_[static_cast<std::size_t>(index)])) {
                    a.kind = GameMessageActionKind::mbp_error;
                    a.address = kDcbInUseErrorText;
                    break;
                }
                const std::int32_t handle = host_ ? host_->lookup_4828c0(r.i16(2)) : 0;
                const std::int32_t name = host_ ? host_->allocate_44a0c0(
                                                      GameRecordArray::players_7cca80, index)
                                                : 0;
                a.writes.push_back({0x00, 4, static_cast<std::uint32_t>(handle)});
                a.writes.push_back(
                    {0x04, 4, (host_ ? host_->actor_record_base() : kActorTableBase) + static_cast<std::uint32_t>(handle) * kActorTableStride});
                a.writes.push_back({0x08, 4, r.u32(4)});
                a.writes.push_back({0x0C, 4, static_cast<std::uint32_t>(name)});
                a.writes.push_back({0x24, 4, r.u32(0x1A)});
                a.writes.push_back({0x28, 4, r.u32(0x1E)});
                a.writes.push_back({0x2C, 4, r.u32(0x22)});
                a.writes.push_back({0x30, 4, r.u32(0x26)});
                a.writes.push_back({0x6C, 4, static_cast<std::uint32_t>(r.i32(0x16) >> 16)});
                a.writes.push_back({0x70, 2, r.u8(0x2A)});
                a.writes.push_back({0x98, 4, r.u8(0x2B)});
                a.writes.push_back({0x110, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x84) : 0)});
                a.writes.push_back({0x10C, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x90) : 0)});
                std::string name_text;
                r.string(8, name_text);
                a.bytes.assign(name_text.begin(), name_text.end());
                a.args[0] = index;
                a.args[1] = handle;
                a.args[2] = name;
                a.args[3] = r.u8(0x2C);  // written at name buffer + 0x64
                if (index >= static_cast<std::int32_t>(player_slots_.size())) {
                    player_slots_.resize(static_cast<std::size_t>(index) + 1, false);
                }
                player_slots_[static_cast<std::size_t>(index)] = true;
                if (max_players_ < static_cast<std::uint32_t>(index) + 1) {
                    max_players_ = static_cast<std::uint32_t>(index) + 1;  // 0x8CFA18
                }
                break;
            }
            case 0x08: {  // 0x7DDE80 create
                const std::int32_t index = r.i16(0);
                GameMessageAction& a = start_action(GameMessageActionKind::actor_create, id);
                a.sequence = sequence;
                a.array = GameRecordArray::actors_7dde80;
                a.index = index;
                if (index < 0 || index >= 256) {
                    a.kind = GameMessageActionKind::undispatchable;
                    break;
                }
                const std::int32_t handle = host_ ? host_->lookup_4828c0(r.i16(2)) : 0;
                a.writes.push_back({0x00, 4, static_cast<std::uint32_t>(handle)});
                a.writes.push_back(
                    {0x04, 4, (host_ ? host_->actor_record_base() : kActorTableBase) + static_cast<std::uint32_t>(handle) * kActorTableStride});
                a.writes.push_back({0x24, 4, r.u32(0x18)});
                a.writes.push_back({0x28, 4, r.u32(0x1C)});
                a.writes.push_back({0x2C, 4, r.u32(0x20)});
                a.writes.push_back({0x30, 4,
                                    static_cast<std::uint32_t>(r.i32(0x22) >> 16) << 16});
                a.writes.push_back({0x34, 4,
                                    static_cast<std::uint32_t>(r.i32(0x24) >> 16) << 16});
                a.writes.push_back({0x6C, 4, r.u32(0x14)});
                a.writes.push_back({0x70, 2, r.u8(0x28)});
                a.writes.push_back({0x110, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x84) : 0)});
                a.writes.push_back({0x10C, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x90) : 0)});
                if ((host_ ? host_->actor_record_dword(handle, 0x20) : 0) == 0xFEB) {
                    a.writes.push_back({0xE0, 4, r.u8(0x29)});
                }
                const bool wants_name =
                    (static_cast<std::uint32_t>(host_ ? host_->actor_record_dword(handle, 0x24) : 0) &
                     0x04) != 0;
                if (wants_name) {
                    const std::int32_t name = host_ ? host_->allocate_44a0c0(
                                                          GameRecordArray::actors_7dde80, index)
                                                    : 0;
                    a.writes.push_back({0x0C, 4, static_cast<std::uint32_t>(name)});
                    std::string name_text;
                    r.string(4, name_text);
                    a.bytes.assign(name_text.begin(), name_text.end());
                }
                a.args[0] = index;
                a.args[1] = handle;
                if (max_actors_ < static_cast<std::uint32_t>(index) + 1) {
                    max_actors_ = static_cast<std::uint32_t>(index) + 1;  // 0x8CFA1C
                }
                break;
            }
            case 0x09: {  // 0x462F20 projectile spawn
                const std::int32_t index = r.i16(8);
                std::uint32_t frac = ((r.u32(0) & 0xFF) << 16) | ((r.u32(4) & 0xFF) << 8);
                if ((frac & 0x800000) != 0) {
                    frac |= 0xFF000000;
                }
                const std::uint32_t x = r.u32(0) & 0xFFFFFF00;
                const std::uint32_t y = r.u32(4) & 0xFFFFFF00;
                const std::uint32_t terrain = host_ ? static_cast<std::uint32_t>(
                                                          host_->terrain_height_47df40(x, y))
                                                    : 0;
                if (index >= 0 && index < 256 &&
                    !(index < static_cast<std::int32_t>(player_slots_.size()) &&
                      player_slots_[static_cast<std::size_t>(index)])) {
                    const std::uint8_t slot_bytes[2] = {
                        static_cast<std::uint8_t>(r.u16(8) & 0xFF),
                        static_cast<std::uint8_t>(r.u16(8) >> 8)};
                    enqueue_followup(0x17, slot_bytes, 2);
                }
                GameMessageAction& a = start_action(GameMessageActionKind::projectile_spawn, id);
                a.sequence = sequence;
                a.array = GameRecordArray::players_7cca80;
                a.index = index;
                a.args[0] = static_cast<std::int32_t>(x);
                a.args[1] = static_cast<std::int32_t>(y);
                a.args[2] = static_cast<std::int32_t>(terrain + frac);
                a.args[3] = static_cast<std::int32_t>(static_cast<std::uint32_t>(r.u16(0x0A)) << 16);
                a.args[4] = static_cast<std::int32_t>(static_cast<std::uint32_t>(r.u16(0x0C)) << 16);
                a.args[5] = r.u8(0x0E);
                a.address = 0x462F20;
                break;
            }
            case 0x0B: {  // 0x470D50 world command
                GameMessageAction& a = start_action(GameMessageActionKind::world_command, id);
                a.sequence = sequence;
                a.address = 0x470D50;
                a.bytes.assign(payload, payload + payload_length);
                break;
            }
            case 0x0C: {  // 0x867E80 batch, 0x13 bytes per record
                const std::int32_t base = r.i16(0);
                const std::int32_t count = r.i16(2);
                for (std::int32_t i = 0; i < count; ++i) {
                    PayloadReader src(payload + static_cast<std::size_t>(i) * 0x13, available);
                    const std::int32_t index = base + i;
                    GameMessageAction& a =
                        start_action(GameMessageActionKind::actor_batch_create, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::squad_867e80;
                    a.index = index;
                    // 0x46E7C1 reads the high word of the dword at src + 2.
                    const std::int32_t handle = host_ ? host_->lookup_4828c0(src.i16(4)) : 0;
                    a.writes.push_back({0x00, 4, static_cast<std::uint32_t>(handle)});
                    a.writes.push_back({0x04, 4,
                                        (host_ ? host_->actor_record_base() : kActorTableBase) +
                                            static_cast<std::uint32_t>(handle) * kActorTableStride});
                    a.writes.push_back({0x24, 4, src.u32(8)});
                    a.writes.push_back({0x28, 4, src.u32(0x0C)});
                    a.writes.push_back({0x2C, 4, src.u32(0x10)});
                    a.writes.push_back(
                        {0x30, 4, static_cast<std::uint32_t>(src.i32(4) >> 16) << 16});
                    a.writes.push_back({0x6C, 4, src.u8(0x15)});
                    a.writes.push_back({0x70, 2, src.u8(0x14)});
                    a.writes.push_back({0x110, 4,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_dword(handle, 0x84) : 0)});
                    a.writes.push_back({0x10C, 4,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_dword(handle, 0x90) : 0)});
                    if (is_actor_type(static_cast<std::uint32_t>(
                            host_ ? host_->actor_record_dword(handle, 0x20) : 0))) {
                        a.writes.push_back({0xE0, 4, src.u8(0x16)});
                    }
                    a.args[0] = index;
                    a.args[1] = handle;
                }
                const std::uint32_t top = static_cast<std::uint32_t>(base + count);
                if (max_squad_ < top) {
                    max_squad_ = top;  // 0x8CF904
                }
                break;
            }
            case 0x0D: {
                flag_bc5d14_ = true;  // 0xBC5D14 = 1
                GameMessageAction& a = start_action(GameMessageActionKind::flag_d, id);
                a.sequence = sequence;
                break;
            }
            case 0x0E: {  // 0x7CCA80 update, index has bit 15 masked
                const std::int32_t index = r.u16(2) & 0x7FFF;
                GameMessageAction& a = start_action(GameMessageActionKind::player_move, id);
                a.sequence = sequence;
                a.array = GameRecordArray::players_7cca80;
                a.index = index;
                const bool occupied =
                    index < static_cast<std::int32_t>(player_slots_.size()) &&
                    player_slots_[static_cast<std::size_t>(index)];
                if (occupied) {
                    a.writes.push_back({0x24, 4, r.u32(4)});
                    a.writes.push_back({0x28, 4, r.u32(8)});
                    a.writes.push_back({0x2C, 4, r.u32(0x0C)});
                    a.writes.push_back(
                        {0x30, 4, static_cast<std::uint32_t>(r.i16(0)) << 16});
                    a.writes.push_back({0x6C, 1, r.u8(0x10)});
                    a.writes.push_back({0x7C, 4, r.u8(0x11)});
                } else {
                    const std::uint8_t slot_bytes[2] = {
                        static_cast<std::uint8_t>(r.u16(2) & 0xFF),
                        static_cast<std::uint8_t>(r.u16(2) >> 8)};
                    enqueue_followup(0x17, slot_bytes, 2);
                }
                break;
            }
            case 0x0F: {  // 0x7CCA80 move with terrain height
                const std::int32_t index = r.u8(1);
                GameMessageAction& a = start_action(GameMessageActionKind::actor_move, id);
                a.sequence = sequence;
                a.array = GameRecordArray::players_7cca80;
                a.index = index;
                const bool occupied =
                    index < static_cast<std::int32_t>(player_slots_.size()) &&
                    player_slots_[static_cast<std::size_t>(index)];
                if (occupied) {
                    std::uint32_t frac = ((r.u32(4) & 0xFF) << 16) | ((r.u32(8) & 0xFF) << 8);
                    if ((frac & 0x800000) != 0) {
                        frac |= 0xFF000000;
                    }
                    const std::uint32_t x = r.u32(4) & 0xFFFFFF00;
                    const std::uint32_t y = r.u32(8) & 0xFFFFFF00;
                    const std::uint32_t terrain =
                        host_ ? static_cast<std::uint32_t>(host_->terrain_height_47df40(x, y)) : 0;
                    a.writes.push_back({0x24, 4, x});
                    a.writes.push_back({0x28, 4, y});
                    a.writes.push_back({0x2C, 4, terrain + frac});
                    a.writes.push_back({0x30, 4, static_cast<std::uint32_t>(r.u8(0)) << 24});
                    a.writes.push_back({0x34, 4, static_cast<std::uint32_t>(r.u8(2)) << 24});
                    a.writes.push_back({0x6C, 1, r.u8(3)});
                    a.writes.push_back({0x7C, 4, r.u8(0x0C)});
                    a.writes.push_back({0x94, 4, static_cast<std::uint32_t>(r.u8(0x0D) & 0x3F)});
                    if ((r.u8(0x0D) & 0x40) != 0) {
                        a.writes.push_back({0x70, 2, 1});
                    } else if ((r.u8(0x0D) & 0x80) != 0) {
                        a.writes.push_back({0x70, 2, 2});
                    }
                } else {
                    const std::uint8_t slot_bytes[2] = {r.u8(1), 0};
                    enqueue_followup(0x17, slot_bytes, 2);
                }
                break;
            }
            case 0x10: {  // 0x45DA30 reset, single player
                if (single_player_) {
                    GameMessageAction& a =
                        start_action(GameMessageActionKind::player_reset, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::players_7cca80;
                    a.index = r.i16(0);
                    a.address = 0x45DA30;
                    a.args[0] = a.index;
                }
                break;
            }
            case 0x11: {  // single player weapon flag on the player record
                if (single_player_) {
                    const std::int32_t index = r.i16(0);
                    const bool occupied =
                        index >= 0 && index < static_cast<std::int32_t>(player_slots_.size()) &&
                        player_slots_[static_cast<std::size_t>(index)];
                    GameMessageAction& a =
                        start_action(GameMessageActionKind::player_weapon_flag, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::players_7cca80;
                    a.index = index;
                    if (occupied) {
                        a.writes.push_back({0x6C, 4, 0x04});  // OR 4; port reports the mask
                        a.writes.push_back(
                            {0x7C, 4, static_cast<std::uint32_t>(r.i32(0) >> 16)});
                        a.args[0] = 1;
                    } else {
                        a.args[0] = 0;
                    }
                }
                break;
            }
            case 0x12: {  // 0x479F00 HUD text, payload[0..1] is the total length
                GameMessageAction& a = start_action(GameMessageActionKind::hud_text, id);
                a.sequence = sequence;
                const std::uint8_t mode = r.u8(2);
                std::uint32_t color = 0xFF;
                switch (mode) {
                case 0: color = text_config_.color_67db40; break;
                case 1: case 4: case 5: color = text_config_.color_67db4c; break;
                case 2: color = text_config_.color_67db48; break;
                case 3: color = text_config_.color_67db50; break;
                default: color = 0xFF; break;
                }
                a.args[0] = mode;
                a.args[1] = static_cast<std::int32_t>(color);
                std::string text;
                r.string(3, text);
                a.bytes.assign(text.begin(), text.end());
                a.address = 0x479F00;
                break;
            }
            case 0x13: {  // local player respawn
                const std::int32_t index = r.i16(0);
                GameMessageAction& a = start_action(GameMessageActionKind::local_respawn, id);
                a.sequence = sequence;
                a.array = GameRecordArray::players_7cca80;
                a.index = index;
                a.address = 0x4848C0;
                a.args[0] = index;
                a.args[1] = single_player_ ? 1 : 0;
                if (index == local_player_index_) {
                    if (single_player_) {
                        a.writes.push_back({0x24, 4, r.u32(4)});
                        a.writes.push_back({0x28, 4, r.u32(8)});
                        a.writes.push_back({0x2C, 4, r.u32(0x0C)});
                        a.args[2] = r.i16(2);
                        a.args[3] = 1;  // 0x42CEF0(record, value, 0)
                    } else {
                        a.args[3] = 0;
                    }
                }
                break;
            }
            case 0x14: {  // cancel queue records with this sequence
                GameMessageAction& a = start_action(GameMessageActionKind::queue_cancel, id);
                a.sequence = sequence;
                a.args[0] = static_cast<std::int32_t>(queue_.cancel_sequence(r.u16(0)));
                break;
            }
            case 0x15: {  // scoreboard table
                GameMessageAction& a = start_action(GameMessageActionKind::scoreboard, id);
                a.sequence = sequence;
                scoreboard_field_bcfbe4_ = r.u8(0x8D);
                scoreboard_count_ = r.u8(0x8E);
                scoreboard_names_.clear();
                scoreboard_values_.clear();
                for (std::uint32_t i = 0; i < scoreboard_count_ && i < 64; ++i) {
                    const std::uint8_t dcb = r.u8(0x0D + i);
                    scoreboard_names_.push_back(std::string());
                    scoreboard_values_.push_back(static_cast<std::int32_t>(r.i32(0x4B + 2 * i) >> 16));
                    a.writes.push_back({0xBCF684 + i * 0x2C, 4,
                                        static_cast<std::uint32_t>(r.i32(0x4B + 2 * i) >> 16)});
                    a.writes.push_back({0xBCF688 + i * 0x2C, 4, dcb});
                    a.writes.push_back({0xBCF68C + i * 0x2C, 4, r.u8(0x2D + i)});
                }
                for (std::size_t i = 0; i < 3; ++i) {
                    scoreboard_pair_a_[i] = r.i16(2 * i);
                    scoreboard_pair_b_[i] = r.u8(9 + i);
                    scoreboard_pair_c_[i] = r.u8(6 + i);
                }
                a.args[0] = static_cast<std::int32_t>(scoreboard_count_);
                a.args[1] = static_cast<std::int32_t>(scoreboard_field_bcfbe4_);
                break;
            }
            case 0x16: {  // 0x7CCA80 replace with 0x45D6F0
                const std::int32_t index = r.i16(0);
                GameMessageAction& a = start_action(GameMessageActionKind::player_replace, id);
                a.sequence = sequence;
                a.array = GameRecordArray::players_7cca80;
                a.index = index;
                if (index < 0 || index >= 256) {
                    a.kind = GameMessageActionKind::undispatchable;
                    break;
                }
                const std::int32_t handle = host_ ? host_->lookup_4828c0(r.i16(2)) : 0;
                a.writes.push_back({0x00, 4, static_cast<std::uint32_t>(handle)});
                a.writes.push_back(
                    {0x04, 4, (host_ ? host_->actor_record_base() : kActorTableBase) + static_cast<std::uint32_t>(handle) * kActorTableStride});
                a.writes.push_back({0x08, 4, r.u32(4)});
                a.writes.push_back({0x10, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x6C) : 0)});
                a.writes.push_back({0x18, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->lookup_42cce0(
                                                    host_->actor_record_dword(handle, 0x74))
                                              : 0)});
                a.writes.push_back({0x24, 4, r.u32(0x1A)});
                a.writes.push_back({0x28, 4, r.u32(0x1E)});
                a.writes.push_back({0x2C, 4, r.u32(0x22)});
                a.writes.push_back({0x30, 4, r.u32(0x26)});
                a.writes.push_back({0x6C, 4, static_cast<std::uint32_t>(r.i32(0x16) >> 16)});
                a.writes.push_back({0x70, 2, r.u8(0x2A)});
                a.writes.push_back({0x74, 2,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_word(handle, 0x94) : 0)});
                a.writes.push_back({0x98, 4, r.u8(0x2B)});
                a.writes.push_back({0x110, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x84) : 0)});
                a.writes.push_back({0x10C, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x90) : 0)});
                const std::int32_t name = host_ ? host_->allocate_44a0c0(
                                                      GameRecordArray::players_7cca80, index)
                                                : 0;
                a.writes.push_back({0x0C, 4, static_cast<std::uint32_t>(name)});
                std::string name_text;
                r.string(8, name_text);
                a.bytes.assign(name_text.begin(), name_text.end());
                a.args[0] = index;
                a.args[1] = handle;
                a.args[2] = name;
                call_action(GameMessageActionKind::notify_45d6f0, id, 0x45D6F0);
                actions_.back().sequence = sequence;
                actions_.back().args[0] = handle;
                if (max_players_ < static_cast<std::uint32_t>(index) + 1) {
                    max_players_ = static_cast<std::uint32_t>(index) + 1;
                }
                break;
            }
            case 0x18: {  // 0x7CCA80 reset + state update
                const std::int32_t index = r.i16(0);
                call_action(GameMessageActionKind::reset_45da30, id, 0x45DA30);
                actions_.back().sequence = sequence;
                actions_.back().array = GameRecordArray::players_7cca80;
                actions_.back().index = index;
                actions_.back().args[0] = index;
                GameMessageAction& a = start_action(GameMessageActionKind::player_state, id);
                a.sequence = sequence;
                a.array = GameRecordArray::players_7cca80;
                a.index = index;
                if (index < 0 || index >= 256) {
                    a.kind = GameMessageActionKind::undispatchable;
                    break;
                }
                a.args[0] = r.u8(4);
                if (r.u8(4) != 0) {
                    const std::int32_t handle = host_ ? host_->lookup_4828c0(r.i16(2)) : 0;
                    a.writes.push_back({0x00, 4, static_cast<std::uint32_t>(handle)});
                    a.writes.push_back({0x04, 4,
                                        (host_ ? host_->actor_record_base() : kActorTableBase) +
                                            static_cast<std::uint32_t>(handle) * kActorTableStride});
                    a.writes.push_back({0x08, 4, r.u32(8)});
                    a.writes.push_back({0x10, 4,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_dword(handle, 0x6C) : 0)});
                    a.writes.push_back({0x24, 4, r.u32(0x1C)});
                    a.writes.push_back({0x28, 4, r.u32(0x20)});
                    a.writes.push_back({0x2C, 4, r.u32(0x24)});
                    a.writes.push_back(
                        {0x30, 4, static_cast<std::uint32_t>(r.i32(0x26) >> 16) << 16});
                    a.writes.push_back(
                        {0x34, 4, static_cast<std::uint32_t>(r.i32(0x28) >> 16) << 16});
                    a.writes.push_back({0x6C, 4, static_cast<std::uint32_t>(r.i32(4) >> 16)});
                    a.writes.push_back({0x70, 2, r.u8(5)});
                    a.writes.push_back({0x74, 2,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_word(handle, 0x94) : 0)});
                    a.writes.push_back({0x98, 4, r.u8(0x2C)});
                    a.writes.push_back({0x110, 4,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_dword(handle, 0x84) : 0)});
                    a.writes.push_back({0x10C, 4,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_dword(handle, 0x90) : 0)});
                    if ((static_cast<std::uint32_t>(
                             host_ ? host_->actor_record_dword(handle, 0x24) : 0) &
                         0x04) != 0) {
                        const std::int32_t name = host_ ? host_->allocate_44a0c0(
                                                              GameRecordArray::players_7cca80, index)
                                                        : 0;
                        a.writes.push_back({0x0C, 4, static_cast<std::uint32_t>(name)});
                        std::string name_text;
                        r.string(0x0C, name_text);
                        a.bytes.assign(name_text.begin(), name_text.end());
                    }
                    if ((host_ ? host_->actor_record_dword(handle, 0x28) : 0) == 3) {
                        a.writes.push_back(
                            {0x18, 4,
                             static_cast<std::uint32_t>(
                                 host_ ? host_->lookup_42cce0(
                                             host_->actor_record_dword(handle, 0x74))
                                       : 0)});
                        // 0x46EE30: the 0x8CFA18 update lives in this handler.
                        if (max_players_ < static_cast<std::uint32_t>(index) + 1) {
                            max_players_ = static_cast<std::uint32_t>(index) + 1;
                        }
                    }
                    a.args[1] = handle;
                    call_action(GameMessageActionKind::notify_45d6f0, id, 0x45D6F0);
                    actions_.back().sequence = sequence;
                    actions_.back().args[0] = handle;
                }
                break;
            }
            case 0x19: {
                flag_bc5d18_ = true;  // 0xBC5D18 = 1
                GameMessageAction& a = start_action(GameMessageActionKind::flag_19, id);
                a.sequence = sequence;
                break;
            }
            case 0x1A: {
                GameMessageAction& a = start_action(GameMessageActionKind::set_send_delay, id);
                a.sequence = sequence;
                a.args[0] = static_cast<std::int32_t>(r.u32(0));
                a.args[1] = single_player_ ? 1 : 0;
                if (single_player_) {
                    delay_50321c_ = r.u32(0);  // 0x50321C
                    queue_.set_send_delay(static_cast<std::uint16_t>(delay_50321c_ & 0xFFFF));
                }
                break;
            }
            case 0x1B: {
                GameMessageAction& a = start_action(GameMessageActionKind::set_value_67f548, id);
                a.sequence = sequence;
                a.args[0] = static_cast<std::int32_t>(r.u32(0));
                a.args[1] = single_player_ ? 1 : 0;
                if (single_player_) {
                    value_67f548_ = r.u32(0);  // 0x67F548
                }
                break;
            }
            case 0x1C: {
                GameMessageAction& a = start_action(GameMessageActionKind::local_death, id);
                a.sequence = sequence;
                death_bcfc2c_ = r.i16(0);      // 0xBCFC2C
                death_bcfc30_ = r.i32(0) >> 16;  // 0xBCFC30
                death_bcfc34_ = r.u8(6);       // 0xBCFC34
                a.args[0] = static_cast<std::int32_t>(death_bcfc2c_);
                a.args[1] = static_cast<std::int32_t>(death_bcfc30_);
                a.args[2] = static_cast<std::int32_t>(death_bcfc34_);
                if (local_player_index_ >= 0) {
                    a.array = GameRecordArray::players_7cca80;
                    a.index = local_player_index_;
                    a.writes.push_back({0x6C, 1, r.u8(4)});
                    if (r.u8(5) == 0xFF) {
                        a.writes.push_back({0x90, 4, 0});
                    } else {
                        a.writes.push_back(
                            {0x90, 4,
                             static_cast<std::uint32_t>(
                                 kActorDcbArray +
                                 static_cast<std::uint32_t>(r.u8(5)) * kGameRecordSize)});
                        if (flag_67f53c_ == 5 && r.u8(5) != 0xFF) {
                            a.args[3] = 1;  // 0x484ED0 branch is host wiring
                        }
                    }
                }
                break;
            }
            case 0x1D: {
                GameMessageAction& a = start_action(GameMessageActionKind::mission_state, id);
                a.sequence = sequence;
                a.address = 0x4848C0;
                a.args[0] = static_cast<std::int32_t>(text_config_.mission_config_67db5c);
                break;
            }
            case 0x1E: {
                GameMessageAction& a = start_action(GameMessageActionKind::world_1e, id);
                a.sequence = sequence;
                a.address = 0x471010;
                a.bytes.assign(payload, payload + payload_length);
                break;
            }
            case 0x20: {
                GameMessageAction& a = start_action(GameMessageActionKind::world_20, id);
                a.sequence = sequence;
                a.address = 0x470400;
                a.bytes.assign(payload, payload + payload_length);
                break;
            }
            case 0x23: {
                control_bcfc14_ = r.u32(0);  // 0xBCFC14
                const std::uint32_t values[3] = {r.u32(4), r.u32(8), r.u32(0x0C)};
                const std::uint32_t addresses[3] = {0x455AF0, 0x455B30, 0x455B10};
                for (int i = 0; i < 3; ++i) {
                    call_action(GameMessageActionKind::control_flags, id, addresses[i]);
                    actions_.back().sequence = sequence;
                    actions_.back().args[0] = static_cast<std::int32_t>(values[i]);
                }
                break;
            }
            case 0x24: {
                GameMessageAction& a = start_action(GameMessageActionKind::set_net_flag, id);
                a.sequence = sequence;
                a.args[0] = r.i16(0);
                a.args[1] = single_player_ ? 1 : 0;
                if (single_player_) {
                    net_flag_67dbe8_ = static_cast<std::uint32_t>(r.i16(0) * 1000) >> 4;
                }
                break;
            }
            case 0x25: {
                GameMessageAction& a = start_action(GameMessageActionKind::entity_action, id);
                a.sequence = sequence;
                a.address = 0x43F5A0;
                a.array = GameRecordArray::players_7cca80;
                a.index = r.u8(0);
                a.args[0] = a.index;
                a.args[1] = r.u8(1);
                break;
            }
            case 0x26: {  // 0x8ACE80 batch, 0x17 bytes per record
                const std::int32_t base = r.i16(0);
                const std::int32_t count = r.i16(2);
                for (std::int32_t i = 0; i < count; ++i) {
                    PayloadReader src(payload + static_cast<std::size_t>(i) * 0x17, available);
                    const std::int32_t index = base + i;
                    GameMessageAction& a =
                        start_action(GameMessageActionKind::effect_batch_create, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::effects_8ace80;
                    a.index = index;
                    // 0x46E8F4 reads the high word of the dword at src + 2.
                    const std::int32_t handle = host_ ? host_->lookup_4828c0(src.i16(4)) : 0;
                    a.writes.push_back({0x00, 4, static_cast<std::uint32_t>(handle)});
                    a.writes.push_back({0x04, 4,
                                        (host_ ? host_->actor_record_base() : kActorTableBase) +
                                            static_cast<std::uint32_t>(handle) * kActorTableStride});
                    a.writes.push_back({0x24, 4, src.u32(6)});
                    a.writes.push_back({0x28, 4, src.u32(0x0A)});
                    a.writes.push_back({0x2C, 4, src.u32(0x0E)});
                    a.writes.push_back({0x30, 4, src.u32(0x12) << 16});
                    a.writes.push_back({0x70, 2, src.u8(0x16)});
                    a.writes.push_back({0x7C, 4, src.u32(0x17)});
                    a.writes.push_back({0x110, 4,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_dword(handle, 0x84) : 0)});
                    a.writes.push_back({0x10C, 4,
                                        static_cast<std::uint32_t>(
                                            host_ ? host_->actor_record_dword(handle, 0x90) : 0)});
                    a.args[0] = index;
                    a.args[1] = handle;
                }
                const std::uint32_t top = static_cast<std::uint32_t>(base + count);
                if (max_effects_ < top) {
                    max_effects_ = top;  // 0x8CF900
                }
                break;
            }
            case 0x27: {  // local movement segment call
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::hud_mode, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::players_7cca80;
                    a.index = r.i16(0);
                    a.args[0] = static_cast<std::int32_t>(r.u32(2));
                    a.args[1] = static_cast<std::int32_t>(r.u32(6));
                    a.args[2] = static_cast<std::int32_t>(r.u32(0x0A));
                    a.args[3] = static_cast<std::int32_t>(r.i32(0x0C) >> 16) << 16;
                    const std::uint8_t mode = r.u8(0x10);
                    a.args[4] = mode;
                    a.args[5] = r.u8(0x11);
                    a.address = mode == 0 ? 0x4606B0 : (mode == 1 ? 0x4607E0 : 0x460950);
                }
                break;
            }
            case 0x29: {  // 0x7DDE80 state update
                const std::int32_t index = r.i16(0);
                const bool occupied =
                    index >= 0 && index < static_cast<std::int32_t>(actor_slots_.size()) &&
                    actor_slots_[static_cast<std::size_t>(index)];
                if (occupied) {
                    GameMessageAction& a = start_action(GameMessageActionKind::actor_state, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::actors_7dde80;
                    a.index = index;
                    a.writes.push_back({0x24, 4, r.u32(2)});
                    a.writes.push_back({0x28, 4, r.u32(6)});
                    a.writes.push_back({0x2C, 4, r.u32(0x0A)});
                    a.writes.push_back({0x30, 4, static_cast<std::uint32_t>(r.u8(0x0E)) << 24});
                    a.writes.push_back({0x6C, 1, r.u8(0x0F)});
                } else {
                    GameMessageAction& a = start_action(GameMessageActionKind::unknown, id);
                    a.sequence = sequence;
                    a.index = index;
                    a.args[0] = 0;
                }
                break;
            }
            case 0x2B: {  // 0x7DDE80 create with dcb link and sounds
                const std::int32_t index = r.i16(0);
                GameMessageAction& a = start_action(GameMessageActionKind::actor_place, id);
                a.sequence = sequence;
                a.array = GameRecordArray::actors_7dde80;
                a.index = index;
                if (index < 0 || index >= 256) {
                    a.kind = GameMessageActionKind::undispatchable;
                    break;
                }
                const std::int32_t handle = host_ ? host_->lookup_4828c0(r.i16(2)) : 0;
                a.writes.push_back({0x00, 4, static_cast<std::uint32_t>(handle)});
                a.writes.push_back(
                    {0x04, 4, (host_ ? host_->actor_record_base() : kActorTableBase) + static_cast<std::uint32_t>(handle) * kActorTableStride});
                a.writes.push_back({0x10, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x6C) : 0)});
                a.writes.push_back({0x24, 4, r.u32(0x18)});
                a.writes.push_back({0x28, 4, r.u32(0x1C)});
                a.writes.push_back({0x2C, 4, r.u32(0x20)});
                a.writes.push_back({0x30, 4,
                                    static_cast<std::uint32_t>(r.i32(0x22) >> 16) << 16});
                a.writes.push_back({0x34, 4,
                                    static_cast<std::uint32_t>(r.i32(0x24) >> 16) << 16});
                a.writes.push_back({0x6C, 4, r.u32(0x14)});
                a.writes.push_back({0x70, 2, r.u8(0x2A)});
                const std::uint16_t link = r.u16(0x28);
                if (link == 0xFFFF) {
                    a.writes.push_back({0xF8, 4, 0});
                    a.args[1] = -1;
                } else {
                    a.writes.push_back(
                        {0xF8, 4,
                         static_cast<std::uint32_t>(
                             kPlayerDcbArray + static_cast<std::uint32_t>(link) * kGameRecordSize)});
                    a.args[1] = link;
                }
                a.writes.push_back({0x110, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x84) : 0)});
                a.writes.push_back({0x10C, 4,
                                    static_cast<std::uint32_t>(
                                        host_ ? host_->actor_record_dword(handle, 0x90) : 0)});
                if ((static_cast<std::uint32_t>(
                         host_ ? host_->actor_record_dword(handle, 0x24) : 0) &
                     0x04) != 0) {
                    const std::int32_t name = host_ ? host_->allocate_44a0c0(
                                                          GameRecordArray::actors_7dde80, index)
                                                    : 0;
                    a.writes.push_back({0x0C, 4, static_cast<std::uint32_t>(name)});
                    std::string name_text;
                    r.string(4, name_text);
                    a.bytes.assign(name_text.begin(), name_text.end());
                }
                const std::uint16_t type = r.u16(2);
                if (type == 0xFAE || type == 0xFC8) {
                    call_action(GameMessageActionKind::actor_place, id, 0x43E7D0);
                    actions_.back().sequence = sequence;
                    actions_.back().args[0] = 0x78;
                    actions_.back().args[1] = static_cast<std::int32_t>(r.u32(0x18));
                    actions_.back().args[2] = static_cast<std::int32_t>(r.u32(0x1C));
                    actions_.back().args[3] = static_cast<std::int32_t>(r.u32(0x20));
                }
                if (type == 0xFB4) {
                    call_action(GameMessageActionKind::actor_place, id, 0x43E7D0);
                    actions_.back().sequence = sequence;
                    actions_.back().args[0] = 0x79;
                    actions_.back().args[1] = static_cast<std::int32_t>(r.u32(0x18));
                    actions_.back().args[2] = static_cast<std::int32_t>(r.u32(0x1C));
                    actions_.back().args[3] = static_cast<std::int32_t>(r.u32(0x20));
                }
                a.args[0] = index;
                if (max_actors_ < static_cast<std::uint32_t>(index) + 1) {
                    max_actors_ = static_cast<std::uint32_t>(index) + 1;
                }
                break;
            }
            case 0x2D: {
                GameMessageAction& a = start_action(GameMessageActionKind::mission_ok, id);
                a.sequence = sequence;
                a.address = 0x43E720;
                a.args[0] = 0x1AF;
                a.args[1] = static_cast<std::int32_t>(text_config_.request_config_67db54);
                break;
            }
            case 0x2E: {
                GameMessageAction& a = start_action(GameMessageActionKind::mission_denied, id);
                a.sequence = sequence;
                a.address = 0x43E720;
                a.args[0] = 0x1B0;
                a.args[1] = static_cast<std::int32_t>(text_config_.request_config_67db54);
                break;
            }
            case 0x30: {
                byte_c23b78_ = r.u8(0);  // 0xC23B78
                GameMessageAction& a = start_action(GameMessageActionKind::set_byte_c23b78, id);
                a.sequence = sequence;
                a.args[0] = static_cast<std::int32_t>(byte_c23b78_);
                break;
            }
            case 0x33: {
                GameMessageAction& a = start_action(GameMessageActionKind::set_flag_67dbbc, id);
                a.sequence = sequence;
                a.args[0] = 4;
                a.args[1] = single_player_ ? 1 : 0;
                if (single_player_) {
                    flag_67dbbc_ = 4;  // 0x67DBBC
                }
                break;
            }
            case 0x34: case 0x35: case 0x36: case 0x37: case 0x38: {  // actor updates
                const std::int32_t index = r.i16(0);
                const bool occupied =
                    index >= 0 && index < static_cast<std::int32_t>(actor_slots_.size()) &&
                    actor_slots_[static_cast<std::size_t>(index)];
                if (!occupied) {
                    GameMessageAction& a = start_action(GameMessageActionKind::unknown, id);
                    a.sequence = sequence;
                    a.index = index;
                    a.args[0] = 0;
                    break;
                }
                const GameMessageActionKind kind = id == 0x34   ? GameMessageActionKind::actor_update_34
                                                   : id == 0x35 ? GameMessageActionKind::actor_update_35
                                                   : id == 0x36 ? GameMessageActionKind::actor_update_36
                                                   : id == 0x37 ? GameMessageActionKind::actor_update_37
                                                                : GameMessageActionKind::actor_update_38;
                GameMessageAction& a = start_action(kind, id);
                a.sequence = sequence;
                a.array = GameRecordArray::actors_7dde80;
                a.index = index;
                const std::uint32_t base24 = static_cast<std::uint32_t>(r.u8(0x0E)) << 24;
                a.writes.push_back({0x24, 4, r.u32(2)});
                a.writes.push_back({0x28, 4, r.u32(6)});
                a.writes.push_back({0x2C, 4, r.u32(0x0A)});
                a.writes.push_back({0x30, 4, base24});
                a.writes.push_back({0x6C, 1, r.u8(0x0F)});
                if (id == 0x34) {
                    a.writes.push_back({0x7C, 4, static_cast<std::uint32_t>(r.i32(0x0E) >> 16) << 8});
                    a.writes.push_back({0x88, 4, static_cast<std::uint32_t>(r.i32(0x10) >> 16) << 8});
                    a.writes.push_back({0x8C, 4, static_cast<std::uint32_t>(r.u8(0x14)) << 24});
                } else if (id == 0x35) {
                    a.writes.push_back({0x7C, 4, static_cast<std::uint32_t>(r.i32(0x0E) >> 16) << 8});
                    a.writes.push_back({0x88, 4, static_cast<std::uint32_t>(r.i32(0x10) >> 16) << 8});
                    a.writes.push_back({0x8C, 4, static_cast<std::uint32_t>(r.u8(0x14)) << 24});
                    a.writes.push_back({0x98, 4, static_cast<std::uint32_t>(r.u8(0x15)) << 22});
                    a.writes.push_back({0xA0, 4, static_cast<std::uint32_t>(r.u8(0x15)) << 22});
                    a.writes.push_back({0xA8, 4, r.u8(0x16)});
                    a.writes.push_back({0xAC, 4, static_cast<std::uint32_t>(r.i32(0x16) >> 24) << 4});
                    a.writes.push_back({0xB0, 4, static_cast<std::uint32_t>(r.i32(0x17) >> 24) << 4});
                    a.writes.push_back({0xCC, 4, static_cast<std::uint32_t>(r.u8(0x17)) << 24});
                    a.writes.push_back({0xD0, 4, static_cast<std::uint32_t>(r.u8(0x18)) << 24});
                    a.writes.push_back({0xD4, 4, r.u8(0x1B)});
                } else if (id == 0x36) {
                    a.writes.push_back({0x7C, 4, static_cast<std::uint32_t>(r.i32(0x0E) >> 16) << 8});
                    a.writes.push_back({0x88, 4, static_cast<std::uint32_t>(r.i32(0x10) >> 16) << 8});
                    a.writes.push_back({0x8C, 4, static_cast<std::uint32_t>(r.u8(0x1A)) << 24});
                    a.writes.push_back({0xE8, 4, r.u8(0x1B)});
                    a.writes.push_back({0xEC, 4, r.u32(0x14)});
                    a.writes.push_back({0xF0, 4, static_cast<std::uint32_t>(r.i32(0x16) >> 16) << 4});
                    // 0x477820(record + 0x30 = byte 0x0E << 24, &out1, &out2)
                    std::int32_t value_40 = 0;
                    std::int32_t value_3c = 0;
                    if (host_) {
                        host_->sincos_477820(static_cast<std::int32_t>(base24), value_40, value_3c);
                    }
                    const std::int32_t scale = static_cast<std::int32_t>(r.i32(0x0E) >> 16) << 8;
                    // 0x46E156: imul, shrd 22, sar 6 (two step like the original).
                    const std::int64_t product_3c = static_cast<std::int64_t>(scale) * value_3c;
                    const std::int64_t product_40 = static_cast<std::int64_t>(scale) * value_40;
                    a.writes.push_back(
                        {0x3C, 4, static_cast<std::uint32_t>(
                                      static_cast<std::int32_t>(
                                          static_cast<std::uint32_t>(product_3c >> 22)) >>
                                      6)});
                    a.writes.push_back(
                        {0x40, 4, static_cast<std::uint32_t>(
                                      static_cast<std::int32_t>(
                                          static_cast<std::uint32_t>(product_40 >> 22)) >>
                                      6)});
                    break;
                } else if (id == 0x37) {
                    a.writes.push_back({0xE0, 4, static_cast<std::uint32_t>(r.i32(0x0E) >> 16)});
                    a.writes.push_back({0xE4, 4, static_cast<std::uint32_t>(r.i32(0x10) >> 16)});
                    a.writes.push_back({0xF0, 4, r.u8(0x14)});
                } else if (id == 0x38) {
                    a.writes.push_back({0x8C, 4, static_cast<std::uint32_t>(r.u8(0x10)) << 24});
                    a.writes.push_back({0xEC, 4, static_cast<std::uint32_t>(r.i32(0x10) >> 16) << 8});
                    a.writes.push_back({0xF0, 4, static_cast<std::uint32_t>(r.u8(0x11)) << 24});
                }
                break;
            }
            case 0x39: {  // local entity callback through record + 0x110
                if (single_player_) {
                    const std::int32_t index = r.i16(0);
                    entity_callback_a158c0_ = static_cast<std::uint32_t>(r.i32(0) >> 16);
                    GameMessageAction& a =
                        start_action(GameMessageActionKind::entity_callback, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::players_7cca80;
                    a.index = index;
                    a.args[0] = static_cast<std::int32_t>(entity_callback_a158c0_);
                    a.args[1] = 4;
                    const std::int32_t target = host_ ? host_->record_field_110(
                                                            GameRecordArray::players_7cca80, index)
                                                      : 0;
                    if (target != 0) {
                        a.address = static_cast<std::uint32_t>(target);
                    } else {
                        a.kind = GameMessageActionKind::unknown;
                        a.args[2] = 0;
                    }
                }
                break;
            }
            case 0x3A: {  // local sound pair
                if (single_player_) {
                    // 0x46E323: 45DC70(i32@0x0A >> 16, u32@0, u32@4, u32@8)
                    call_action(GameMessageActionKind::sound_pair, id, 0x45DC70);
                    actions_.back().sequence = sequence;
                    actions_.back().args[0] = r.i32(0x0A) >> 16;
                    actions_.back().args[1] = static_cast<std::int32_t>(r.u32(0));
                    actions_.back().args[2] = static_cast<std::int32_t>(r.u32(4));
                    actions_.back().args[3] = static_cast<std::int32_t>(r.u32(8));
                    // 0x46E341: 43E7D0(i32@0x0C >> 16, u32@0, u32@4, u32@8)
                    call_action(GameMessageActionKind::sound_pair, id, 0x43E7D0);
                    actions_.back().sequence = sequence;
                    actions_.back().args[0] = r.i32(0x0C) >> 16;
                    actions_.back().args[1] = static_cast<std::int32_t>(r.u32(0));
                    actions_.back().args[2] = static_cast<std::int32_t>(r.u32(4));
                    actions_.back().args[3] = static_cast<std::int32_t>(r.u32(8));
                }
                break;
            }
            case 0x3B: {  // actor update keyed by 0x0A high word
                const std::int32_t index = r.i32(0x0A) >> 16;
                const bool occupied =
                    index >= 0 && index < static_cast<std::int32_t>(actor_slots_.size()) &&
                    actor_slots_[static_cast<std::size_t>(index)];
                if (occupied) {
                    GameMessageAction& a = start_action(GameMessageActionKind::actor_state_dcb, id);
                    a.sequence = sequence;
                    a.array = GameRecordArray::actors_7dde80;
                    a.index = index;
                    a.writes.push_back({0x24, 4, r.u32(0)});
                    a.writes.push_back({0x28, 4, r.u32(4)});
                    a.writes.push_back({0x2C, 4, r.u32(8)});
                    a.writes.push_back({0x30, 4, static_cast<std::uint32_t>(r.u8(0x0E)) << 24});
                    a.writes.push_back({0x6C, 1, r.u8(0x0F)});
                    a.writes.push_back({0x8C, 4, static_cast<std::uint32_t>(r.u8(0x10)) << 24});
                    a.writes.push_back({0xF0, 4, static_cast<std::uint32_t>(r.u8(0x11))});
                }
                break;
            }
            case 0x3C: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::local_text, id);
                    a.sequence = sequence;
                    a.address = 0x4560C0;
                    a.args[0] = r.i32(6) >> 16;
                    a.bytes.assign(payload, payload + payload_length);
                }
                break;
            }
            case 0x3D: {
                GameMessageAction& a = start_action(GameMessageActionKind::copy_blob, id);
                a.sequence = sequence;
                a.bytes.resize(0x100);
                for (std::size_t i = 0; i < 0x100; ++i) {
                    a.bytes[i] = r.u8(i);
                }
                std::memcpy(blob_50352c_.data(), a.bytes.data(), 0x100);
                break;
            }
            case 0x3E: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::input_ints, id);
                    a.sequence = sequence;
                    a.address = 0x471570;
                    a.args[0] = r.u8(0);
                    a.args[1] = r.u8(1);
                    a.args[2] = r.u8(2);
                }
                break;
            }
            case 0x3F: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::input_blob, id);
                    a.sequence = sequence;
                    a.address = 0x4715B0;
                    a.bytes.assign(payload, payload + payload_length);
                }
                break;
            }
            case 0x41: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::input_none, id);
                    a.sequence = sequence;
                    a.address = 0x471600;
                }
                break;
            }
            case 0x42: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::session_strings, id);
                    a.sequence = sequence;
                    session_string_bcf624_.clear();
                    r.string(0, session_string_bcf624_);
                    mission_file_67f438_.clear();
                    r.string(0x20, mission_file_67f438_);
                    a.bytes.assign(session_string_bcf624_.begin(), session_string_bcf624_.end());
                    a.args[0] = static_cast<std::int32_t>(mission_file_67f438_.size());
                }
                break;
            }
            case 0x43: {
                GameMessageAction& a = start_action(GameMessageActionKind::input_43, id);
                a.sequence = sequence;
                a.address = 0x471760;
                a.bytes.assign(payload, payload + payload_length);
                break;
            }
            case 0x44: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::hud_44, id);
                    a.sequence = sequence;
                    a.address = 0x479FA0;
                    a.args[0] = 3;
                }
                break;
            }
            case 0x46: {
                GameMessageAction& a = start_action(GameMessageActionKind::input_46, id);
                a.sequence = sequence;
                a.address = 0x471810;
                a.bytes.assign(payload, payload + payload_length);
                break;
            }
            case 0x47: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::hud_47, id);
                    a.sequence = sequence;
                    a.address = 0x479FA0;
                    a.args[0] = 0x2E;
                }
                break;
            }
            case 0x48: {
                GameMessageAction& a = start_action(GameMessageActionKind::input_48, id);
                a.sequence = sequence;
                a.address = 0x471870;
                a.bytes.assign(payload, payload + payload_length);
                break;
            }
            case 0x4A: {
                if (single_player_) {
                    GameMessageAction& a =
                        start_action(GameMessageActionKind::weapon_list_request, id);
                    a.sequence = sequence;
                    a.args[0] = host_ ? host_->weapon_list_46c180() : 0;
                    a.args[1] = static_cast<std::int32_t>(
                        host_ ? host_->session_counter_c2382c() >> 8 : 0);
                    const std::uint8_t report[8] = {
                        static_cast<std::uint8_t>(a.args[0] & 0xFF),
                        static_cast<std::uint8_t>((a.args[0] >> 8) & 0xFF),
                        static_cast<std::uint8_t>((a.args[0] >> 16) & 0xFF),
                        static_cast<std::uint8_t>((a.args[0] >> 24) & 0xFF),
                        static_cast<std::uint8_t>(a.args[1] & 0xFF),
                        static_cast<std::uint8_t>((a.args[1] >> 8) & 0xFF),
                        static_cast<std::uint8_t>((a.args[1] >> 16) & 0xFF),
                        static_cast<std::uint8_t>((a.args[1] >> 24) & 0xFF)};
                    enqueue_followup(0x50, report, 8);
                }
                break;
            }
            case 0x4B: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::slot_report, id);
                    a.sequence = sequence;
                    const std::uint8_t weapon = r.u8(0);
                    const std::uint32_t class_byte =
                        host_ ? host_->weapon_table_field(weapon, 0x74, 1) : 0;
                    const std::uint32_t fire_delay =
                        host_ ? host_->weapon_table_field(weapon, 0x9C, 2) : 0;
                    const std::uint32_t burst = host_ ? host_->weapon_table_field(weapon, 0xA0, 1) : 0;
                    const std::uint32_t flags = host_ ? host_->weapon_table_field(weapon, 0xA4, 2) : 0;
                    std::uint32_t slot_sum = 0;
                    for (std::uint32_t i = 0; i < 10; ++i) {
                        slot_sum += host_ ? (host_->slot_map_word(i) & 0xFFFF) : 0;
                    }
                    const std::uint8_t report[9] = {
                        weapon,
                        static_cast<std::uint8_t>(class_byte & 0xFF),
                        static_cast<std::uint8_t>(fire_delay & 0xFF),
                        static_cast<std::uint8_t>((fire_delay >> 8) & 0xFF),
                        static_cast<std::uint8_t>(burst & 0xFF),
                        static_cast<std::uint8_t>(flags & 0xFF),
                        static_cast<std::uint8_t>((flags >> 8) & 0xFF),
                        static_cast<std::uint8_t>(slot_sum & 0xFF),
                        static_cast<std::uint8_t>((slot_sum >> 8) & 0xFF)};
                    enqueue_followup(0x4C, report, 9);
                    a.args[0] = weapon;
                    a.args[1] = static_cast<std::int32_t>(class_byte);
                    a.args[2] = static_cast<std::int32_t>(fire_delay);
                    a.args[3] = static_cast<std::int32_t>(burst);
                    a.args[4] = static_cast<std::int32_t>(flags);
                    a.args[5] = static_cast<std::int32_t>(slot_sum);
                }
                break;
            }
            case 0x4D: {
                if (single_player_) {
                    GameMessageAction& a = start_action(GameMessageActionKind::class_report, id);
                    a.sequence = sequence;
                    const std::uint8_t weapon_class = r.u8(0);
                    const std::uint32_t value =
                        host_ ? host_->class_table_field(weapon_class, 8, 2) : 0;
                    // 0x46F384: payload = class word at [0..1], class byte at [2]
                    const std::uint8_t report[3] = {
                        static_cast<std::uint8_t>(value & 0xFF),
                        static_cast<std::uint8_t>((value >> 8) & 0xFF), weapon_class};
                    enqueue_followup(0x4E, report, 3);
                    a.args[0] = weapon_class;
                    a.args[1] = static_cast<std::int32_t>(value);
                }
                break;
            }
            default:
                break;
            }
        }
        at += advance;  // post-switch skip: table length or self length
        if (at > length) {
            at = length;  // malformed trailing message clamps instead of under-running
        }
    }
    current_ = nullptr;
    return actions_;
}

// --------------------------------------------------------------- stage ----

GameMessageStage::GameMessageStage() : dispatch_(table_, queue_, &default_host_) {
    dispatch_.set_local_player_index(-1);
    dispatch_.set_single_player(true);
}

void GameMessageStage::set_host(GameMessageHost* host) noexcept {
    host_ = host != nullptr ? host : &default_host_;
    dispatch_.set_host(host_);
}

void GameMessageStage::set_single_player(bool enabled) noexcept {
    single_player_ = enabled;
}

void GameMessageStage::set_networked(bool enabled) noexcept { networked_ = enabled;dispatch_.set_single_player(!enabled); }

void GameMessageStage::set_local_player_index(std::int32_t index) noexcept {
    dispatch_.set_local_player_index(index);
}

void GameMessageStage::set_flag_67f53c(std::uint32_t value) noexcept {
    dispatch_.set_flag_67f53c(value);
}

void GameMessageStage::set_text_config(const GameMessageTextConfig& config) noexcept {
    dispatch_.set_text_config(config);
}

std::vector<GameMessageAction> GameMessageStage::pump_receive() {
    std::vector<GameMessageAction> actions;
    while(check_receive()){
        auto dispatched=dispatch_received();actions.insert(actions.end(),dispatched.begin(),dispatched.end());
        update_cell()=0;
    }
    return actions;
}
bool GameMessageStage::check_receive(){
    if(dispatching_)throw std::runtime_error("Message receive cannot replace an active dispatch frame");
    if(received_pending_)throw std::runtime_error("Dispatch the current received frame before checking again");
    if(source_global(0x67f528)||!source_global(0x67f520)){
        if(local_echo_.empty())return false;
        received_=std::move(local_echo_);local_echo_.clear();
    }else{
        const std::uint8_t* frame = nullptr;
        const auto length=host_->receive_local_frame(&frame);
        if(!length)return false;
        if(!frame)throw std::runtime_error("Received message frame has no bytes");
        received_.assign(frame,frame+length);
    }
    received_pending_=true;return true;
}
std::uint32_t GameMessageStage::source_global(std::uint32_t address) const{
    if(!inspection_){if(!live_.read_global)missing_message_host();return live_.read_global(address);}
    if(address==0x67f520)return single_player_?0:1;
    if(address==0x67f528)return networked_?1:0;
    if(address==0x67f538)return tick_limit_;
    throw std::runtime_error("Inspection source global is not owned");
}
std::uint32_t& GameMessageStage::update_cell(){
    if(live_.update_counter_bc5d20)return *live_.update_counter_bc5d20;
    if(!inspection_)throw std::runtime_error("Message stage needs canonical BC5D20");
    return update_counter_;
}
std::vector<GameMessageAction> GameMessageStage::dispatch_received(){
    if(dispatching_)throw std::runtime_error("Message dispatch cannot run inside itself");
    if(!received_pending_)throw std::runtime_error("Message dispatch needs a received frame");
    received_pending_=false;
    std::vector<GameMessageAction> actions;
    dispatching_=true;
    try{if(inspection_)actions=dispatch_.dispatch(received_.data(),received_.size());else dispatch_live();}
    catch(...){dispatching_=false;throw;}
    dispatching_=false;
    return actions;
}
void GameMessageStage::dispatch_live(){
    if(!live_.received_bytes_bcfc24)throw std::runtime_error("Live messages need canonical BCFC24");
    *live_.received_bytes_bcfc24+=std::uint32_t(received_.size());
    const auto write=[&](std::uint32_t address,std::uint32_t value){
        if(!live_.write_global)missing_message_host();live_.write_global(address,value);
    };
    for(std::size_t at=0;at<received_.size();){
        const auto id=received_[at++];const auto initial=table_.descriptor(id);
        bool duplicate=false;
        if(initial.flags&kSequencedMask){
            if(received_.size()-at<2)throw std::runtime_error("Truncated live message sequence");
            const auto sequence=load_u16(received_.data(),at);at+=2;
            const std::uint8_t bytes[2]={std::uint8_t(sequence),std::uint8_t(sequence>>8)};
            queue_.enqueue(table_,0x14,bytes,2);
            if(!live_.dedup)throw std::runtime_error("Live messages need canonical sequence ring");
            auto& ring=*live_.dedup;duplicate=std::find(ring.begin(),ring.end(),sequence)!=ring.end();
            if(!duplicate){std::move_backward(ring.begin(),ring.end()-1,ring.end());ring[0]=sequence;}
        }
        auto* payload=received_.data()+at;const auto available=received_.size()-at;
        PayloadReader r(payload,available);
        const auto length=(initial.flags&kSelfLength)?std::size_t(r.u16(0)):std::size_t(initial.length);
        if(length>available)throw std::runtime_error("Truncated live message payload");
        const auto call=[&](std::uint32_t address,std::initializer_list<std::uint32_t> args={},
            GameRecordArray array=GameRecordArray::none,std::int32_t index=-1){
            if(!live_.call)missing_message_host();GameMessageLiveCall request;
            request.address=address;request.count=std::uint8_t(args.size());std::copy(args.begin(),args.end(),request.args.begin());
            request.array=array;request.index=index;request.payload=payload;request.length=length;live_.call(request);
        };
        const auto read_record=[&](GameRecordArray array,std::int32_t index,std::uint32_t offset){
            if(!live_.record_dword)missing_message_host();return live_.record_dword(array,index,offset);
        };
        const auto put_record=[&](GameRecordArray array,std::int32_t index,std::uint32_t offset,std::uint8_t width,std::uint32_t value){
            if(!live_.record_write)missing_message_host();live_.record_write(array,index,offset,width,value);
        };
        if(!duplicate)switch(id){
        case 0x01:{
            if(length<132)throw std::runtime_error("Truncated descriptor patch");
            table_.patch(r.u32(0),payload+4);break;
        }
        case 0x09:
            if(!live_.projectile_09)throw std::runtime_error("Live projectile message09 body is not bound");
            live_.projectile_09(payload,length);break;
        case 0x0a:break; // Explicit source switch case.
        case 0x0b:call(0x470d50);break;
        case 0x0d:write(0xbc5d14,1);break;
        case 0x0e:{
            const auto array=GameRecordArray::players_7cca80;
            const auto index=std::int32_t(r.u16(2)&0x7fff);
            if(!read_record(array,index,0)){
                const std::uint8_t bytes[2]={std::uint8_t(r.u16(2)),std::uint8_t(r.u16(2)>>8)};
                queue_.enqueue(table_,0x17,bytes,2);
            }else{
                put_record(array,index,0x6c,1,0);
                put_record(array,index,0x6c,4,read_record(array,index,0x6c)|r.u8(0x10));
                put_record(array,index,0x24,4,r.u32(4));put_record(array,index,0x28,4,r.u32(8));
                put_record(array,index,0x2c,4,r.u32(12));put_record(array,index,0x30,4,std::uint32_t(std::int32_t(r.i16(0)))<<16);
                put_record(array,index,0x7c,4,r.u8(0x11));
            }break;
        }
        case 0x29:case 0x34:case 0x35:case 0x36:case 0x37:case 0x38:case 0x3b:{
            const auto array=GameRecordArray::actors_7dde80;
            const auto index=id==0x3b?r.i32(10)>>16:std::int32_t(r.i16(0));
            if(!read_record(array,index,0))break;
            const auto put=[&](std::uint32_t offset,std::uint32_t value){put_record(array,index,offset,4,value);};
            put_record(array,index,0x6c,1,0);put(0x6c,read_record(array,index,0x6c)|r.u8(15));
            const unsigned xyz=id==0x3b?0:2;
            put(0x24,r.u32(xyz));put(0x28,r.u32(xyz+4));put(0x2c,r.u32(xyz+8));put(0x30,std::uint32_t(r.u8(14))<<24);
            if(id==0x34||id==0x35||id==0x36){
                put(0x7c,std::uint32_t(r.i32(14)>>16)<<8);put(0x88,std::uint32_t(r.i32(16)>>16)<<8);
            }
            if(id==0x34||id==0x35)put(0x8c,std::uint32_t(r.u8(20))<<24);
            if(id==0x35){
                put(0x98,std::uint32_t(r.u8(21))<<22);put(0xa0,std::uint32_t(r.u8(21))<<22);put(0xa8,r.u8(22));
                put(0xcc,std::uint32_t(r.u8(23))<<24);put(0xd0,std::uint32_t(r.u8(24))<<24);
                put(0xac,std::uint32_t(r.i32(22)>>24)<<4);put(0xb0,std::uint32_t(r.i32(23)>>24)<<4);put(0xd4,r.u8(27));
            }else if(id==0x36){
                put(0xec,r.u32(20));put(0xf0,std::uint32_t(r.i32(22)>>16)<<4);
                put(0x8c,std::uint32_t(r.u8(26))<<24);put(0xe8,r.u8(27));
                std::int32_t out40=0,out3c=0;
                host_->sincos_477820(std::int32_t(read_record(array,index,0x30)),out40,out3c);
                const auto product=[&](std::int32_t factor){
                    const auto value=std::int64_t(std::int32_t(read_record(array,index,0x7c)))*factor;
                    return std::uint32_t(std::int32_t(std::uint32_t(std::uint64_t(value)>>22))>>6);
                };
                put(0x3c,product(out3c));put(0x40,product(out40));
            }else if(id==0x37){
                put(0xe0,std::uint32_t(r.i32(14)>>16));put(0xe4,std::uint32_t(r.i32(16)>>16));put(0xf0,r.u8(20));
            }else if(id==0x38){
                put(0x8c,std::uint32_t(r.u8(16))<<24);put(0xf0,std::uint32_t(r.u8(17))<<24);put(0xec,std::uint32_t(r.i32(16)>>16)<<8);
            }else if(id==0x3b){put(0x8c,std::uint32_t(r.u8(16))<<24);put(0xf0,r.u8(17));}
            break;
        }
        case 0x14:queue_.cancel_sequence(r.u16(0));break;
        case 0x19:write(0xbc5d18,1);break;
        case 0x1a:if(!source_global(0x67f528)){write(0x50321c,r.u32(0));queue_.set_send_delay(r.u16(0));}break;
        case 0x1b:if(!source_global(0x67f528))write(0x67f548,r.u32(0));break;
        case 0x1e:call(0x471010);break;
        case 0x20:call(0x470400);break;
        case 0x23:
            write(0xbcfc14,r.u32(0));call(0x455af0,{r.u32(4)});call(0x455b30,{r.u32(8)});call(0x455b10,{r.u32(12)});break;
        case 0x25:call(0x43f5a0,{r.u8(1)},GameRecordArray::players_7cca80,r.u8(0));break;
        case 0x30:write(0xc23b78,r.u8(0));break;
        case 0x33:if(!source_global(0x67f528))write(0x67dbbc,4);break;
        case 0x39:
            if(!source_global(0x67f528)){
                const auto index=r.i16(0);write(0xa158c0,std::uint32_t(r.i32(0)>>16));
                if(!live_.record_dword)missing_message_host();
                const auto target=live_.record_dword(GameRecordArray::players_7cca80,index,0x110);
                if(target)call(target,{4},GameRecordArray::players_7cca80,index);
            }break;
        case 0x3a:
            if(!source_global(0x67f528)){
                call(0x45dc70,{std::uint32_t(r.i32(10)>>16),r.u32(0),r.u32(4),r.u32(8)});
                call(0x43e7d0,{std::uint32_t(r.i32(12)>>16),r.u32(0),r.u32(4),r.u32(8)});
            }break;
        case 0x3c:if(!source_global(0x67f528))call(0x4560c0,{std::uint32_t(r.i32(6)>>16)});break;
        case 0x3e:if(!source_global(0x67f528))call(0x471570,{r.u8(0),r.u8(1),r.u8(2)});break;
        case 0x3f:if(!source_global(0x67f528))call(0x4715b0);break;
        case 0x41:if(!source_global(0x67f528))call(0x471600);break;
        case 0x43:call(0x471760);break;
        case 0x44:if(!source_global(0x67f528))call(0x479fa0,{3,0,0,0});break;
        case 0x46:call(0x471810);break;
        case 0x47:if(!source_global(0x67f528))call(0x479fa0,{0x2e,0,0,0});break;
        case 0x48:call(0x471870);break;
        default:
            if(!live_.message||!live_.message(id,payload,length))
                throw std::runtime_error("Live message body is not bound: "+std::to_string(id));
        }
        const auto current=table_.descriptor(id);
        const auto advance=(current.flags&kSelfLength)?std::size_t(r.u16(0)):std::size_t(current.length);
        if(advance>available)throw std::runtime_error("Live message skip exceeds its frame");
        at+=advance;
    }
}

void GameMessageStage::flush_queue() {
    std::vector<GameMessageSend> sends;
    queue_.step(frame_, table_, sends);
    flush_frame();
    queue_.compact();
}

void GameMessageStage::flush_frame() {
    if (!frame_.bytes().empty()) {
        if (source_global(0x67f528)) {
            if (network_packet_.size() + frame_.bytes().size() < kGameMessageFrameCapacity) {
                network_packet_.insert(network_packet_.end(), frame_.bytes().begin(),
                                       frame_.bytes().end());
            }
        } else if (host_ != nullptr) {
            host_->send_local_frame(frame_.bytes().data(), frame_.bytes().size());
        }
    }
    frame_.begin();
}

void GameMessageStage::serialize(std::uint8_t id, std::uint16_t sequence,
                                 const std::uint8_t* payload, std::uint16_t length) {
    frame_.append(table_, id, sequence, payload, length);
}

void GameMessageStage::enqueue_player_update(const GameMessageUpdateInput& input) {
    if (!input.local_present) {
        return;
    }
    const std::uint32_t terrain = host_ != nullptr
                                      ? static_cast<std::uint32_t>(
                                            host_->terrain_height_47df40(input.x, input.y))
                                      : 0;
    const std::uint32_t dz = input.z - terrain;
    const std::uint32_t x = (input.x & 0xFFFFFF00) | ((dz >> 16) & 0xFF);
    const std::uint32_t y = (input.y & 0xFFFFFF00) | ((dz >> 8) & 0xFF);
    std::uint8_t payload[0x10] = {};
    payload[0] = static_cast<std::uint8_t>(x & 0xFF);
    payload[1] = static_cast<std::uint8_t>((x >> 8) & 0xFF);
    payload[2] = static_cast<std::uint8_t>((x >> 16) & 0xFF);
    payload[3] = static_cast<std::uint8_t>((x >> 24) & 0xFF);
    payload[4] = static_cast<std::uint8_t>(y & 0xFF);
    payload[5] = static_cast<std::uint8_t>((y >> 8) & 0xFF);
    payload[6] = static_cast<std::uint8_t>((y >> 16) & 0xFF);
    payload[7] = static_cast<std::uint8_t>((y >> 24) & 0xFF);
    payload[8] = static_cast<std::uint8_t>(input.pitch >> 24);
    payload[9] = static_cast<std::uint8_t>(input.yaw >> 24);
    payload[0x0A] = input.field_7c;
    payload[0x0B] = input.field_94;
    payload[0x0C] = static_cast<std::uint8_t>(player_update_counter_ & 0xFF);
    payload[0x0D] = static_cast<std::uint8_t>((player_update_counter_ >> 8) & 0xFF);
    payload[0x0E] = static_cast<std::uint8_t>((player_update_counter_ >> 16) & 0xFF);
    payload[0x0F] = static_cast<std::uint8_t>((player_update_counter_ >> 24) & 0xFF);
    queue_.enqueue(table_, 0x0A, payload, 0x10);
    ++player_update_counter_;  // 0xBCFC28
}

void GameMessageStage::enqueue_shot() { queue_.enqueue(table_, 0x28, nullptr, 0); }

void GameMessageStage::enqueue_detonate() { queue_.enqueue(table_, 0x2C, nullptr, 0); }

void GameMessageStage::enqueue_placement(std::uint16_t a, std::uint32_t b, std::uint32_t c,
                                         std::uint32_t d, std::uint32_t e) {
    std::uint8_t payload[0x0F] = {};
    payload[0] = static_cast<std::uint8_t>(a & 0xFF);
    payload[1] = static_cast<std::uint8_t>(a >> 8);
    for (int i = 0; i < 4; ++i) {
        payload[2 + i] = static_cast<std::uint8_t>((b >> (8 * i)) & 0xFF);
        payload[6 + i] = static_cast<std::uint8_t>((c >> (8 * i)) & 0xFF);
        payload[0x0A + i] = static_cast<std::uint8_t>((d >> (8 * i)) & 0xFF);
    }
    payload[0x0E] = static_cast<std::uint8_t>(e >> 24);
    queue_.enqueue(table_, 0x2A, payload, 0x0F);
}

void GameMessageStage::enqueue_weapon_trace() {
    const std::uint32_t a = host_ != nullptr ? host_->weapon_aim_c23b70() : 0;
    const std::uint32_t b = host_ != nullptr ? host_->weapon_aim_c23b74() : 0;
    std::uint8_t payload[4] = {
        static_cast<std::uint8_t>(a >> 16), static_cast<std::uint8_t>(a >> 24),
        static_cast<std::uint8_t>(b >> 16), static_cast<std::uint8_t>(b >> 24)};
    queue_.enqueue(table_, 0x2F, payload, 4);
}

void GameMessageStage::enqueue_peer_ping() { queue_.enqueue(table_, 0x32, nullptr, 0); }

void GameMessageStage::submit_local_echo(const std::uint8_t* frame, std::size_t length) {
    local_echo_.assign(frame, frame + length);
}

void GameMessageDispatch::reset() {
    dedup_ring_.fill(0);
    actions_.clear();
    followups_.clear();
    current_ = nullptr;
    single_player_ = false;
    local_player_index_ = -1;
    flag_67f53c_ = 0;
    text_config_ = GameMessageTextConfig{};
    flag_bc5d14_ = false;
    flag_bc5d18_ = false;
    delay_50321c_ = 0;
    value_67f548_ = 0;
    death_bcfc2c_ = 0;
    death_bcfc30_ = 0;
    death_bcfc34_ = 0;
    control_bcfc14_ = 0;
    net_flag_67dbe8_ = 0;
    byte_c23b78_ = 0;
    flag_67dbbc_ = 0;
    entity_callback_a158c0_ = 0;
    max_players_ = 0;
    max_actors_ = 0;
    max_squad_ = 0;
    max_effects_ = 0;
    session_string_bcf624_.clear();
    mission_file_67f438_.clear();
    blob_50352c_.fill(0);
    scoreboard_count_ = 0;
    scoreboard_field_bcfbe4_ = 0;
    scoreboard_names_.clear();
    scoreboard_values_.clear();
    scoreboard_pair_a_.fill(0);
    scoreboard_pair_b_.fill(0);
    scoreboard_pair_c_.fill(0);
    player_slots_.clear();
    actor_slots_.clear();
}

void GameMessageStage::reset() {
    if(dispatching_)throw std::runtime_error("Message reset cannot clear an active dispatch frame");
    table_ = GameMessageTable();
    queue_ = GameMessageQueue();
    frame_.clear();
    dispatch_.reset();
    single_player_ = false;
    networked_ = false;
    tick_limit_ = 1;
    update_counter_ = 0;
    tick_counter_ = 0;
    player_update_counter_ = 0;
    network_packet_.clear();
    local_echo_.clear();
    received_.clear();received_pending_=false;
    dispatch_.set_single_player(true);
}

std::vector<GameMessageAction> GameMessageStage::update(const GameMessageUpdateInput& input) {
    ++update_cell();  // 0xBC5D20
    std::vector<GameMessageAction> actions = pump_receive();
    if (host_ != nullptr) {
        host_->weapons_tick_482b00();  // 0x482B00
    }
    if (source_global(0x67f520)) {
        ++tick_counter_;    // 0xBC5D10
        if (tick_counter_ < source_global(0x67f538)) {
            return actions;  // 0x46F70C early out
        }
        tick_counter_ = 0;
        if (!source_global(0x67f528)) {
            if(inspection_)enqueue_player_update(input);
            else{if(!live_.player_update_0a)missing_message_host();live_.player_update_0a();}
        }
    }
    const auto current=inspection_?input:(live_.current_update?live_.current_update():throw std::runtime_error("Message update needs current player cells"));
    if (current.current_weapon_item == 0x1B) {
        enqueue_weapon_trace();  // 0x470FB0
    }
    if (!source_global(0x67f528) && (inspection_?input.flag_67dbc0:live_.current_update().flag_67dbc0)) {
        enqueue_peer_ping();  // 0x32
    }
    flush_queue();  // 0x46F4B0
    return actions;
}

}  // namespace dfre
