#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionObjectEffectBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// Keep each mapped frame current through synchronous callbacks. Offsets are hex.
//446FB0: packet[0,12), args owner1C/pose20/count24.
void mission_object_effect_request(const MissionObjectEffectBindings&,std::uint32_t frame);
//447610: packet[0,2B), args owner38/X3C/Y40/Z44.
void mission_object_effect_publish(const MissionObjectEffectBindings&,std::uint32_t frame);
//4606B0: pose[0,18), args owner2C/pose30/count34. Word14 is caller data.
void mission_object_effect_apply(const MissionObjectEffectBindings&,std::uint32_t frame);
//460D20: actor[0,114), args owner120/X124/Y128/Z12C. Returns45D970 EAX.
std::uint32_t mission_object_effect_allocate(const MissionObjectEffectBindings&,std::uint32_t frame);
}
