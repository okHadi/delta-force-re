#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionActorCreateError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct MissionActorCreateBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>& arguments)> call;
};
//45D520. Returns a physical group-three actor pointer, or zero when its pool is full.
std::uint32_t mission_actor_create(const MissionActorCreateBindings&,std::uint32_t face,std::uint32_t side);
} // namespace dfre
