#pragma once
#include "dfre/mission_debrief.hpp"
namespace dfre {
void mission_debrief_campaign_audio(const MissionDebriefBindings&); //41EF40
void mission_debrief_won_audio(const MissionDebriefBindings&); //41F320
// The caller owns a mapped 128h-byte frame, current text/scratch/result/profile
// memory, and all synchronous services.493A80 and4C4F40 remain required.
void mission_debrief_text(const MissionDebriefBindings&,std::uint32_t frame); //41EF80
}
