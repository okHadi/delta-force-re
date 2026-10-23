#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionModelCleanupBindings {
    std::function<std::uint32_t(std::uint32_t,unsigned)> read;
    std::function<void(std::uint32_t,unsigned,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// Full486660 chain. Borrow the current physical registries and allocation owner.
void mission_model_cleanup(const MissionModelCleanupBindings&);
}
