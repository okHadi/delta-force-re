#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace dfre {
struct MissionAnimationEffectBindings {
    // Source reads use widths1,4 and8. Eight bytes hold the PE double constant.
    std::function<std::uint64_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t site,const std::array<std::uint32_t,4>& args)> call;
};
//45E0F0 args: unused first flag, current step flag, source actor pointer.
void mission_animation_effect(const MissionAnimationEffectBindings&,const std::array<std::uint32_t,3>& args);
}
