#pragma once
#include "dfre/animation_driver.hpp"
#include "dfre/resources.hpp"
#include "dfre/world_runtime.hpp"
#include <map>
#include <memory>

namespace dfre {
struct MissionAnimationBinding {
    std::uint32_t instance{};
    const CharacterModel* character{};
    std::vector<AnimationDriverAction> actions;
};

// Own one controller per runtime instance. Keep the asset cache alive.
class MissionAnimations {
public:
    MissionAnimations(const MissionRuntimeState& state,const MissionAssetCache& cache);
    explicit MissionAnimations(std::vector<MissionAnimationBinding> bindings);
    MissionAnimations(const MissionAnimations&)=delete;
    MissionAnimations& operator=(const MissionAnimations&)=delete;
    AnimationController& controller(std::uint32_t instance);
    const AnimationController& controller(std::uint32_t instance) const;
    const std::vector<AnimationDriverAction>& actions(std::uint32_t instance) const;
    const std::vector<CharacterBonePose>* pose(std::uint32_t instance) const noexcept;
    std::size_t size() const noexcept { return instances_.size(); }
private:
    struct Instance {
        std::unique_ptr<AnimationController> controller;
        std::vector<AnimationDriverAction> actions;
    };
    std::map<std::uint32_t,Instance> instances_;
};
} // namespace dfre
