#pragma once
#include "dfre/projectiles.hpp"
#include <functional>
namespace dfre {
struct MissionAudioRegistration {std::uint32_t definition=0,actor=0,identity=0,reserved=0;};
struct MissionAudioCandidate {std::uint32_t registration=0,volume=0;};
struct MissionAudioSelected {std::uint32_t sample=0,identity=0,registration=0,volume=0,played=0,rate=0,pan=0;};
struct MissionAudioFrameCells {
    std::uint32_t counter_4f7ea8=0;
    Fixed3& listener_6e06a8;
    std::uint32_t& yaw_6e06b4;
    std::array<MissionAudioRegistration,1024> registrations_6e0700{};
    std::array<MissionAudioCandidate,64> candidates_6e0270{};
    std::array<MissionAudioSelected,6> selected_6e0470{};
    std::array<std::uint32_t,6> identities_6e06d0{},handles_6e06e8{};
};
struct MissionAudioFrameBindings {
    std::function<std::uint32_t(std::uint32_t actor,std::uint32_t offset)> actor_word;
    std::function<std::uint32_t(std::uint32_t definition,std::uint32_t offset)> definition_word;
    std::function<void(std::uint32_t handle,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate)> update; // 494FA0
    std::function<void(std::uint32_t handle)> stop; // 495070
    std::function<std::uint32_t(std::uint32_t sample,std::uint32_t pan,std::uint32_t volume,std::uint32_t rate,std::uint32_t priority)> start; // 494F40
};
// Original 49DC20 attenuation, including the signed divide fault boundary.
std::uint32_t mission_audio_attenuation(std::int32_t distance,std::int32_t range,std::uint8_t scale,std::uint8_t ceiling);
// Original 43F0B0. Backend providers run synchronously; the scheduler opens no device.
void mission_audio_frame(MissionAudioFrameCells&,const MissionAudioFrameBindings&);
} // namespace dfre
