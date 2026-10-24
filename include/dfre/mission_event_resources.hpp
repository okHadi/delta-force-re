#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionEventResourceBindings {
    std::function<std::uint32_t(std::uint32_t,unsigned)> read;
    std::function<void(std::uint32_t,unsigned,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
void mission_event_resources_reset(const MissionEventResourceBindings&); //42EBE0
//42EC90: borrow the current mission file cursor and allocation/table owners.
void mission_event_resources_load(const MissionEventResourceBindings&);
}
