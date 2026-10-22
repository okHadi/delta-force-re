#pragma once
#include "dfre/mission_frame_services.hpp"
#include <string>

namespace dfre {
struct MissionScriptVoice {std::uint32_t handle=0,name=0;};
struct MissionScriptAudioCache {std::uint8_t loaded=0;std::uint32_t allocation=0;};
struct MissionScriptStep {
    std::uint32_t text=0,sample=0;
    std::uint8_t mode=0;
    std::array<std::uint32_t,3> position{};
    // The 8E3D68 callback supplies the three playback arguments.
    std::optional<std::array<std::uint32_t,3>> playback;
};
struct MissionScriptRuntimeCells {
    MissionScriptQueueCells& queue;
    std::uint32_t& host_67f528;
    std::uint32_t dictionary_8e3f3c=0;
    std::uint32_t voice_count_8e3d50=0;
    std::array<MissionScriptVoice,256> voices{};
};
struct MissionScriptRuntimeBindings {
    std::function<std::uint8_t(std::uint32_t)> byte;
    std::function<std::uint32_t(std::uint32_t)> word;
    std::function<std::string(std::uint32_t)> string;
    std::function<std::uint32_t(std::uint32_t)> load_audio; // 49D400(name,8E3E74)
    std::function<void(std::uint32_t)> missing_audio; // 493A80 diagnostic format
    std::function<std::uint32_t(std::uint32_t)> parse_id; // 49DFF0
    std::function<std::optional<std::uint32_t>(std::uint32_t,std::uint32_t)> text; // 49A076
    std::function<bool(MissionScriptStep&)> step; // 8E3D68
    std::function<std::uint32_t(std::uint32_t,const std::array<std::uint32_t,3>&)> play; // 494DD0
    std::function<void(std::uint32_t)> show_text; // optional 8E3D6C
    std::function<bool(std::uint32_t)> playing; // 494FF0
    std::function<MissionScriptAudioCache*(std::uint32_t)> cache; // 49CFD0
    std::function<void(std::uint32_t)> free_audio; // 4980D0
    std::function<void(std::uint32_t,const std::array<std::uint8_t,10>&)> message; // 442630
};
class MissionScriptRuntime {
public:
    // Keep the cells and mutable binding owner alive through each call.
    MissionScriptRuntime(MissionScriptRuntimeCells&,const MissionScriptRuntimeBindings&);
    MissionScriptRuntime(MissionScriptRuntimeCells&,MissionScriptRuntimeBindings&&)=delete;
    void step(std::uint32_t resource,std::uint32_t index); // 456110
    void announce(std::uint32_t name,std::uint16_t index); // 447C60
    std::uint32_t remove(std::uint32_t name); // 456000,455FA0
    void cleanup(); // 455B40
    // Complete a call from MissionFrameServiceBindings::script_call.
    void call(std::uint32_t site,std::uint32_t resource=0,std::uint32_t index=0);
private:
    MissionScriptRuntimeCells& cells_;
    const MissionScriptRuntimeBindings& bindings_;
};
} // namespace dfre
