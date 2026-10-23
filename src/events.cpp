#include "dfre/events.hpp"

#include <algorithm>

namespace dfre {

// Each reached foreign operation requires its current host binding.

std::uint32_t EventHost::object_index_dword(std::uint32_t, std::uint32_t) { throw EventError("Event host object field is not bound"); }
std::uint32_t EventHost::probe_460330(std::size_t) { throw EventError("Event host 460330 is not bound"); }
std::uint32_t EventHost::probe_460560(std::uint32_t, std::uint32_t) { throw EventError("Event host 460560 is not bound"); }
bool EventHost::world_67f520_is_zero() { throw EventError("Event host 67F520 is not bound"); }
bool EventHost::world_67f549_bit1() { throw EventError("Event host 67F549 is not bound"); }
void EventHost::action_45f670(std::uint32_t, std::uint32_t) { throw EventError("Event host 45F670 is not bound"); }
void EventHost::action_45f4c0(std::uint32_t) { throw EventError("Event host 45F4C0 is not bound"); }
void EventHost::action_4601a0(std::size_t) { throw EventError("Event host 4601A0 is not bound"); }
void EventHost::action_45f5b0(std::uint32_t) { throw EventError("Event host 45F5B0 is not bound"); }
void EventHost::action_404dd0(std::uint32_t) { throw EventError("Event host 404DD0 is not bound"); }
void EventHost::action_43f500(std::uint32_t) { throw EventError("Event host 43F500 is not bound"); }
void EventHost::action_444ca0(std::uint32_t) { throw EventError("Event host 444CA0 is not bound"); }
void EventHost::action_460640(std::uint32_t, std::uint32_t) { throw EventError("Event host 460640 is not bound"); }
void EventHost::action_460280(std::size_t) { throw EventError("Event host 460280 is not bound"); }

std::uint32_t EventHost::external_event_fired(std::size_t){throw EventError("Event external fired byte is not bound");}

namespace {
std::uint32_t low16(std::uint32_t value) noexcept { return value & 0xffffu; }
std::uint32_t high16(std::uint32_t value) noexcept { return value >> 16; }

std::uint32_t with_low16(std::uint32_t value, std::uint32_t part) noexcept {
    return (value & 0xffff0000u) | (part & 0xffffu);
}
} // namespace

EventStage::EventStage(EventHost* host) : host_(host) {}

void EventStage::reset() {
    // 0x42ebe0 frees the tables, then clears 0x6e0050 (0x200), 0x6d7e50
    // (0x200), 0x6d8050 (0x8000), 0x6e0254 and 0x6e0250. It leaves the cursor
    // 0x4f2be4 and 0x67dbec alone, so this does too.
    events_.clear();
    triggers_.clear();
    actions_.clear();
    flags_6e0050_.fill(0);
    flags_6d7e50_.fill(0);
    flags_6d8050_.fill(0);
    flags_6e0250_ = 0;
    flags_6e0254_ = 0;
}

void EventStage::load(const MissionData& mission) {
    // 0x42ec90 order: reset, tables, index to pointer conversion (the stage
    // keeps the raw indexes), both binders, then the loader flag toggle.
    reset();
    events_ = mission.events;
    triggers_ = mission.triggers;
    actions_ = mission.actions;
    bind_object_triggers(mission.object_groups[2]);
    bind_object_actions(mission.object_groups[2]);
    loader_flag_ ^= 1; // 0x42edb6
}

void EventStage::load_tables(std::vector<MissionEvent> events, std::vector<EventTableRecord> triggers,
                             std::vector<EventTableRecord> actions) {
    reset();
    events_ = std::move(events);
    triggers_ = std::move(triggers);
    actions_ = std::move(actions);
}

void EventStage::bind_object_triggers(const std::vector<MissionObject>& markers) {
    bind_records(markers, true);
}

void EventStage::bind_object_actions(const std::vector<MissionObject>& markers) {
    bind_records(markers, false);
}

void EventStage::bind_records(const std::vector<MissionObject>& markers, bool trigger_table) {
    // 0x42eea0 for triggers, 0x42f020 for actions. Both walk every event and
    // every slot of its run, and both match markers by type 0x177d plus the
    // converter's value for raw byte 0x4a (0x449710: param_1[0x1f] for 0x177d).
    for (auto& record : events_) {
        const std::uint32_t base = record.words[trigger_table ? 1 : 2];
        const std::uint32_t count = trigger_table ? record.trigger_count() : record.action_count();
        auto& table = trigger_table ? triggers_ : actions_;
        for (std::uint32_t slot = 0; slot < count; ++slot) {
            const auto index=std::uint64_t(base)+slot;
            if(index>=table.size())throw EventError("Event binding run exceeds its table");
            auto& entry = table[std::size_t(index)];
            const bool bind = trigger_table ? (entry[1] == 1 && entry[2] == 10)
                                            : (entry[1] == 0xcu || entry[1] == 0xdu);
            if (!bind) continue;
            const std::uint32_t id = trigger_table ? entry[4] : entry[3];
            // Rectangle slots: triggers 4,5,6,7; actions 3,5,6,7 (0x42f0f2).
            const std::size_t slots[4] = {trigger_table ? 4u : 3u, 5u, 6u, 7u};
            bool found = false;
            for (const auto& marker : markers) {
                if (marker.type_id != 0x177d) continue;
                if (static_cast<std::uint32_t>(marker.raw[0x4a]) != id) continue;
                const std::uint32_t x = static_cast<std::uint32_t>(marker.position_raw[0]);
                const std::uint32_t y = static_cast<std::uint32_t>(marker.position_raw[1]);
                if (!found) {
                    // First hit: point rectangle, min x, min y, max x, max y.
                    entry[slots[0]] = x;
                    entry[slots[1]] = y;
                    entry[slots[2]] = x;
                    entry[slots[3]] = y;
                    found = true;
                } else {
                    if (static_cast<std::int32_t>(x) < static_cast<std::int32_t>(entry[slots[0]])) entry[slots[0]] = x;
                    if (static_cast<std::int32_t>(y) < static_cast<std::int32_t>(entry[slots[1]])) entry[slots[1]] = y;
                    if (static_cast<std::int32_t>(x) > static_cast<std::int32_t>(entry[slots[2]])) entry[slots[2]] = x;
                    if (static_cast<std::int32_t>(y) > static_cast<std::int32_t>(entry[slots[3]])) entry[slots[3]] = y;
                }
            }
            if (!found) {
                // No marker: the trigger becomes kind 0 (never true); the
                // action keeps its index but its kind is cleared (never runs).
                entry[1] = 0;
                if (trigger_table) entry[2] = 0;
            }
        }
    }
}

void EventStage::tick() {
    // 0x42eaf0: events[cursor], events[cursor + 8], ... then cursor wraps mod 8.
    for (std::size_t index = cursor_; index < events_.size(); index += 8) {
        if ((events_[index].words[0] & 0x6u) == 0) process_event(index);
    }
    cursor_ = (cursor_ + 1) & 7u;
}

void EventStage::run_bit1_events() {
    // 0x42eb60.
    for (std::size_t index = 0; index < events_.size(); ++index) {
        if ((events_[index].words[0] & 0x2u) != 0) process_event(index);
    }
}

void EventStage::run_bit2_events() {
    // 0x42eba0.
    for (std::size_t index = 0; index < events_.size(); ++index) {
        if ((events_[index].words[0] & 0x4u) != 0) process_event(index);
    }
}

void EventStage::frame_driver_step() {
    // 0x443c8c-0x443ca5: counter 0x6e4734 increments per step, tick() runs on
    // signed values above 15 and the counter resets to zero.
    ++frame_counter_;
    if (static_cast<std::int32_t>(frame_counter_) > 15) {
        frame_counter_ = 0;
        tick();
    }
}

std::uint32_t EventStage::event_fired(std::size_t event_index) const {
    return events_.at(event_index).words[5] & 0xffu;
}

std::uint32_t event_fired_reference(const EventStage& stage,std::size_t requested,
    EventReferencePolicy policy,const EventReferenceDiagnostic& diagnostic){
    const auto loaded=stage.event_count();
    if(requested<loaded)return stage.event_fired(requested);
    if(policy!=EventReferencePolicy::missing_not_fired)
        throw EventError("Event fired reference exceeds its loaded table");
    if(!diagnostic)throw EventError("Missing event repair needs its diagnostic");
    diagnostic(requested,loaded);return 0;
}

EventHost& EventStage::require_host() const {
    if(!host_)throw EventError("Event host is not bound");
    return *host_;
}

std::uint32_t EventStage::host_object_dword(std::uint32_t address, std::uint32_t index) const {
    return require_host().object_index_dword(address, index);
}

void EventStage::dispatch_event_actions(const MissionEvent& record) {
    for (std::uint32_t slot = 0; slot < record.action_count(); ++slot) {
        const auto index=std::uint64_t(record.words[2])+slot;
        if(index>=actions_.size())throw EventError("Event action run exceeds its table");
        dispatch_action(std::size_t(index));
    }
}

void EventStage::process_event(std::size_t event_index) {
    // 0x42e9e0, line by line. Timers step by 0x80 and compare as signed 16 bit.
    MissionEvent& record = events_.at(event_index);
    std::uint32_t* w = record.words.data();

    std::uint32_t delay = low16(w[4]);
    std::uint32_t repeat = low16(w[3]);

    // Counter path 0x42ea10: step the repeat timer down and clear the fired
    // flag when it expires.
    const auto step_repeat = [&] {
        repeat=low16(w[3]);
        if (repeat == 0) return;
        const auto next = static_cast<std::int16_t>(repeat - 0x80u);
        repeat = static_cast<std::uint16_t>(next);
        w[3] = with_low16(w[3], repeat);
        if (next <= 0) {
            repeat = 0;
            w[3] = with_low16(w[3], 0);
            w[5] &= 0xffffff00u; // fired = 0
        }
    };

    // Arm path 0x42ead6: repeat flag on stores the reload; reload zero clears.
    const auto arm_repeat = [&] {
        if (!(w[0]&1)) return;
        const std::uint32_t reload = high16(w[3]);
        if (reload == 0) {
            w[5] &= 0xffffff00u;
        } else {
            w[3] = with_low16(w[3], reload);
        }
    };

    // Fire path 0x42ea7e: clear the delay, evaluate, then dispatch and step.
    const auto fire = [&] {
        w[4] = with_low16(w[4], 0);
        if (evaluate_event_triggers(event_index) == 0) {
            w[5] &= 0xffffff00u;
            w[3] = with_low16(w[3], 0);
            return;
        }
        dispatch_event_actions(record);
        step_repeat();
    };

    if (delay != 0) {
        const auto next = static_cast<std::int16_t>(delay - 0x80u);
        delay = static_cast<std::uint16_t>(next);
        w[4] = with_low16(w[4], delay);
        if (next > 0) {
            step_repeat();
            return;
        }
        fire();
        return;
    }
    if (repeat != 0) {
        step_repeat();
        return;
    }
    if ((w[5] & 0xffu) != 0) return; // fired, waiting
    if (evaluate_event_triggers(event_index) == 0) return;
    const std::uint32_t reload = high16(w[4]);
    w[5] = (w[5] & 0xffffff00u) | 1u; // fired = 1
    if (reload != 0) {
        w[4] = with_low16(w[4], reload);
        arm_repeat();
        return;
    }
    dispatch_event_actions(record);
    arm_repeat();
}

std::uint32_t EventStage::evaluate_event_triggers(std::size_t event_index) {
    // 0x42e930: one trigger is a value; the list combines with the previous
    // trigger's words[0] bits 1 (or) and 2 (xor); neither means and.
    const MissionEvent& record = events_.at(event_index);
    if(!record.trigger_count())return 1;
    const auto trigger_index=[&](std::uint32_t slot){
        const auto index=std::uint64_t(record.words[1])+slot;
        if(index>=triggers_.size())throw EventError("Event trigger run exceeds its table");
        return std::size_t(index);
    };
    std::uint32_t result=evaluate_trigger(trigger_index(0));
    if(triggers_[trigger_index(0)][0]&1)result^=1;
    for(std::uint32_t slot=1;slot<record.trigger_count();++slot){
        auto value=evaluate_trigger(trigger_index(slot));
        if(triggers_[trigger_index(slot)][0]&1)value^=1;
        const auto operation=triggers_[trigger_index(slot-1)][0];
        if(operation&2)result|=value;
        else if(operation&4)result^=value;
        else result&=value;
    }
    return result;
}

std::uint32_t EventStage::evaluate_trigger(std::size_t trigger_index) {
    // 0x42e620. Jump tables: 0x42e5cc on words[1], 0x42e5e4 on words[2].
    const EventTableRecord& entry = triggers_.at(trigger_index);
    switch (entry[1]) {
    case 0:
        return 0;
    case 1:
        switch (entry[2]) {
        case 1:
            return flag_42e520(entry[3], entry[4]) ? 1u : 0u;
        case 2:
            return flag_42e550(entry[3], entry[4]) ? 1u : 0u;
        case 3:
            return host_object_dword(event_object_field_7936a4, entry[3]) == 2u ? 1u : 0u;
        case 4:
            return static_cast<std::int32_t>(host_object_dword(event_object_field_7936ac, entry[3])) <= 0 ? 1u : 0u;
        case 5:
            return static_cast<std::int32_t>(host_object_dword(event_object_field_7936ac, entry[3])) > 0 ? 1u : 0u;
        case 6: {
            const auto index=entry[3];
            const auto live = static_cast<std::int32_t>(host_object_dword(event_object_field_7936a8, index));
            const auto start = static_cast<std::int32_t>(host_object_dword(event_object_field_7936ac, index));
            return static_cast<std::int32_t>(std::uint32_t(live) - std::uint32_t(start)) >= static_cast<std::int32_t>(entry[4]) ? 1u : 0u;
        }
        case 7:
            return flag_42e580(entry[3], entry[4], entry[5]) ? 1u : 0u;
        case 9: {
            const auto index=entry[3];
            const auto live=host_object_dword(event_object_field_7936a8,index);
            const auto start=host_object_dword(event_object_field_7936ac,index);
            return live==start?1u:0u;
        }
        case 10:
            // 0x42e72b forwards the whole trigger record pointer.
            return require_host().probe_460330(trigger_index);
        case 11:
            // 0x42e739 forwards words[3] and words[4].
            return require_host().probe_460560(entry[3], entry[4]);
        case 12: {
            const auto index=entry[3],threshold=entry[4];
            return static_cast<std::int32_t>(threshold)<=static_cast<std::int32_t>(
                host_object_dword(event_object_field_7936ac,index))?1u:0u;
        }
        default:
            return 0; // 0x42e637, also for subjects above 12
        }
    case 2: {
        // 0x42e76d: fired flag of the event named by words[3]. The original
        // requires a valid event table index.
        const std::size_t other = entry[3];
        if(other>=events_.size())return require_host().external_event_fired(other)&0xffu;
        return events_[other].words[5]&0xffu;
    }
    case 3:
        return flag_42e5b0(entry[3]) ? 1u : 0u;
    case 4:
        return loader_flag_; // 0x4f2be0, toggled once per load
    case 5:
        return require_host().world_67f520_is_zero() && !require_host().world_67f549_bit1() ? 1u : 0u;
    default:
        return 0; // words[1] above 5
    }
}

void EventStage::dispatch_action(std::size_t action_index) {
    // 0x42e810. Jump table 0x42e7cc; kinds above 15 return at once.
    EventTableRecord& entry = actions_.at(action_index);
    switch (entry[1]) {
    case 0:
        return;
    case 1:
        require_host().action_45f670(entry[3], entry[4]);
        return;
    case 2:
        require_host().action_45f4c0(entry[3]);
        return;
    case 3:
        require_host().action_4601a0(action_index);
        return;
    case 4:
        require_host().action_45f5b0(entry[3]);
        return;
    case 5:
        set_flag_42ee50(entry[3]); // 0x42ee50(word3)
        return;
    case 6:
        require_host().action_404dd0(entry[3]);
        return;
    case 7:
        require_host().action_43f500(entry[3]);
        return;
    case 8:
        require_host().action_444ca0(1);
        return;
    case 9:
        require_host().action_444ca0(2);
        return;
    case 10:
        require_host().action_444ca0(0);
        return;
    case 11:
        require_host().action_460640(entry[3], entry[4]);
        return;
    case 12:
    case 13:
        require_host().action_460280(action_index);
        return;
    case 14:
        set_flags_6e0254_bit(entry[3]);
        return;
    case 15:
        set_flags_6e0250_bit(entry[3]);
        return;
    default:
        return;
    }
}

// --- Flag memory ------------------------------------------------------------
// 0x42e520/0x42edd0 index 0x6e0050 as (a * 2 + (b >> 5)) with bit b & 31.
// 0x42e550/0x42ee00 index 0x6d7e50 the same way. 0x42e580/0x42ee30 index
// 0x6d8050 as ((a << 7) + b) with bit c & 31. 0x42e5b0/0x42ee50 bit a & 31
// in 0x67dbec. Every word index must be inside its table.

namespace {
std::int32_t flag_2d_index(std::uint32_t a, std::uint32_t b) noexcept {
    const std::uint32_t word = static_cast<std::uint32_t>(static_cast<std::int32_t>(b) >> 5) + a * 2u;
    return static_cast<std::int32_t>(word);
}
std::int32_t flag_3d_index(std::uint32_t a, std::uint32_t b) noexcept {
    return static_cast<std::int32_t>((a << 7) + b);
}
} // namespace

bool EventStage::flag_42e520(std::uint32_t a, std::uint32_t b) const {
    const std::int32_t index = flag_2d_index(a, b);
    if (index < 0 || static_cast<std::size_t>(index) >= flags_6e0050_.size()) throw EventError("Event 6E0050 flag index exceeds its table");
    return (flags_6e0050_[static_cast<std::size_t>(index)] & (1u << (b & 31u))) != 0;
}

void EventStage::set_flag_42edd0(std::uint32_t a, std::uint32_t b) {
    const std::int32_t index = flag_2d_index(a, b);
    if (index < 0 || static_cast<std::size_t>(index) >= flags_6e0050_.size()) throw EventError("Event 6E0050 flag index exceeds its table");
    flags_6e0050_[static_cast<std::size_t>(index)] |= 1u << (b & 31u);
}

bool EventStage::flag_42e550(std::uint32_t a, std::uint32_t b) const {
    const std::int32_t index = flag_2d_index(a, b);
    if (index < 0 || static_cast<std::size_t>(index) >= flags_6d7e50_.size()) throw EventError("Event 6D7E50 flag index exceeds its table");
    return (flags_6d7e50_[static_cast<std::size_t>(index)] & (1u << (b & 31u))) != 0;
}

void EventStage::set_flag_42ee00(std::uint32_t a, std::uint32_t b) {
    const std::int32_t index = flag_2d_index(a, b);
    if (index < 0 || static_cast<std::size_t>(index) >= flags_6d7e50_.size()) throw EventError("Event 6D7E50 flag index exceeds its table");
    flags_6d7e50_[static_cast<std::size_t>(index)] |= 1u << (b & 31u);
}

bool EventStage::flag_42e580(std::uint32_t a, std::uint32_t b, std::uint32_t c) const {
    const std::int32_t index = flag_3d_index(a, b);
    if (index < 0 || static_cast<std::size_t>(index) >= flags_6d8050_.size()) throw EventError("Event 6D8050 flag index exceeds its table");
    return (flags_6d8050_[static_cast<std::size_t>(index)] & (1u << (c & 31u))) != 0;
}

void EventStage::set_flag_42ee30(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const std::int32_t index = flag_3d_index(a, b);
    if (index < 0 || static_cast<std::size_t>(index) >= flags_6d8050_.size()) throw EventError("Event 6D8050 flag index exceeds its table");
    flags_6d8050_[static_cast<std::size_t>(index)] |= 1u << (c & 31u);
}

bool EventStage::flag_42e5b0(std::uint32_t bit) const {
    return (flags_67dbec_ & (1u << (bit & 31u))) != 0;
}

void EventStage::set_flag_42ee50(std::uint32_t bit) {
    flags_67dbec_ |= 1u << (bit & 31u);
}

void EventStage::set_flags_6e0254_bit(std::uint32_t bit) {
    flags_6e0254_ |= 1u << (bit & 31u);
}

void EventStage::set_flags_6e0250_bit(std::uint32_t bit) {
    flags_6e0250_ |= 1u << (bit & 31u);
}

} // namespace dfre
