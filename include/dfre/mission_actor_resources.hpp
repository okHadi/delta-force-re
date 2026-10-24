#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionActorResourceBindings {
    std::function<std::uint32_t(std::uint32_t,unsigned)> read;
    std::function<void(std::uint32_t,unsigned,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// Use physical source instance addresses. Typed animation handles need a host bridge.
void mission_character_release(const MissionActorResourceBindings&,std::uint32_t instance); //42CD30
void mission_character_release_raw(const MissionActorResourceBindings&,std::uint32_t source_esi); //439C40
void mission_character_pool_clear(const MissionActorResourceBindings&); //439C10
void mission_character_resources_clear(const MissionActorResourceBindings&); //42C950
void mission_character_work_clear(const MissionActorResourceBindings&); //42CF40
void mission_actor_resources_clear(const MissionActorResourceBindings&); //449620
void mission_auxiliary_resources_clear(const MissionActorResourceBindings&); //47EC50
void mission_item_resource_links_clear(const MissionActorResourceBindings&); //482910
}
