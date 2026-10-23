#pragma once

#include "dfre/mission.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>

namespace dfre {

// One trigger or action record: eight words as stored in the BMS tables.
using EventTableRecord = std::array<std::uint32_t, 8>;

// Object field addresses the original trigger kinds read by index (index * 0x40).
inline constexpr std::uint32_t event_object_field_7936a4 = 0x7936a4;
inline constexpr std::uint32_t event_object_field_7936a8 = 0x7936a8;
inline constexpr std::uint32_t event_object_field_7936ac = 0x7936ac;

// Host hooks for game state the event stage does not own.
//
// Original: Df.exe sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff,
// image base 0x400000. Method names keep the original addresses on purpose:
// gameplay meaning of these targets is not traced yet, so nothing is renamed.
// Bind each reached method. Missing methods throw EventError.
// See docs/events-runtime.md for every call site.
class EventError : public std::runtime_error { public: using std::runtime_error::runtime_error; };

class EventHost {
public:
    virtual ~EventHost() = default;

    // Reads the dword at [address + index * 0x40], as the original does.
    // 0x7936a4: marker/briefing flag.
    // 0x7936a8: initial count per object ident (0x44a110).
    // 0x7936ac: current count per object ident (0x44a200).
    virtual std::uint32_t object_index_dword(std::uint32_t address, std::uint32_t index);

    // 0x460330(trigger): rectangle test, trigger kind 1 subtype 10.
    virtual std::uint32_t probe_460330(std::size_t trigger_index);

    // 0x460560(words[3], words[4]): trigger kind 1 subtype 11.
    virtual std::uint32_t probe_460560(std::uint32_t word3, std::uint32_t word4);

    // Trigger kind 5 reads [0x67f520] and bit 1 of [0x67f549].
    virtual bool world_67f520_is_zero();
    virtual bool world_67f549_bit1();

    // Action dispatch, one method per original callee. Argument lists are the
    // original ones; the stage only routes. Index arguments let the game read
    // the record through the stage accessors.
    virtual void action_45f670(std::uint32_t word3, std::uint32_t word4);
    virtual void action_45f4c0(std::uint32_t word3);
    virtual void action_4601a0(std::size_t action_index);
    virtual void action_45f5b0(std::uint32_t word3);
    virtual void action_404dd0(std::uint32_t word3);
    virtual void action_43f500(std::uint32_t word3);
    virtual void action_444ca0(std::uint32_t mode);
    virtual void action_460640(std::uint32_t word3, std::uint32_t word4);
    virtual void action_460280(std::size_t action_index);
    // Exact byte for a legacy kind2 read beyond the loaded event table.
    virtual std::uint32_t external_event_fired(std::size_t event_index);
};

// Bounded event stage: load, post-load binding, scheduling and dispatch.
//
// Traced original functions (same record layout, same order of operations):
//   0x42ebe0  reset: frees tables, clears flag arrays, keeps 0x67dbec
//   0x42ec90  load: reads counts + tables, index -> pointer conversion
//   0x42eea0  post-load: bind kind (1,10) triggers to marker points
//   0x42f020  post-load: bind kind 12/13 actions to marker points
//   0x42eaf0  tick: one eighth of events per call, cursor mod 8
//   0x42eb60  run every event with bit 1 set in words[0]
//   0x42eba0  run every event with bit 2 set in words[0]
//   0x443c99  frame driver counter: tick() every 16th frame_driver_step()
//   0x42e9e0  event state machine: delays, fired flag, repeat, dispatch
//   0x42e930  trigger list combination (word[0] bits 0..2)
//   0x42e620  trigger evaluation, 6 kinds, jump tables at 0x42e5cc/0x42e5e4
//   0x42e810  action dispatch, 16 kinds, jump table at 0x42e7cc
//
// Event record words keep the original dual use (BMS value then runtime state):
//   words[0] bit0 repeat, bit1 run via 0x42eb60, bit2 run via 0x42eba0
//   words[1] trigger table index      words[2] action table index
//   words[3] low16 repeat timer, high16 repeat reload (step 0x80)
//   words[4] low16 delay timer, high16 delay reload (step 0x80)
//   words[5] byte0 fired flag, byte1 trigger count, byte2 action count
class EventStage {
public:
    // 0x6e0050, 512 bytes; 0x6d7e50, 512 bytes; 0x6d8050, 32768 bytes.
    static constexpr std::size_t flag_2d_words = 128;
    static constexpr std::size_t flag_3d_words = 8192;

    explicit EventStage(EventHost* host = nullptr);

    EventHost* host() const noexcept { return host_; }
    void set_host(EventHost* host) noexcept { host_ = host; }

    // 0x42ebe0. Clears tables and the reload flag arrays. The cursor (0x4f2be4)
    // and 0x67dbec are not cleared by the original and stay here too.
    void reset();

    // Full original load: reset, tables, binders, then the loader flag toggle.
    void load(const MissionData& mission);

    // Table load without a MissionData (same order as 0x42ec90; binders are
    // separate and take the marker group of object group 3).
    void load_tables(std::vector<MissionEvent> events, std::vector<EventTableRecord> triggers,
                     std::vector<EventTableRecord> actions);

    // 0x42eea0: triggers with words[1] == 1 and words[2] == 10 store the id to
    // bind in words[4]; the binder replaces words[4..7] with the marker x/y
    // rect (min/max over every marker with raw byte 0x4a equal to the id and
    // type 0x177d), or clears words[1] and words[2] when none matches.
    void bind_object_triggers(const std::vector<MissionObject>& markers);

