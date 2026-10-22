#pragma once
#include "dfre/ai.hpp"
#include "dfre/collision.hpp"
#include <functional>

namespace dfre {
// Original 4889C0 and 488F00, Df.exe SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
struct ActorCollisionActor { AiActorImage* image = nullptr; std::uint32_t identity = 0; };
struct ActorCollisionDefinition { std::uint32_t id = 0, flags = 0; };
struct ActorCollisionModel { std::int32_t radius_64 = 0; bool collision_present = false; };
struct ActorCollisionMesh {
    ModelMatrix matrix{}; // Current forward 0x40-byte mesh state, first 12 words.
    std::array<std::int32_t,6> bounds{}; // Original mesh +40..+54.
    std::vector<CollisionVolume> volumes;
};
enum class ActorCollisionSite : std::uint32_t {
    refill = 0x446f00, heal = 0x446f60, remove = 0x45da30,
    can_use = 0x4605d0, pickup = 0x460470, deposit = 0x447280,
    deliver_side_one = 0x4472d0, deliver_side_two = 0x447360,
    objective = 0x446fb0, objective_effect = 0x447610, sound = 0x43e720
};
struct ActorCollisionCall {
    ActorCollisionSite site;
    std::array<std::uint32_t,4> arguments{};
    std::uint32_t count = 0;
};
struct ActorCollisionBindings {
    bool host = false;
    std::function<ActorCollisionModel(std::uint32_t model)> model;
    // Null is the original callback's null result. Ignore mesh state +3C flags.
    std::function<const std::vector<ActorCollisionMesh>*(std::uint32_t model,ActorCollisionActor target)> state;
    std::function<ActorCollisionDefinition(std::uint32_t definition)> definition;
    std::function<ActorCollisionActor(std::uint32_t identity)> actor;
    // Execute each reached operation now. Arguments use bound actor identities.
    // objective argument 2 is the target identity +24, the position pointer.
    std::function<std::uint32_t(const ActorCollisionCall&)> call;
};
// Resolves boxes and cylinders in mesh/volume order. Position writes take
// place after each mesh; trigger definitions keep the actor position unchanged.
std::uint32_t actor_target_collision(ActorCollisionActor actor,ActorCollisionActor target,
    const ActorCollisionBindings& bindings);
// Trigger helper. incoming_eax preserves the original host-off return value.
std::uint32_t actor_collision_trigger(ActorCollisionActor actor,ActorCollisionActor target,
    const ActorCollisionBindings& bindings,std::uint32_t incoming_eax = 0);
} // namespace dfre
