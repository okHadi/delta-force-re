#pragma once
#include "dfre/mission_event_resources.hpp"
namespace dfre {
//42EEA0: bind current trigger runs to physical group2 marker rectangles.
void mission_event_markers_triggers(const MissionEventResourceBindings&);
//42F020: bind current action runs to physical group2 marker rectangles.
void mission_event_markers_actions(const MissionEventResourceBindings&);
}
