#pragma once
#include "dfre/mission_logic.hpp"
#include "dfre/mission_live.hpp"
namespace dfre {
struct MissionResultLiveHost {
    MissionLogicFrameBindings state;
    std::function<AiActorImage(const AiActorRef&)> physical_tail;
    std::function<std::string(std::uint32_t actor)> fallback_name;
    std::function<void(std::uint32_t,const std::string&)> store_name;
    std::function<void(std::size_t,std::uint32_t,MissionLogicResultPacket&)> publish;
};
// Keep state and all host owners alive. Identities are physical source addresses.
// Host owns the one current result image, cells, clients, squads, and outbox.
MissionLogicResultBindings mission_result_live_bindings(MissionTickState&,MissionResultLiveHost);
} // namespace dfre
