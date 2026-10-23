#pragma once
#include "dfre/projectiles.hpp"
#include "dfre/scene_models.hpp"
namespace dfre {
// Snapshot the fixed512 source slots. Keep the returned records alive through render.
std::vector<SceneEffectRecord> mission_scene_projectile_records(const std::vector<ProjectileEntity>&);
}
