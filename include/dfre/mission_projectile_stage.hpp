#pragma once
#include "dfre/mission_tick.hpp"
#include "dfre/projectiles.hpp"
#include <functional>
#include <optional>

namespace dfre {
// Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
struct MissionAreaActorRef { std::uint32_t group = 3; std::size_t index = 0; };
struct MissionAreaActor { AiActorImage* image = nullptr; std::uint32_t identity = 0; };
struct MissionAreaDefinition { std::uint32_t type = 0, id = 0; };
struct MissionAreaBounds { std::int32_t radius_64 = 0, height_68 = 0; };
struct MissionListenerPose { Fixed3 position; std::uint32_t yaw = 0, pitch = 0; };
enum class MissionAreaSite : std::uint32_t { blast = 0x465830, shock = 0x465a60, burn = 0x465b30 };
struct MissionAreaDispatch { std::int32_t range = 0; MissionAreaSite site = MissionAreaSite::blast; };
struct MissionProjectileStageState {
    std::uint32_t host = 0;
    std::uint8_t rules_67f549 = 0;
    std::int32_t water = 0;
    std::uint32_t local_player_identity = 0;
    MissionAreaActor local_player;
    std::uint32_t alternate_listener = 0; // 6BA3EC
    MissionListenerPose alternate_pose;  // 6BA3F4..6BA404
    std::vector<MissionPendingSpawn>* pending = nullptr;
    BehaviorState* random = nullptr; // Canonical BD0060 state; required on its path.
    // Bind only when the first pending type lies outside 0..5. The original
    // then uses incoming EDI and the unwritten local callback slot.
    std::optional<MissionAreaDispatch> incoming_dispatch;
};
struct MissionProjectileStageBindings {
    std::function<std::size_t(std::uint32_t group)> count;
    std::function<MissionAreaActor(MissionAreaActorRef)> actor;
    std::function<MissionAreaActor(std::uint32_t identity)> owner;
    std::function<MissionAreaDefinition(std::uint32_t definition)> definition;
    std::function<MissionAreaBounds(std::uint32_t graphic)> bounds;
    // 42E2B0(actor identity,start,0,end). Execute the real query now.
    std::function<std::int32_t(MissionAreaActorRef,const Fixed3& start,const Fixed3& end)> segment;
    // Original actor +110 callback, with messages 2, 3 or 5.
    std::function<void(MissionAreaActorRef,std::uint32_t callback,std::uint32_t message)> update;
    std::function<void()> local_hit; // 484A20
    std::function<void(const MissionListenerPose&)> listener; // 43EBA0
};
// 45E050. Listener selection; no call when local and alternate poses are absent.
void mission_listener_stage(MissionProjectileStageState& state,const MissionProjectileStageBindings& bindings);
// 465830/465A60/465B30. Re-read the live queue record by index after effects.
void mission_area_apply(MissionAreaSite site,MissionAreaActorRef actor,std::size_t pending_index,
    std::int32_t distance,MissionProjectileStageState& state,const MissionProjectileStageBindings& bindings);
// 465BD0. Groups 3,1,0 for each pending record, then clear the host queue.
void mission_pending_area_stage(MissionProjectileStageState& state,const MissionProjectileStageBindings& bindings);
} // namespace dfre
