#pragma once
#include "dfre/ai.hpp"
#include "dfre/projectiles.hpp"
#include <functional>
#include <optional>

namespace dfre {
// Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
struct ActorPickupActor {
    AiActorImage* image = nullptr;
    std::uint32_t identity = 0;
    std::uint32_t original_address = 0; // Original table address for wire indexes.
    AiPersonState* person = nullptr; // Canonical home fields +54..+60, when present.
};
struct ActorPickupClient {
    std::uint8_t active_18c = 0;
    std::uint32_t actor_08 = 0;
    std::uint32_t deliveries_1bc = 0;
};
struct ActorPickupState {
    std::uint32_t host = 0;
    std::uint8_t rules_67f549 = 0;
    std::uint32_t local_player = 0;
    std::uint32_t team_one_6e815c = 0, team_two_6e81c4 = 0;
    std::array<ActorPickupClient,17>* clients = nullptr;
};
struct ActorPickupDefinition { std::uint32_t id = 0; std::uint16_t health_94 = 0; };
enum class ActorPickupTransport : std::uint32_t { broadcast = 0x4423f0, reliable = 0x442630 };
struct ActorPickupPose { std::uint8_t* bytes = nullptr; std::uint32_t identity = 0; }; // 16 live bytes.
struct ActorPickupBindings {
    std::function<ActorPickupDefinition(std::uint32_t)> definition;
    std::function<void(std::size_t client,std::uint32_t message,std::uint32_t,std::uint32_t)> client_message; // 441750
    std::function<void(std::size_t client)> resupply; // 447E80
    std::function<void(std::uint32_t)> sound; // 43E720
    std::function<void()> local_flag; // 484ED0
    std::function<void(ActorPickupActor)> remove; // 45DA30
    std::function<void(ActorPickupTransport,std::uint32_t id,const std::vector<std::uint8_t>&)> message;
    std::function<void(ActorPickupActor,ActorPickupPose,std::uint32_t)> objective; // 4606B0
    std::function<std::optional<ActorPickupActor>(std::uint32_t owner,Fixed3)> allocate; // 460D20
};
void actor_pickup_refill(ActorPickupActor actor,ActorPickupState& state,const ActorPickupBindings& bindings);
void actor_pickup_heal(ActorPickupActor actor,ActorPickupState& state,const ActorPickupBindings& bindings);
void actor_pickup_take(ActorPickupActor actor,ActorPickupActor target,ActorPickupState& state,const ActorPickupBindings& bindings);
void actor_pickup_flag_message(ActorPickupActor actor,ActorPickupState& state,const ActorPickupBindings& bindings);
void actor_pickup_return(ActorPickupActor actor,ActorPickupState& state,const ActorPickupBindings& bindings);
std::uint32_t actor_pickup_restore_home(ActorPickupActor actor);
void actor_pickup_deposit(ActorPickupActor actor,ActorPickupState& state,const ActorPickupBindings& bindings);
void actor_pickup_deliver(ActorPickupActor actor,ActorPickupActor carried,bool side_two,
    ActorPickupState& state,const ActorPickupBindings& bindings);
void actor_pickup_objective(ActorPickupActor actor,ActorPickupPose pose,std::uint32_t mode,
    ActorPickupState& state,const ActorPickupBindings& bindings);
void actor_pickup_objective_spawn(std::uint32_t owner,Fixed3 position,const ActorPickupBindings& bindings);
} // namespace dfre
