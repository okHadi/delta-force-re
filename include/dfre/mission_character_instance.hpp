#pragma once
#include "dfre/mission_character_init.hpp"
namespace dfre {
struct MissionCharacterInstanceBindings {
    MissionCharacterAllocateBindings memory;
    std::function<std::uint32_t(std::uint32_t size,std::uint32_t alignment,
        std::uint32_t flags,std::uint32_t label)> allocate;
};
//ESI is the physical instance. Constructor EDI is the prepared CHR payload.
//Frame/decode EDI is the frame; timed decode EBX is the signed divisor.
//The seven register values are current. ESP, EFLAGS and saved stack slots
//are outside this interface. All source arrays use the borrowed memory.
void mission_character_instance_construct(const MissionCharacterInstanceBindings&,MissionCharacterAllocateRegisters&); //439E48
void mission_character_instance_auxiliary(const MissionCharacterInstanceBindings&,MissionCharacterAllocateRegisters&); //43A1C0
void mission_character_instance_frame(const MissionCharacterInstanceBindings&,MissionCharacterAllocateRegisters&); //43A690
void mission_character_instance_delta(const MissionCharacterInstanceBindings&,MissionCharacterAllocateRegisters&); //43A2C7
void mission_character_instance_immediate(const MissionCharacterInstanceBindings&,MissionCharacterAllocateRegisters&); //43A4DC
void mission_character_instance_rotate(const MissionCharacterInstanceBindings&,MissionCharacterAllocateRegisters&); //43A730
}