    // 0x42f020: actions with words[1] == 0xc or 0xd store the id to bind in
    // words[3]; the binder fills words[3], words[5], words[6], words[7], or
    // clears words[1] when no marker matches.
    void bind_object_actions(const std::vector<MissionObject>& markers);

    // 0x42eaf0: events cursor, cursor+8, ... then cursor = (cursor + 1) & 7.
    // Events with words[0] bit 1 or bit 2 set are skipped.
    void tick();

    // 0x42eb60 and 0x42eba0: one pass over every event with bit 1 or bit 2.
    void run_bit1_events();
    void run_bit2_events();

    // 0x443c99-0x443ca5: increment6E4734; signed values above15 tick/reset.
    void frame_driver_step();

    // Runtime entry points, exposed for the game and for tests.
    void process_event(std::size_t event_index);                 // 0x42e9e0
    std::uint32_t evaluate_event_triggers(std::size_t event_index); // 0x42e930
    std::uint32_t evaluate_trigger(std::size_t trigger_index);   // 0x42e620
    void dispatch_action(std::size_t action_index);              // 0x42e810

    std::size_t event_count() const noexcept { return events_.size(); }
    std::size_t trigger_count() const noexcept { return triggers_.size(); }
    std::size_t action_count() const noexcept { return actions_.size(); }

    MissionEvent& event(std::size_t index) { return events_.at(index); }
    const MissionEvent& event(std::size_t index) const { return events_.at(index); }
    EventTableRecord& trigger(std::size_t index) { return triggers_.at(index); }
    const EventTableRecord& trigger(std::size_t index) const { return triggers_.at(index); }
    EventTableRecord& action(std::size_t index) { return actions_.at(index); }
    const EventTableRecord& action(std::size_t index) const { return actions_.at(index); }

    // words[5] byte 0 of an event, the fired flag the runtime writes.
    std::uint32_t event_fired(std::size_t event_index) const;

    // 0x4f2be4 event cursor and 0x4f2be0 loader flag.
    std::uint32_t cursor() const noexcept { return cursor_; }
    std::uint32_t frame_counter() const noexcept { return frame_counter_; }
    void set_frame_counter(std::uint32_t value) noexcept { frame_counter_=value; }
    void set_cursor(std::uint32_t cursor) noexcept { cursor_ = cursor & 7u; }
    std::uint32_t loader_flag() const noexcept { return loader_flag_; }
    void set_loader_flag(std::uint32_t value) noexcept { loader_flag_ = value; }

    // 0x42edd0 / 0x42ee00 / 0x42ee30 / 0x42ee50 and their test twins
    // 0x42e520 / 0x42e550 / 0x42e580 / 0x42e5b0. Bit selection follows the
    // original x86 shifts. Word indexes must be inside their flag tables.
    bool flag_42e520(std::uint32_t a, std::uint32_t b) const;
    bool flag_42e550(std::uint32_t a, std::uint32_t b) const;
    bool flag_42e580(std::uint32_t a, std::uint32_t b, std::uint32_t c) const;
    bool flag_42e5b0(std::uint32_t bit) const;
    void set_flag_42edd0(std::uint32_t a, std::uint32_t b);
    void set_flag_42ee00(std::uint32_t a, std::uint32_t b);
    void set_flag_42ee30(std::uint32_t a, std::uint32_t b, std::uint32_t c);
    void set_flag_42ee50(std::uint32_t bit);

    // Inline setters from the action dispatch: kinds 14 and 15 write one bit
    // into 0x6e0254 and 0x6e0250 (0x42e8f4 and 0x42e909).
    void set_flags_6e0254_bit(std::uint32_t bit);
    void set_flags_6e0250_bit(std::uint32_t bit);

    // Raw flag words the original reads back: 0x42f1b0 reads 0x6e0254 and
    // 0x6e0250; 0x67dbec is read by trigger kind 3. The three word accessors
    // expose the reload arrays for tools and tests.
    std::uint32_t flag_word_6e0050(std::size_t index) const { return flags_6e0050_.at(index); }
    std::uint32_t flag_word_6d7e50(std::size_t index) const { return flags_6d7e50_.at(index); }
    std::uint32_t flag_word_6d8050(std::size_t index) const { return flags_6d8050_.at(index); }
    std::uint32_t flags_6e0254() const noexcept { return flags_6e0254_; }
    std::uint32_t flags_6e0250() const noexcept { return flags_6e0250_; }
    std::uint32_t& flags_67dbec() noexcept { return flags_67dbec_; }
    std::uint32_t flags_67dbec() const noexcept { return flags_67dbec_; }

private:
    EventHost& require_host() const;
    std::uint32_t host_object_dword(std::uint32_t address, std::uint32_t index) const;
    void dispatch_event_actions(const MissionEvent& record);
    void bind_records(const std::vector<MissionObject>& markers, bool triggers);

    EventHost* host_;
    std::vector<MissionEvent> events_;
    std::vector<EventTableRecord> triggers_;
    std::vector<EventTableRecord> actions_;
    std::uint32_t cursor_ = 0;        // 0x4f2be4
    std::uint32_t loader_flag_ = 0;   // 0x4f2be0
    std::uint32_t frame_counter_ = 0; // 0x6e4734
    std::array<std::uint32_t, flag_2d_words> flags_6e0050_{};
    std::array<std::uint32_t, flag_2d_words> flags_6d7e50_{};
    std::array<std::uint32_t, flag_3d_words> flags_6d8050_{};
    std::uint32_t flags_6e0250_ = 0;
    std::uint32_t flags_6e0254_ = 0;
    std::uint32_t flags_67dbec_ = 0; // 0x42ebe0 does not clear this one
};

} // namespace dfre
