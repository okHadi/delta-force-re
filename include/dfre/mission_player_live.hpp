#pragma once
#include "dfre/mission_live.hpp"
#include "dfre/mission_muzzle.hpp"
#include "dfre/player_weapon_runtime.hpp"

namespace dfre {
// Player-block defaults follow the 4829F0 clear and eye-height write.
struct MissionPlayerCells {
    std::array<std::uint32_t,3> recoil_c239e8{};
    std::uint32_t eye_c23a18=0xb333;
    PlayerAimState aim;
    std::uint32_t waypoint_count_c23b7c=0,waypoint_index_c23b80=0;
    std::vector<std::uint32_t> waypoints_c23b84;
    std::optional<std::uint32_t> camera_mode_6ba3ec;
};
struct MissionPlayerProviders {
    std::function<void(MissionTickState&,MissionPlayerCells&,std::uint32_t actor,
        std::uint32_t site,std::uint32_t kind,MuzzleResult&)> projectile;
    std::function<void(MissionTickState&,MissionPlayerCells&,std::uint32_t actor,
        std::uint32_t site)> melee;
    std::function<std::uint32_t(MissionTickState&,MissionPlayerCells&)> crt_random;
    std::function<void(MissionTickState&,MissionPlayerCells&,std::uint32_t item,
        Fixed3 position,std::uint32_t yaw)> place;
};
// Keep all referenced objects alive. The adapter owns no actor or person copy.
class MissionPlayerLive {
public:
    MissionPlayerLive(MissionTickState&,MissionPlayerCells&,MissionMuzzle&,
        MissionPlayerProviders);
    ~MissionPlayerLive();
    MissionPlayerLive(const MissionPlayerLive&)=delete;
    MissionPlayerLive& operator=(const MissionPlayerLive&)=delete;
    std::optional<std::uint32_t> stage(const MissionStageCall&);
    PlayerWeaponBindings weapon_bindings();
    void install();
    void uninstall() noexcept;
    bool installed() const noexcept { return installed_; }
    void view(WeaponViewKind,std::uint32_t weapon);
private:
    MissionTickState& state_;
    MissionPlayerCells& cells_;
    MissionMuzzle& muzzle_;
    MissionPlayerProviders providers_;
    PlayerStepState* active_=nullptr;
    bool in_foreign_=false;
    bool installed_=false;
    decltype(WeaponState::burst_storage) prior_burst_;
    decltype(WeaponState::view_body) prior_view_;
    decltype(WeaponState::on_event) prior_effect_;
    decltype(WeaponState::trigger_query) prior_query_;
    WeaponShotMode prior_mode_=WeaponShotMode::isolated_fixture;
    AiActorRef ref(std::uint32_t handle) const;
    std::uint32_t local() const;
    void load(PlayerStepState&) const;
    void save(const PlayerStepState&);
    void boundary(const std::function<void()>&);
    void context(const std::function<void(PlayerStepState&)>&);
    void shot(std::uint32_t site,std::uint32_t actor,MuzzleResult&);
};
} // namespace dfre
