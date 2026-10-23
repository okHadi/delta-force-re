#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionOwnerCleanupBindings {
    std::function<std::uint32_t(std::uint32_t,unsigned)> read;
    std::function<void(std::uint32_t,unsigned,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
void mission_sound_cleanup(const MissionOwnerCleanupBindings&); //43E6D0
//49DB20: one current context DWORD at arguments; 16-byte same-owner frame.
//The host supplies source retained-byte reads after earlier table release.
void mission_sound_bank_cleanup(const MissionOwnerCleanupBindings&,
                                std::uint32_t arguments,std::uint32_t frame);
void mission_resource_cleanup(const MissionOwnerCleanupBindings&,std::uint32_t record); //44B940
void mission_resource_cleanup_global(const MissionOwnerCleanupBindings&); //402D40
}
