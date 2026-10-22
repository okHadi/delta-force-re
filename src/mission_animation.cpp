#include "dfre/mission_animation.hpp"
#include <stdexcept>

namespace dfre {
namespace {
template<class T> const std::string& resource_name(const T& registry,std::uint32_t handle) {
    for(const auto& entry:registry)if(entry.second==handle)return entry.first;
    throw std::invalid_argument("Character instance has an unbound resource");
}
std::vector<MissionAnimationBinding> bindings(const MissionRuntimeState& state,const MissionAssetCache& cache) {
    std::vector<MissionAnimationBinding> result;
    std::map<std::uint32_t,std::vector<AnimationDriverAction>> actions;
    for(std::size_t slot=0;slot<state.character_instances.size();++slot){
        const auto item=state.character_instances[slot];
        if(item==0 || item>=state.definitions.size())throw std::invalid_argument("Character item exceeds the definition table");
        const auto& definition=state.definitions[item];
        const auto& character=cache.character(resource_name(state.characters,definition.character));
        const auto animation=definition.animation;
        if(actions.find(animation)==actions.end()){
            const auto text=cache.animations(resource_name(state.animations,animation));
            actions.emplace(animation,build_animation_driver_actions(decode_animation_definition(text),
                original_animation_compiled_actions()));
        }
        result.push_back({kCharacterInstanceBase+std::uint32_t(slot),&character,actions.at(animation)});
    }
    return result;
}
}
MissionAnimations::MissionAnimations(const MissionRuntimeState& state,const MissionAssetCache& cache)
    :MissionAnimations(bindings(state,cache)) {}
MissionAnimations::MissionAnimations(std::vector<MissionAnimationBinding> input) {
    for(auto& binding:input){
        if(binding.instance<kCharacterInstanceBase || !binding.character)
            throw std::invalid_argument("Character instance needs a handle and a prepared character");
        if(instances_.find(binding.instance)!=instances_.end())throw std::invalid_argument("Duplicate character instance");
        Instance instance;
        instance.controller=std::make_unique<AnimationController>(*binding.character);
        instance.actions=std::move(binding.actions);
        instances_.emplace(binding.instance,std::move(instance));
    }
}
AnimationController& MissionAnimations::controller(std::uint32_t instance) {return *instances_.at(instance).controller;}
const AnimationController& MissionAnimations::controller(std::uint32_t instance) const {return *instances_.at(instance).controller;}
const std::vector<AnimationDriverAction>& MissionAnimations::actions(std::uint32_t instance) const {return instances_.at(instance).actions;}
const std::vector<CharacterBonePose>* MissionAnimations::pose(std::uint32_t instance) const noexcept {
    const auto found=instances_.find(instance);
    return found==instances_.end()?nullptr:&found->second.controller->pose();
}
void MissionAnimations::release(std::uint32_t instance) {
    if(!instance)return;
    if(instances_.erase(instance)!=1)throw std::out_of_range("Character release needs a current instance");
}
} // namespace dfre
