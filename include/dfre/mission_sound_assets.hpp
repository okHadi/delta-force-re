#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dfre {
class MissionSoundAssetError : public std::runtime_error {
public:using std::runtime_error::runtime_error;
};
struct MissionSoundAssetBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    // Complete each reached file, allocator, cache or voice service.
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// Addresses are stable handles into current shared storage. Widths are1/2/4.
// Scratch is a separate caller-owned four-byte block for the source stack word.
std::uint32_t mission_sound_file_open(const MissionSoundAssetBindings&,std::uint32_t name,
    std::optional<std::uint32_t> incoming_edi=std::nullopt); //499120
std::uint32_t mission_sound_allocate(const MissionSoundAssetBindings&,std::uint32_t size,
    std::uint32_t alignment,std::uint32_t flags,std::uint32_t label,std::uint32_t tag); //498070
std::uint32_t mission_sound_find_sample(const MissionSoundAssetBindings&,std::uint32_t name,std::uint32_t context); //49CFD0
std::uint32_t mission_sound_find_bank(const MissionSoundAssetBindings&,std::uint32_t name,std::uint32_t context); //49D510
std::uint32_t mission_sound_mark_sample(const MissionSoundAssetBindings&,std::uint32_t name,std::uint32_t context); //49D020
std::uint32_t mission_sound_preload(const MissionSoundAssetBindings&,std::uint32_t context); //49D0F0
void mission_sound_fixup(const MissionSoundAssetBindings&,std::uint32_t context,std::uint32_t scratch); //49D8C0
std::uint32_t mission_sound_context_load(const MissionSoundAssetBindings&,std::uint32_t name,
    std::uint32_t context,std::uint32_t tag,std::uint32_t scratch); //49CE00
} // namespace dfre
