#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionLeaveBindings {
    // Keep current source memory and all called owners alive through the call.
    // Source accesses use widths 1, 4, and 8; qword accesses stay single accesses.
    std::function<std::uint64_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint64_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>&)> call;
};
void mission_leave(const MissionLeaveBindings&); //404150
}
