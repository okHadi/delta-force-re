#pragma once
#include "dfre/mission_object_live.hpp"
namespace dfre {
//45C340/45C3A0/45C400. Keep definition138 and actual43E7D0 bound.
void mission_object_sound(MissionTickState&,const AiActorRef&,std::uint32_t source_body,
    const MissionObjectLiveBindings&,std::uint32_t param2=0);
void bind_mission_object_sound(MissionTickState&,std::uint32_t source_body,MissionObjectLiveBindings);
}
