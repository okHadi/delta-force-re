#pragma once
#include "dfre/mission_live.hpp"
#include "dfre/mission_audio_frame.hpp"
namespace dfre {
struct MissionActorLifecycleBindings {
    std::function<void(std::uint32_t,std::int32_t&,std::int32_t&)> sincos_4776c0;
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> terrain_47df40;
};
// Keep these shared owners alive. Registrations and actor+90 use source addresses.
class MissionActorLifecycle {
public:
    MissionActorLifecycle(MissionTickState&,MissionAnimations&,
        std::array<MissionAudioRegistration,1024>&,MissionActorLifecycleBindings);
    void remove(std::optional<AiActorRef>); // 45DA30; null returns
    void drop_link(const AiActorRef&);     // 4604C0
    void release_character(std::uint32_t); // 42CD30/439C40; zero returns
    void unregister_audio(const AiActorRef&); // 43F070
private:
    AiActorRef linked_actor(std::uint32_t) const;
    std::uint32_t read(const AiActorRef&,std::size_t) const;
    void write(const AiActorRef&,std::size_t,std::uint32_t,std::uint8_t=4);
    MissionTickState& state_;
    MissionAnimations& animations_;
    std::array<MissionAudioRegistration,1024>& registrations_;
    MissionActorLifecycleBindings bindings_;
};
} // namespace dfre
