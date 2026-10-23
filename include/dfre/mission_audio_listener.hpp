#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
namespace dfre {
class MissionAudioListenerError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct MissionAudioListenerBindings {
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
};
// 43EBA0. Map6E06A8/AC/B0 and6E06B4 to MissionAudioFrame's listener refs.
void mission_audio_listener(std::uint32_t x,std::uint32_t y,std::uint32_t z,
    std::uint32_t yaw,std::uint32_t pitch,const MissionAudioListenerBindings&);
}
