#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dfre {
// Df.exe 0x482B00, SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
struct PlayerStepActor {
    std::array<std::uint32_t,2> position{}; // +24/+28
    std::uint8_t flags_6e=0;
    std::uint32_t waypoint_radius_7c=0;
};
struct PlayerStepState {
    std::uint32_t local_actor_c23820=0;
    std::array<std::uint32_t,3> recoil_c239e8{};
    std::uint32_t eye_c23a18=0;
    std::uint32_t prone_c23828=0;
    std::uint32_t game_type_67f53c=0;
    std::uint32_t waypoint_count_c23b7c=0;
    std::uint32_t waypoint_index_c23b80=0;
    std::vector<std::uint32_t> waypoints_c23b84;
};
struct PlayerStepWorld {
    // Runs 482B11..482B78 in order, including reached weapon effects.
    std::function<void(PlayerStepState&)> weapon_prefix;
    // Reads the current actor record at each reached source read.
    std::function<std::optional<PlayerStepActor>(std::uint32_t)> actor;
};
class PlayerStepError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct PlayerStepReport {bool ran=false;bool waypoint_advanced=false;bool waypoint_wrapped=false;};
PlayerStepReport step_player(PlayerStepState& state,const PlayerStepWorld& world);
} // namespace dfre
