#pragma once
#include "dfre/mission_actor_insert.hpp"
#include "dfre/mission_tick.hpp"
#include <memory>
namespace dfre {
struct MissionPrimaryActorLiveBindings {
    MissionActorInsertBindings delegated;
    std::function<std::uint32_t(std::size_t slot)> initial_definition;
    std::function<std::uint32_t(std::uint32_t raw)> definition_index;
    std::function<std::uint32_t(std::uint32_t index)> definition_address;
};
// Own only raw header metadata and the uncounted physical tail. Keep state and
// delegated providers alive. Supply exactly1024-currentcount current raw tail images.
class MissionPrimaryActorLive {
public:
    MissionPrimaryActorLive(MissionTickState&,MissionPrimaryActorLiveBindings,
        std::vector<AiActorImage> uncounted_tail);
    ~MissionPrimaryActorLive();
    MissionPrimaryActorLive(const MissionPrimaryActorLive&)=delete;
    MissionPrimaryActorLive& operator=(const MissionPrimaryActorLive&)=delete;
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::optional<std::uint32_t> record_base(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width);
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
    // Complete-preflight/commit service for49A620(7DDE80,0,45000h).
    // Keep current count/extents; character/person release has a separate boundary.
    void clear_physical_pool();
    MissionActorInsertBindings insertion_bindings();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
