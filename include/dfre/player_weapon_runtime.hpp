#pragma once
#include "dfre/player_step.hpp"
#include "dfre/projectiles.hpp"
#include "dfre/weapons.hpp"

namespace dfre {
// Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
struct PlayerWeaponBindings {
    std::function<std::uint32_t&()> burst_storage;
    std::function<void(const WeaponEvent&)> effect; // sound, queue, and UI bodies
    std::function<void(WeaponViewKind,std::uint32_t)> view_body;
    std::function<bool(std::uint32_t)> input_query; // lazy 47A290(8C)
};
void player_weapon_prefix(WeaponState& state,const PlayerWeaponBindings& bindings);
PlayerStepReport player_weapon_step(WeaponState& weapons,PlayerStepState& player,
    const PlayerWeaponBindings& bindings,
    const std::function<std::optional<PlayerStepActor>(std::uint32_t)>& actor);

struct PlayerAimState {std::uint32_t sway_c23b68=0,sway_c23b6c=0;};
void toggle_player_aim(WeaponState& weapons,PlayerAimState& aim,
    const std::function<std::uint32_t()>& random);

struct PlayerShotBindings {
    std::function<std::uint32_t()> random; // shared 477860 source
    std::function<std::uint32_t&()> burst_storage;
    std::function<void(std::uint32_t kind,MuzzleResult&)> projectile; // 46FB40
    std::function<void()> melee; // 470B00
};
// Original 45EA00. This owns its burst writes and spread, and executes each
// reached spawn/melee body before later burst writes.
void execute_player_shot(std::uint32_t weapon,MuzzleResult& muzzle,
    const PlayerShotBindings& bindings);
} // namespace dfre

namespace dfre {
struct PlayerGunActor {Fixed3 position;Angle3 angles;};
struct PlayerGunBindings {
    std::function<bool()> steady; // 484740
    std::function<void(std::uint32_t)> animated; // 45E7A0(actor)
    std::function<MuzzleResult()> muzzle; // 482D90 then 4847D0 and current aim
    std::function<void(std::uint32_t site,std::uint32_t actor,MuzzleResult&)> shot;
    std::function<std::optional<PlayerGunActor>(std::uint32_t)> actor;
    std::function<void(std::uint32_t,std::array<std::int32_t,2>)> translate_actor;
    std::function<void(std::uint32_t item,Fixed3 position,std::uint32_t yaw)> place;
    std::function<std::uint32_t()> crt_random; // separate 49AF6C source
};
// Live view/recoil blocks for the firing paths. WeaponState owns the
// pending/timer writes that follow each block.
void execute_player_view(WeaponViewKind kind,std::uint32_t weapon,std::uint32_t camera_mode,
    PlayerStepState& player,const PlayerGunBindings& bindings);
} // namespace dfre
