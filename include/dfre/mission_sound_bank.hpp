#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace dfre {
class MissionSoundBankError:public std::runtime_error {
public:using std::runtime_error::runtime_error;
};
struct MissionSoundBankBindings {
    // Current shared bytes. Width is 1, 2, or 4; addresses and handles stay stable.
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    // Required reached bodies: 499120,49D050,499020,498230,494DD0,494E80.
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>&)> call;
};
inline constexpr std::uint32_t mission_sound_bank_scratch=0xd099c0;
inline constexpr std::uint32_t mission_sound_bank_candidates=8;
// Shared 5014FC RNG advances even when range is zero.
std::uint32_t mission_sound_bank_random(const MissionSoundBankBindings&,std::uint32_t range); // 49D4C0
bool mission_sound_bank_filter(const MissionSoundBankBindings&,std::uint32_t flags); // 49D560 reject
bool mission_sound_bank_quality(const MissionSoundBankBindings&,std::uint32_t flags); // 49D580 reject
// 49D5A0. Params holds 32 current bytes; context and bank remain shared.
// Producing a ninth candidate or using an invalid round-robin index raises an error.
std::uint32_t mission_sound_bank_play(const MissionSoundBankBindings&,std::uint32_t bank,
    std::uint32_t params,std::uint32_t context);
// 43E720. Supply a 32-byte caller-owned parameter address, kept alive
// through all nested callbacks. The original sound definition table has 512 records.
void mission_sound_global_request(const MissionSoundBankBindings&,std::uint32_t id,std::uint32_t params);
} // namespace dfre
