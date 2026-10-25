#pragma once
#include "dfre/mission_live.hpp"
#include "dfre/events.hpp"
namespace dfre {
struct MissionEventPersonBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint16_t()> fp_control;
};
//45FF20: apply one current action to the actor's current person.
void mission_event_person_edit(const MissionEventPersonBindings&,std::uint32_t action_source,std::uint32_t actor_source);
//4601A0: scan current group1, group3, then group0 counts for the signed side.
void mission_event_person_edits(const MissionEventPersonBindings&,std::uint32_t action_source);
//460047: signed value, source x87 precision and rounding, then truncation.
std::uint32_t mission_event_person_rate(std::int32_t value,std::uint16_t control);
// Current physical actor/person fields and counts. Keep all owners alive.
// Unknown non-record cells use the required foreign memory providers.
MissionEventPersonBindings mission_event_person_current_bindings(MissionTickState&,
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> foreign_read,
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> foreign_write,
    std::function<std::uint32_t(std::size_t)> definition_address,
    std::function<std::uint16_t()> fp_control);
// Keep state and action storage alive. Supply the current 4FB2CC table owner.
MissionEventPersonBindings mission_event_person_bindings(MissionTickState&,EventTableRecord&,
    std::uint32_t action_source,std::function<std::uint32_t(std::uint32_t address)> table_word,
    std::function<std::uint16_t()> fp_control);
}
