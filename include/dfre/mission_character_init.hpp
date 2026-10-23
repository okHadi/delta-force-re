#pragma once
#include "dfre/mission_character_allocate.hpp"
namespace dfre {
void mission_character_clear_scalars(const MissionCharacterAllocateBindings&,MissionCharacterAllocateRegisters&); //43A080
void mission_character_seed_pose(const MissionCharacterAllocateBindings&,MissionCharacterAllocateRegisters&); //43A0E0
//43A150. true is source carry-clear; false is source carry-set. EFLAGS are outside the register interface.
bool mission_character_select_action(const MissionCharacterAllocateBindings&,MissionCharacterAllocateRegisters&);
} // namespace dfre
