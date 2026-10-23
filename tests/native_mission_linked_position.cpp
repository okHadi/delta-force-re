#include "dfre/mission_live.hpp"
#include "dfre/mission_pickup_live.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

#define CHECK(x) do{if(!(x))throw std::runtime_error("linked position: " #x);}while(false)
int main(){try{
    using namespace dfre;using U=std::uint32_t;
    const auto position=[](const AiActorState* actor){return std::array<U,3>{actor->position[0],actor->position[1],actor->position[2]};};
    for(std::uint8_t group=0;group<4;++group){
        MissionTickState state;AiActorTables tables;for(auto& rows:tables.groups)rows.resize(2);
        const AiActorRef owner{3,0},target{group,group==3?1u:0u};
        auto& source=tables.groups[3][0];source.active=true;source.definition_index=1;source.field_6c=1;source.field_18=kCharacterInstanceBase;
        source.position[0]=0x7fffffffu;source.position[1]=0x80000000u;source.position[2]=0xffffffffu;
        auto& carried=tables.groups[target.group][target.index];carried.active=true;carried.definition_index=2;carried.angle_yaw=0x11223344;carried.position[0]=7;carried.position[1]=8;carried.position[2]=9;
        std::vector<AiItemDefinition> definitions(3);definitions[2].id=2;
        init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},0);
        CharacterModel character;character.runtime_prepared=true;character.bone_map={0};
        MissionAnimations animations({{kCharacterInstanceBase,&character,{{0,0,0,false,0,0,0,0}}}});
        bind_mission_animations(state,animations);bind_mission_linked_position(state);
        ActorPickupState shared;auto pickup=mission_pickup_live_effect(shared,{});
        ActorCollisionCall call{};call.site=ActorCollisionSite::pickup;call.arguments={actor_handle(3,0),actor_handle(target.group,target.index)};call.count=2;
        CHECK(pickup(state,call).has_value());
        CHECK(state.runtime_for(owner).link_90==mission_actor_source_address(target.group,target.index));
        const auto prior=snapshot_mission_actor(state,target);
        state.runtime_for(owner).step_cache={1,-2,3};MissionTickReport report;mission_actor_update(state,owner,0,report);
        CHECK(position(state.actor_at(target))==position(state.actor_at(owner)));
        CHECK(state.actor_at(target)->angle_yaw==0x11223344);
        auto expected=prior;const auto after=snapshot_mission_actor(state,target);for(std::size_t offset=0x24;offset<0x30;++offset)expected[offset]=after[offset];CHECK(expected==after);
        state.actor_at(target)->active=false;state.actor_at(target)->definition_index=0;
        state.world.linked_position(mission_actor_source_address(target.group,target.index),{11,22,33});CHECK((position(state.actor_at(target))==std::array<U,3>{11,22,33}));
        state.world.linked_position(mission_actor_source_address(3,0),{-1,std::numeric_limits<std::int32_t>::min(),0});CHECK((position(state.actor_at(owner))==std::array<U,3>{0xffffffffu,0x80000000u,0}));
        state.runtime_for(owner).link_90=mission_actor_source_address(3,0);mission_actor_update(state,owner,0,report);CHECK((position(state.actor_at(owner))==std::array<U,3>{0,0x7ffffffeu,3}));
        for(auto invalid:{0u,actor_handle(target.group,target.index),mission_actor_source_address(target.group,target.index)+1,mission_actor_source_address(0,3),0x822e80u}){
            const auto unchanged=snapshot_mission_actors(state);bool rejected=false;try{state.world.linked_position(invalid,{44,55,66});}catch(const MissionTickError&){rejected=true;}CHECK(rejected&&snapshot_mission_actors(state)==unchanged);
        }
        state.tables.groups[0].resize(1025);bool rejected=false;try{state.world.linked_position(0x822e80,{1,2,3});}catch(const MissionTickError&){rejected=true;}CHECK(rejected);
    }
    std::cout<<"linked position checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
