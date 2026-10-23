#pragma once
#include "dfre/mission_object_live.hpp"
namespace dfre {
// Invoke the captured source callback pointer with its original message word.
// Owned bodies:429BC0,4519A0,4534A0,45C340,45C3A0,45C400,453810,4533F0,453930.
void mission_object_update(MissionTickState&,const AiActorRef&,std::uint32_t callback,
    std::uint32_t message,const MissionObjectLiveBindings&);
}
