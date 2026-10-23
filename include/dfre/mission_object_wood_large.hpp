#pragma once
#include "dfre/mission_object_live.hpp"
namespace dfre {
// 453930. Bind current44F270,447980 andA158C0 at each reached source site.
void mission_object_wood_large(MissionTickState&,const AiActorRef&,
    const MissionObjectLiveBindings&,std::uint32_t message=0);
void bind_mission_object_wood_large(MissionTickState&,MissionObjectLiveBindings);
}
