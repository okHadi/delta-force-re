#pragma once
#include "dfre/mission_live.hpp"

namespace dfre {
struct MissionProjectileMaterialBinding {
    std::uint32_t model{};
    std::vector<std::uint32_t> identities; // Stable first-LOD material identities.
    std::vector<std::uint32_t> flags;      // Corresponding current material flags.
};
struct MissionProjectileCharacterBinding {
    std::uint32_t instance{};
    const CharacterModel* character{};
};
struct MissionProjectileLiveResources {
    std::vector<std::uint32_t> damage_thresholds; // Item definition +98, in loaded order.
    std::vector<MissionProjectileMaterialBinding> materials;
    std::vector<MissionProjectileCharacterBinding> characters;
};
struct MissionProjectileLiveGlobals {
    std::uint8_t rules_67f549{};
    bool alternate_move_filter{}; // Current 67F520/67F548/67F549 selection.
};
struct MissionProjectileLiveBindings {
    std::function<MissionProjectileLiveGlobals(const MissionTickState&)> globals;
    // Optional override. Empty uses current state.world.terrain through 47E390.
    // Exact return: 1 means clear; every other value is the hit distance code.
    std::function<std::int32_t(MissionTickState&,const ProjectileEntity&,const Fixed3&,std::int32_t)> terrain;
    std::function<void(MissionTickState&,const AiActorRef&,std::uint32_t,std::uint32_t)> update;
    std::function<void(MissionTickState&,std::int32_t,std::uint32_t)> alert;
    std::function<void(MissionTickState&)> local_hit;
    std::function<void(MissionTickState&,ProjectileEntity&,const ProjectileImpactEffect&)> impact;
    std::function<void(MissionTickState&,ProjectileEntity&,const ProjectileEvent&)> move;
    std::function<void(MissionTickState&,ProjectileEntity&,const ProjectileEvent&)> world_edge;
    std::function<void(MissionTickState&,std::int32_t,std::int32_t)> divide_error;
};
// Base terrain47E390: clear is1; preserve all original hit return values.
std::int32_t mission_projectile_terrain_query(const MissionTickState& state,
    const ProjectileEntity& projectile,const Fixed3& step,std::int32_t length);
// Projectile owner_slot is a group 3 index: -1 means no owner; slot 0 is valid.
// Keep all referenced owners alive. Damage stores the external A158C0/A158BC
// cells. Actor images flush before foreign calls and refresh after return/errors.
ProjectileQuery mission_projectile_live_query(MissionTickState& state,const MissionGeometry& geometry,
    const MissionAnimations& animations,MissionProjectileLiveResources resources,
    ProjectileDamageState& damage,MissionProjectileLiveBindings bindings,
    MissionGeometryMatrixSource matrices={});
// Material identities come from the caller's resource identity table. Flags,
// thresholds and prepared CHR bindings come from loaded mission resources.
ProjectileQuery mission_projectile_live_query(MissionTickState& state,const MissionGeometry& geometry,
    const MissionAssetCache& cache,const MissionRuntimeState& initial,const MissionAnimations& animations,
    std::vector<MissionProjectileMaterialBinding> materials,ProjectileDamageState& damage,
    MissionProjectileLiveBindings bindings,MissionGeometryMatrixSource matrices={});
} // namespace dfre
