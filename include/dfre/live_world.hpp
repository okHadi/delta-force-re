#pragma once
#include "dfre/actor_behavior.hpp"
#include "dfre/ai_update.hpp"
#include "dfre/world_runtime.hpp"
#include <array>
#include <vector>

namespace dfre {
using MissionActorImages = std::array<std::vector<AiActorImage>,4>;
enum class ActorPersonEncoding { slot, handle };
struct MissionActorSeed {
    MissionActorImages images;
    AiActorTables tables;
    std::array<std::vector<BehaviorActorExtra>,4> extras;
    std::array<std::vector<ActorPersonEncoding>,4> person_encoding;
};

// Keep every original byte while loading the fields owned by the tick code.
MissionActorImages initial_actor_images(const MissionRuntimeState& state);
MissionActorSeed seed_mission_actors(const MissionRuntimeState& state);
AiActorState read_live_actor_image(const AiActorImage& image,bool active,
    std::optional<std::size_t> person);
BehaviorActorExtra read_live_actor_extra(const AiActorImage& image);
AiActorImage merge_live_actor_image(const AiActorImage& stored,const AiActorState& actor,
    const BehaviorActorExtra& extra,ActorPersonEncoding encoding);
} // namespace dfre
