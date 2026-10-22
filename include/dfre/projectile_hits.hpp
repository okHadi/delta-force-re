#pragma once
#include "dfre/ai.hpp"
#include "dfre/projectiles.hpp"
#include "dfre/world_hits.hpp"
#include <functional>
#include <optional>

namespace dfre {

// Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Actor identities and callback addresses are bound by the caller. Images keep
// all original bytes; effects run at the original call site and may change them.
struct ProjectileHitActor {
    AiActorImage* image = nullptr;
    std::uint32_t identity = 0;
};
struct ProjectileHitResult {
    bool hit = false;
    std::int32_t distance = 0;
    std::uint32_t material = 0;
    std::size_t record = 0, mesh = 0, face = 0;
};
struct ProjectileHitMesh {
    WorldHitMesh geometry;
    std::vector<std::uint32_t> material_ids;
};
struct ProjectileModelBindings {
    // Model is the selected actor +0x10/+0x14 handle. Null state means no hit.
    std::function<std::int32_t(std::uint32_t model)> radius;
    std::function<const std::vector<ProjectileHitMesh>*(std::uint32_t model,
        ProjectileHitActor actor)> state;
    // Original 0x4564B0 divide-error message, required only on that path.
    std::function<void(std::int32_t numerator, std::int32_t denominator)> divide_error;
};
// 0x464400: nearest two-sided model face. Equal distances select the later face.
std::int32_t projectile_model_hit(const std::vector<ProjectileHitActor>& actors,
    const WorldHitQuery& query, ProjectileHitResult& result, const ProjectileModelBindings& bindings);

struct ProjectileBoneSphere { std::array<std::int32_t,3> center{}; std::int32_t radius = 0; };
struct ProjectileBoneState { bool graphic_present = false; std::vector<ProjectileBoneSphere> bones; };
using ProjectileBoneProvider = std::function<ProjectileBoneState(ProjectileHitActor actor)>;
// Real 0x46BC00 matrices and prepared CHR bone +0x28/+0x34 hit spheres.
ProjectileBoneState projectile_character_spheres(const CharacterModel& prepared,
    const std::vector<CharacterBonePose>& pose, ProjectileHitActor actor);
// 0x463790: bone order 2,1,0,3,...; material remains unchanged.
std::int32_t projectile_bone_hit(const std::vector<ProjectileHitActor>& actors,
    const WorldHitQuery& query, ProjectileHitResult& result, const ProjectileBoneProvider& provider);
// 0x464BA0. A clear ray keeps result.distance unchanged.
std::int32_t projectile_water_hit(std::int32_t water, const WorldHitQuery& query, ProjectileHitResult& result);
// 0x464C00. All arithmetic, including the exponent shifts, uses original words.
std::uint32_t projectile_damage_amount(std::uint32_t threshold, const ProjectileEntity& projectile) noexcept;

struct ProjectileDamageDefinition {
    std::uint32_t type = 0, flags = 0, threshold = 0;
};
struct ProjectileDamageState {
    std::uint32_t host = 0;
    std::uint8_t rules_67f549 = 0;
    std::uint32_t local_player = 0;
    std::int32_t water = 0;
    std::uint32_t last_kind = 0, last_damage = 0;
};
struct ProjectileDamageBindings {
    std::function<ProjectileDamageDefinition(std::uint32_t definition)> definition;
    std::function<const AiActorImage&(std::uint32_t identity)> owner;
    std::function<std::uint8_t&(std::uint32_t person)> person_alert;
    std::function<void(std::int32_t slot, std::uint32_t direction)> alert;
    std::function<void(std::uint32_t callback, ProjectileHitActor actor, std::uint32_t message)> update;
    std::function<void()> local_hit;
};
// 0x464C50. owner_identity is the original projectile +0xF8 identity.
std::int32_t projectile_apply_damage(const ProjectileEntity& projectile, std::uint32_t owner_identity,
    ProjectileHitActor target, std::uint32_t hit_kind, ProjectileDamageState& state,
    const ProjectileDamageBindings& bindings);

struct ProjectileMuzzleWeapon { std::uint32_t model = 0, hardpoint = 0; };
struct ProjectileMuzzleBindings {
    // Bind weapon +0x98 through the item +0x6C lookup; hardpoint is weapon +0x90.
    std::function<ProjectileMuzzleWeapon(std::uint32_t weapon)> weapon;
    std::function<Matrix4&(std::uint32_t graphic, ProjectileHitActor actor)> slot_seven;
    std::function<Fixed3(std::uint32_t model, std::uint32_t index)> hardpoint;
    Matrix4 attachment; // original 0xC22B40
    std::function<void(ProjectileHitActor actor, const MuzzleResult& muzzle)> shot;
};
// 0x45E7A0. The slot-seven matrix changes in place before the shot call.
bool projectile_animated_muzzle(ProjectileHitActor actor, const ProjectileMuzzleBindings& bindings);

enum class ProjectileImpactSite : std::uint32_t {
    terrain_mark = 0x45dab0, explosion = 0x4657d0, visual = 0x45dc70,
    sound = 0x43e7d0, ground = 0x460d20, fragments = 0x45fcf0
};
struct ProjectileImpactEffect {
    ProjectileImpactSite site;
    Fixed3 position;
    std::uint32_t argument = 0, owner = 0;
};
struct ProjectileImpactBindings {
    ProjectileDamageBindings damage;
    std::function<std::uint32_t(std::uint32_t material)> material_flags;
    // Execute synchronously. Fragments uses (position,argument,0,0).
    std::function<void(const ProjectileImpactEffect&)> effect;
};
// The impact branch inside 0x464EB0, before projectile removal. hit_type is
// tick_projectile's event type 0..4; hit.mesh is the damage kind. For types
// 1..3 bind the actor from the selected query's list and hit.record.
void projectile_impact_effects(ProjectileEntity& projectile, std::uint32_t owner_identity,
    std::uint8_t hit_type, Fixed3 impact_position, ProjectileHitActor target,
    const ProjectileHitResult& hit, ProjectileDamageState& state,
    const ProjectileImpactBindings& bindings);

} // namespace dfre
