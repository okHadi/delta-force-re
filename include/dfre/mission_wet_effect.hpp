#pragma once
#include <array>
#include <cstdint>
#include <functional>

namespace dfre {
struct MissionWetEffectBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    //49A620(frame,0,114),477860(),45D970(frame,1). Calls stay synchronous.
    std::function<std::uint32_t(std::uint32_t site,const std::array<std::uint32_t,3>&)> call;
};
//45DC70. Map a0x130-byte caller frame: scratch[0,114), args at120..12C.
// Share this memory with all reached callbacks and the canonical actor insertion.
void mission_wet_effect(const MissionWetEffectBindings&,std::uint32_t frame);
} // namespace dfre
