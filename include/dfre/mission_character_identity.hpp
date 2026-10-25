#pragma once
#include "dfre/animation.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
namespace dfre {
struct MissionCharacterResourceOwner {std::string_view filename;const CharacterModel& model;};
struct MissionCharacterAllocationIdentity {
    std::uint32_t generation,size;
    std::uint64_t incarnation; //Distinct nonzero arena-owner identity for every allocation.
};
struct MissionCharacterIdentityBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<MissionCharacterResourceOwner(std::uint32_t token)> resource;
    std::function<AnimationController&(std::uint32_t token)> instance;
    //Return a stamp only for an exact current live arena payload.
    std::function<std::optional<MissionCharacterAllocationIdentity>(std::uint32_t payload)> allocation;
};
//Keep current memory, arena, cache and controller owners alive. Serialize owner
//changes with these calls. Each publication follows complete source setup.
class MissionCharacterIdentity {
public:
    explicit MissionCharacterIdentity(MissionCharacterIdentityBindings);
    ~MissionCharacterIdentity();
    MissionCharacterIdentity(const MissionCharacterIdentity&)=delete;
    MissionCharacterIdentity& operator=(const MissionCharacterIdentity&)=delete;
    void publish_resource(std::uint32_t token,std::uint32_t physical_row);
    void publish_instance(std::uint32_t token,std::uint32_t physical_row,std::uint32_t resource_token);
    std::uint32_t physical_resource(std::uint32_t token) const;
    std::uint32_t resource_token(std::uint32_t physical_row) const;
    std::uint32_t physical_instance(std::uint32_t token) const;
    std::uint32_t instance_token(std::uint32_t physical_row) const;
    //Call after the real source release/clear. Typed owner release stays with
    //the caller. Remove instance bindings before their resource binding.
    void unpublish_instance(std::uint32_t token);
    void unpublish_resource(std::uint32_t token);
    std::size_t resource_count() const noexcept;
    std::size_t instance_count() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
