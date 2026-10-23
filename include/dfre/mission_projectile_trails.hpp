#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace dfre {
struct MissionProjectileTrailCells {
    std::uint32_t ring_4f9148=0;
    std::array<std::array<std::uint32_t,8>,2048> records_9e3098{};
};
struct MissionProjectileTrailBindings {
    std::function<std::uint32_t(std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t)> write;
};
MissionProjectileTrailBindings mission_projectile_trail_bindings(MissionProjectileTrailCells&);
// Arguments stay current until each source read: fromXYZ, toXYZ, color.
std::uint32_t mission_projectile_trail_record(const MissionProjectileTrailBindings&,
    const std::array<std::uint32_t,7>&); //463350
//4633C0 clears the first4000hex bytes and preserves the ring index.
void mission_projectile_trail_clear(MissionProjectileTrailCells&);
} // namespace dfre
