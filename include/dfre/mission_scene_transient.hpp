#pragma once
#include "dfre/scene_models.hpp"
namespace dfre {
struct MissionTickState;
// A current image snapshot for one physical 822E80 slot. The provider is required.
using MissionSceneTransientSource=std::function<AiActorImage(std::size_t slot)>;
std::vector<SceneModelRecord> mission_scene_transient_records(
 const MissionRuntimeState&,const MissionSceneTransientSource&);
// Read the canonical typed extra actor owner for each frame snapshot.
std::vector<SceneModelRecord> mission_scene_transient_records(
 const MissionRuntimeState&,const MissionTickState&);
}
