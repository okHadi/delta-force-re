#pragma once
#include "dfre/mission_client_added.hpp"
namespace dfre {
//440640. Current client/actor/person memory and nested services share their owners.
void mission_client_finalize(const MissionClientAddedBindings&);
} // namespace dfre
