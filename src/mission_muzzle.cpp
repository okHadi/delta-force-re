#include "dfre/mission_muzzle.hpp"
#include "dfre/model_characters.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace dfre {
namespace {
std::uint32_t word(const AiActorImage& image,std::size_t at) {
    std::uint32_t value;std::memcpy(&value,image.data()+at,4);return value;
}
std::int32_t integer(const AiActorImage& image,std::size_t at) {
    std::int32_t value;std::memcpy(&value,image.data()+at,4);return value;
}
MissionMuzzleResources resources(const MissionRuntimeState& state,const MissionAssetCache& cache) {
    MissionMuzzleResources result;result.definitions=state.definitions;
    for(const auto& binding:state.models)result.models.push_back({binding.handle,&cache.model(binding.name)});
    for(std::size_t slot=0;slot<state.character_instances.size();++slot) {
        const auto item=state.character_instances[slot];
        const auto character=state.definitions.at(item).character;
        const CharacterModel* source=nullptr;
        for(const auto& entry:state.characters)if(entry.second==character){source=&cache.character(entry.first);break;}
        if(!source)throw std::invalid_argument("Muzzle character resource is not loaded");
        result.characters.push_back({kCharacterInstanceBase+std::uint32_t(slot),source});
    }
    return result;
}
}
Matrix4 original_muzzle_attachment() {
    return compose_pose({}, {0xeaab0000u,0xd1c80000u,0xeaab0000u});
}
MissionMuzzle::MissionMuzzle(const MissionRuntimeState& state,const MissionAssetCache& cache,
    const MissionAnimations& animations):MissionMuzzle(resources(state,cache),animations) {}
MissionMuzzle::MissionMuzzle(MissionMuzzleResources input,const MissionAnimations& animations)
    :resources_(std::move(input)),animations_(&animations),attachment_(original_muzzle_attachment()) {
    if(resources_.definitions.empty())throw std::invalid_argument("Muzzle needs the loaded item table");
    for(std::size_t index=0;index<kWeaponCount;++index) {
        const auto& definition=weapon_definitions()[index];
        weapons_[index]={std::uint32_t(runtime_item_index(resources_.definitions,definition.primary_item)),
            std::uint32_t(runtime_item_index(resources_.definitions,definition.secondary_item))};
    }
    for(const auto& binding:resources_.models)
        if(!binding.handle || !binding.model || binding.model->lods.empty())
            throw std::invalid_argument("Muzzle model binding needs its first LOD");
    for(const auto& binding:resources_.characters)
        if(!binding.instance || !binding.character || !binding.character->runtime_prepared)
            throw std::invalid_argument("Muzzle character binding needs prepared data");
}
const MissionMuzzleWeapon& MissionMuzzle::weapon(std::uint32_t index) const {
    if(index>=weapons_.size())throw std::invalid_argument("Muzzle weapon exceeds the runtime table");
    return weapons_[index];
}
const Model& MissionMuzzle::model(std::uint32_t handle) const {
    for(const auto& binding:resources_.models)if(binding.handle==handle)return *binding.model;
    throw std::invalid_argument("Muzzle model handle is not loaded");
}
Fixed3 MissionMuzzle::hardpoint(std::uint32_t handle,std::uint32_t index) const {
    const auto& points=model(handle).lods.front().hardpoints;
    if(index>=points.size())throw std::invalid_argument("Muzzle hardpoint exceeds the loaded model");
    return {points[index][0],points[index][1],points[index][2]};
}
MuzzleResult MissionMuzzle::normal(const AiActorImage& actor,std::uint32_t index,const MissionMuzzleView& view) const {
    const auto& binding=weapon(index);
    const auto& definition=weapon_definitions()[index];
    MuzzleInputs input;
    input.actor_position={integer(actor,0x24),integer(actor,0x28),integer(actor,0x2c)};
    input.actor_angles={word(actor,0x30),word(actor,0x34),word(actor,0x38)};
    input.view_delta=view.view_delta;input.eye_height=view.eye_height;
    input.weapon_muzzle={definition.muzzle_x,definition.muzzle_y,definition.muzzle_z};
    input.weapon_aim={std::uint32_t(definition.aim_x),std::uint32_t(definition.aim_y),std::uint32_t(definition.aim_z)};
    auto result=compute_muzzle(input);
    const auto handle=resources_.definitions[binding.primary_definition].model;
    const auto offset=handle ? hardpoint(handle,std::uint32_t(definition.aim_field)) : Fixed3{};
    result.origin=compute_barrel_tip(result.origin,result.aim,offset);
    return result;
}
Matrix4& MissionMuzzle::slot_seven() {
    if(!slot_seven_)throw std::invalid_argument("Muzzle needs prior slot-seven scratch state");
    return *slot_seven_;
}
Matrix4& MissionMuzzle::animated_slot(std::uint32_t graphic,const AiActorImage& actor) {
    if(slot_source_)return slot_source_(graphic,actor);
    const auto& source=model(graphic);
    const auto& lod=source.lods.front();
    std::vector<ModelMatrix> matrices;
    if(source.render_types[0]==0x6f726730u) {
        const auto instance=word(actor,0x18);
        const CharacterModel* character=nullptr;
        if(instance) {
            for(const auto& binding:resources_.characters)if(binding.instance==instance){character=binding.character;break;}
            if(!character)throw std::invalid_argument("Animated muzzle character is not bound");
        }
        matrices=character_draw_matrices(character,animations_->pose(instance),&lod,word(actor,0x34),lod.meshes.size());
    } else if(source.render_types[0]==0x676e7263u) {
        ModelInstanceFields fields;
        for(std::size_t axis=0;axis<3;++axis) {
            fields.translation[axis]=integer(actor,0x24+axis*4);fields.angles[axis]=word(actor,0x30+axis*4);
        }
        matrices=model_instance_local_matrices(lod.meshes.size(),fields);
    } else throw std::invalid_argument("Animated muzzle graphic tag needs a slot source");
    if(matrices.size()>7)slot_seven_=Matrix4{matrices[7]};
    return slot_seven();
}
bool MissionMuzzle::animated(ProjectileHitActor actor,
    const std::function<void(ProjectileHitActor,const MuzzleResult&)>& shot) {
    ProjectileMuzzleBindings bindings;
    bindings.weapon=[this](std::uint32_t index) {
        const auto& binding=weapon(index);
        return ProjectileMuzzleWeapon{resources_.definitions[binding.secondary_definition].model,
            std::uint32_t(weapon_definitions()[index].aim_field)};
    };
    bindings.slot_seven=[this](std::uint32_t graphic,ProjectileHitActor actor)->Matrix4& {
        if(!actor.image)throw std::invalid_argument("Animated muzzle needs an actor image");
        return animated_slot(graphic,*actor.image);
    };
    bindings.hardpoint=[this](auto handle,auto index){return hardpoint(handle,index);};
    bindings.attachment=attachment_;bindings.shot=shot;
    return projectile_animated_muzzle(actor,bindings);
}
} // namespace dfre
