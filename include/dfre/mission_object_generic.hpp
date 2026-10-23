#pragma once
#include "dfre/mission_object_live.hpp"
namespace dfre {
// 4533F0. Bind current453270,447980 andA158C0 when the source reaches them.
void mission_object_generic(MissionTickState&,const AiActorRef&,
    const MissionObjectLiveBindings&,std::uint32_t message=0);
void bind_mission_object_generic(MissionTickState&,MissionObjectLiveBindings);
}
