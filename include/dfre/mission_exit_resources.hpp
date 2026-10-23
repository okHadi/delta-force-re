#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionExitResourceBindings {
    std::function<std::uint32_t(std::uint32_t,unsigned)> read;
    std::function<void(std::uint32_t,unsigned,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
void mission_input_record_close(const MissionExitResourceBindings&); //47A640
void mission_input_replay_close(const MissionExitResourceBindings&); //47A660
//47A690. Borrow the current physical pointer table and allocation owner.
void mission_terrain_resources_release(const MissionExitResourceBindings&);
}
