#pragma once
#include "dfre/mission_object_live.hpp"
namespace dfre {
// 453810. Bind current44ED40,447980 andA158C0 at each reached source site.
void mission_object_wood_small(MissionTickState&,const AiActorRef&,
    const MissionObjectLiveBindings&,std::uint32_t message=0);
void bind_mission_object_wood_small(MissionTickState&,MissionObjectLiveBindings);
}
