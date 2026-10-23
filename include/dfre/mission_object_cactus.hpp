#pragma once
#include "dfre/mission_object_live.hpp"
namespace dfre {
// 4534A0. Bind current44DF90,447980 andA158C0 when reached.
void mission_object_cactus(MissionTickState&,const AiActorRef&,
    const MissionObjectLiveBindings&,std::uint32_t param2=0);
void bind_mission_object_cactus(MissionTickState&,MissionObjectLiveBindings);
}
