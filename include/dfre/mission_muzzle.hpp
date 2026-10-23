#pragma once
#include "dfre/mission_animation.hpp"
#include "dfre/projectile_hits.hpp"
#include "dfre/weapons.hpp"
#include <optional>
#include <utility>

namespace dfre {
struct MissionMuzzleModel { std::uint32_t handle{}; const Model* model{}; };
struct MissionMuzzleCharacter { std::uint32_t instance{}; const CharacterModel* character{}; };
struct MissionMuzzleResources {
    std::vector<RuntimeItemDefinition> definitions;
    std::vector<MissionMuzzleModel> models;
    std::vector<MissionMuzzleCharacter> characters;
};
struct MissionMuzzleView { Fixed3 view_delta; std::int32_t eye_height{}; };
struct MissionMuzzleWeapon { std::uint32_t primary_definition{},secondary_definition{}; };
using MissionMuzzleSlotSource = std::function<Matrix4&(std::uint32_t,const AiActorImage&)>;

Matrix4 original_muzzle_attachment(); // 481AF0 writes C22B40.
class MissionMuzzle {
public:
    // Keep the cache and animations alive. Resources use the loaded item order.
    MissionMuzzle(const MissionRuntimeState& state,const MissionAssetCache& cache,
        const MissionAnimations& animations);
    MissionMuzzle(MissionMuzzleResources resources,const MissionAnimations& animations);
    const MissionMuzzleWeapon& weapon(std::uint32_t index) const;
    MuzzleResult normal(const AiActorImage& actor,std::uint32_t weapon,
        const MissionMuzzleView& view) const; // 482D90 followed by 4847D0.
    bool animated(ProjectileHitActor actor,
        const std::function<void(ProjectileHitActor,const MuzzleResult&)>& shot);
    // Refresh the current image after the graphic callback and before aim reads.
    // Keep the returned slot-seven matrix alive through the refresh.
    bool animated(ProjectileHitActor actor,
        const std::function<void(ProjectileHitActor,const MuzzleResult&)>& shot,
        const std::function<void(ProjectileHitActor)>& after_graphic);
    Matrix4& attachment() noexcept { return attachment_; }
    const Matrix4& attachment() const noexcept { return attachment_; }
    // Seed the shared scratch value when a callback does not write slot seven.
    void set_slot_seven(Matrix4 value) { slot_seven_=value; }
    Matrix4& slot_seven();
    // Supply full mutable scratch state for other graphic callback tags.
    void set_slot_source(MissionMuzzleSlotSource source) { slot_source_=std::move(source); }
private:
    bool animated_body(ProjectileHitActor actor,
        const std::function<void(ProjectileHitActor,const MuzzleResult&)>& shot,
        const std::function<void(ProjectileHitActor)>* after_graphic);
    const Model& model(std::uint32_t handle) const;
    Fixed3 hardpoint(std::uint32_t model,std::uint32_t index) const;
    Matrix4& animated_slot(std::uint32_t graphic,const AiActorImage& actor);
    MissionMuzzleResources resources_;
    const MissionAnimations* animations_{};
    std::array<MissionMuzzleWeapon,kWeaponCount> weapons_{};
    Matrix4 attachment_;
    std::optional<Matrix4> slot_seven_;
    MissionMuzzleSlotSource slot_source_;
};
} // namespace dfre
