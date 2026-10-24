#pragma once
#include "dfre/mission_debrief.hpp"
namespace dfre {
//42CBB0. frame is source EBP. Supply current bytes from frame-7Ch through
//frame+18h and put the filename pointer at frame+14h before entry.
std::uint32_t mission_character_resource_load(const MissionDebriefBindings&,std::uint32_t frame);
//439D20. Stable filename/output correspond to source ESI/EDX. CPU registers,
//flags and saved stack slots are outside this memory-only interface.
void mission_character_resource_relocate(const MissionDebriefBindings&,
    std::uint32_t filename,std::uint32_t output);
//42C9A0. The caller owns eight current local bytes. Integer math uses source
//signed multiplication, wrapping addresses and current unsigned division.
void mission_character_resource_prepare(const MissionDebriefBindings&,
    std::uint32_t registry_index,std::uint32_t locals);
std::uint32_t mission_character_resource_label(const MissionDebriefBindings&,
    std::uint32_t payload,std::uint32_t label); //498440
}
