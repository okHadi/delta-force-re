#include "dfre/mission_live.hpp"
#include <cstring>
#include <map>
#include <memory>

namespace dfre {
namespace {
std::uint32_t word(const AiActorImage& image,std::size_t at){
    std::uint32_t value;std::memcpy(&value,image.data()+at,4);return value;
}
std::int32_t signed_word(const AiActorImage& image,std::size_t at){
    std::int32_t value;std::memcpy(&value,image.data()+at,4);return value;
}
std::int32_t signed_bits(std::uint32_t value){std::int32_t result;std::memcpy(&result,&value,4);return result;}
}
void bind_mission_animations(MissionTickState& state,MissionAnimations& animations){
    for(std::size_t group=0;group<4;++group)for(std::size_t slot=0;slot<state.tables.groups[group].size();++slot){
        const auto instance=state.tables.groups[group][slot].field_18;
        if(instance)state.bind_animation({std::uint8_t(group),slot},animations.controller(instance),animations.actions(instance));
    }
    state.world.frame_mask=[&state,&animations](const AiActorRef& ref){
        const auto* actor=state.actor_at(ref);
        if(!actor || !actor->field_18)throw MissionTickError("Frame mask needs a character instance");
        return animations.controller(actor->field_18).frame_mask();
    };
}
void bind_mission_geometry(MissionTickState& state,const MissionGeometry& geometry,
    MissionGeometryMatrixSource matrices){
    state.world.bounds=[&geometry](std::uint32_t handle){return geometry.bounds(handle);};
    // GroundEntity views keep raw definition pointers. Keep their heap addresses
    // stable when a target is queried again, including duplicate target entries.
    auto storage=std::make_shared<std::map<std::uint32_t,MissionGroundEntity>>();
    state.world.ground_entity=[&state,&geometry,matrices=std::move(matrices),storage](
        const AiActorRef& ref,const std::array<std::int32_t,3>& query)->std::optional<GroundEntity>{
        if(ref.group>=4)throw MissionTickError("Ground entity needs a mission actor");
        auto built=geometry.ground_entity(snapshot_mission_actor(state,ref),query,matrices);
        if(!built)return std::nullopt;
        auto& retained=(*storage)[actor_handle(ref.group,ref.index)];
        if(!retained.definition)retained.definition=std::make_unique<GroundDefinition>();
        *retained.definition=std::move(*built->definition);
        retained.entity=built->entity;
        if(built->entity.definition)retained.entity.definition=retained.definition.get();
        if(built->entity.alternate)retained.entity.alternate=retained.definition.get();
        return retained.entity;
    };
    state.world.ground_object=[&state](std::uint32_t handle)->std::optional<GroundObject>{
        if(handle<kActorHandleBase)throw MissionTickError("Ground actor handle is invalid");
        const auto key=handle-kActorHandleBase;
        const auto group=key/0x10000u,slot=key%0x10000u;
        if(group>=4 || slot>=state.tables.groups[group].size())throw MissionTickError("Ground actor exceeds its table");
        const auto image=snapshot_mission_actor(state,{std::uint8_t(group),slot});
        GroundObject result;
        for(std::size_t axis=0;axis<3;++axis)result.position[axis]=signed_word(image,0x24+axis*4);
        result.rotation=word(image,0x90);result.offset_x=signed_word(image,0x3c);result.offset_y=signed_word(image,0x40);
        return result;
    };
}
MissionGeometryMatrixSource initialized_mission_matrices(const MissionRuntimeState& initial,
    const MissionAssetCache& cache,const MissionAnimations& animations){
    return [&initial,&cache,&animations](const MissionGeometryModel& binding,const AiActorImage& image,const auto&){
        const CharacterModel* character=nullptr;
        const auto instance=word(image,0x18);
        if(binding.model && binding.model->render_types[0]==0x6f726730u && instance){
            if(instance<kCharacterInstanceBase)throw MissionTickError("Character instance handle is invalid");
            const auto item=initial.character_instances.at(instance-kCharacterInstanceBase);
            const auto handle=initial.definitions.at(item).character;
            for(const auto& entry:initial.characters)if(entry.second==handle){character=&cache.character(entry.first);break;}
            if(!character)throw MissionTickError("Character resource is not bound");
        }
        const std::vector<std::uint8_t> flags(binding.model?binding.model->lods.at(0).meshes.size():0,0);
        return mission_geometry_callback_matrices(binding,image,character,animations.pose(instance),&flags,std::uint8_t(0));
    };
}
ActorQueryData mission_actor_query_data(MissionTickState& state,const AiActorRef& self,
    const MissionGeometry& geometry,const AssetBytes& height_bounds,
    std::vector<ActorZoneRect> zones,bool double_score,MissionGeometryMatrixSource matrices){
    if(self.group!=3 || !state.actor_at(self))throw MissionTickError("NPC queries need a group-three actor");
    ActorQueryData data;data.zones=std::move(zones);data.water_level=state.world.water_level;
    data.double_score_67f548_bit80=double_score;data.local_player=state.config.local_actor;
    data.local_player_excluded_67dbc9_bit8=(state.globals.flags_67dbc9&8)!=0;
    data.raw_pointer=[](std::size_t slot){return 0x7cca80u+std::uint32_t(slot)*0x114u;};
    data.sin_cos_477820=[](std::uint32_t angle,std::int32_t& sine,std::int32_t& cosine){
        const auto index=angle>>21;sine=kSineTableQ22[index];cosine=kSineTableQ22[index+512];
    };
    data.sin_cos_4776c0=sincos_q22;
    data.terrain_height=[&state](std::int32_t x,std::int32_t y){
        if(!state.world.terrain)throw MissionTickError("NPC query needs terrain");
        return ground_height_query(*state.world.terrain,x,y);
    };
    const auto frame=[&state](std::size_t slot)->const AnimationDriverAction&{
        const auto& runtime=state.runtime_for({3,slot});
        if(!runtime.driver)throw MissionTickError("NPC query needs an animation driver");
        const auto* action=runtime.driver->action(std::uint32_t(runtime.driver->current_action()));
        if(!action)throw MissionTickError("NPC action exceeds its animation table");
        return *action;
    };
    data.weapon_frame_10=[frame](std::size_t slot){return frame(slot).weight_10;};
    data.weapon_frame_14=[frame](std::size_t slot){return frame(slot).aim_14>>16;};
    data.sight_line=[&state,&geometry,&height_bounds,self,matrices=std::move(matrices)](std::size_t slot){
        if(!state.world.terrain)throw MissionTickError("Sight query needs terrain");
        const auto* source=state.actor_at(self);const auto* target=state.actor_at({3,slot});
        if(!source || !target)throw MissionTickError("Sight query actor is missing");
        const auto images=snapshot_mission_actors(state);
        std::array<std::vector<WorldHitRecord>,2> lists;
        for(std::size_t list=0;list<2;++list){
            const std::size_t group=list==0?1:0;
            std::vector<MissionGeometryActor> actors;
            for(std::size_t index=0;index<images[group].size();++index)
                actors.push_back({actor_handle(group,index),&images[group][index]});
            lists[list]=geometry.world_hit_records(actors,matrices);
        }
        std::array<std::int32_t,3> start{},end{};
        for(std::size_t axis=0;axis<3;++axis){start[axis]=signed_bits(source->position[axis]);end[axis]=signed_bits(target->position[axis]);}
        return world_segment_query(*state.world.terrain,height_bounds,state.world.water_level,lists[0],lists[1],
            actor_handle(3,self.index),actor_handle(3,slot),start,end).code;
    };
    return data;
}
} // namespace dfre
