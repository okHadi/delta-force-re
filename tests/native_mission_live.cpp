#include "dfre/mission_live.hpp"
#include "dfre/mission_pickup_live.hpp"
#include "dfre/mission_actor_lifecycle.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("mission live: " #x);}while(false)
namespace {
void pickup_collision_delivery(){
    using namespace dfre;using U=std::uint32_t;
    Model wall;wall.version=5;wall.render_types[0]=0x676e7263;wall.lods.resize(1);auto& lod=wall.lods[0];lod.meshes.resize(1);lod.meshes[0].raw.resize(112);
    const auto write=[](auto& bytes,std::size_t at,U value){std::memcpy(bytes.data()+at,&value,4);};
    std::array<std::uint8_t,72> volume{};write(volume,0,1);write(lod.meshes[0].raw,0x1c,1);
    for(std::size_t axis=0;axis<3;++axis){write(volume,32+axis*4,32768);write(volume,48+axis*8,U(-32768));write(volume,52+axis*8,32768);write(lod.meshes[0].raw,0x40+axis*8,U(-65536));write(lod.meshes[0].raw,0x44+axis*8,65536);}
    lod.collision_volumes={volume};MissionGeometry geometry({{1,&wall,{131072,196608},{},{}}});
    for(std::uint8_t group=0;group<4;++group)for(bool side_two:{false,true}){
        MissionTickState state;AiActorTables tables;for(auto g:{0u,1u,2u,3u})tables.groups[g].resize(2);
        const AiActorRef carrier{3,0},carried{group,group==3?1u:0u},base{1,1};
        auto& player=tables.groups[3][0];player.active=true;player.definition_index=1;player.field_6c=1;player.field_70=side_two?2:1;
        for(const auto ref:{carried,base}){auto& actor=tables.groups[ref.group][ref.index];actor.active=true;actor.definition_index=ref.group==base.group&&ref.index==base.index?3:2;actor.field_10=1;}
        std::vector<AiItemDefinition> defs(4);defs[2].id=side_two?0xfc4:0xfc3;defs[2].flags24=0x10;defs[2].field_28=6;defs[3].id=side_two?0xfcc:0xfcb;defs[3].flags24=0x10;
        MissionTickConfig config;config.local_actor=0;init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(defs),{},0);state.globals.mission_67f528=1;
        ActorPickupState shared;std::array<ActorPickupClient,17> clients{};shared.clients=&clients;clients[2]={1,actor_handle(3,0),7};
        MissionAnimations animations(std::vector<MissionAnimationBinding>{});std::array<MissionAudioRegistration,1024> registrations{};registrations[0]={42,mission_actor_source_address(group,carried.index),1,0};MissionActorLifecycle lifecycle(state,animations,registrations,{});
        std::vector<U> order;MissionPickupLiveBindings bindings;
        bindings.message=[&](auto& live,auto& same,auto transport,auto id,const auto& bytes){
            CHECK(&live==&state&&&same==&shared);
            if(id==0x20){CHECK(transport==ActorPickupTransport::broadcast&&bytes.size()==3&&bytes[1]==0&&bytes[2]==0);if(bytes[0]==0x14){CHECK(order.empty());order.push_back(1);}else{CHECK(bytes[0]==0x13&&order==std::vector<U>({1,2}));CHECK((side_two?same.team_two_6e81c4:same.team_one_6e815c)==1);order.push_back(3);}}
            else{const auto slot=(mission_actor_source_address(group,carried.index)-0x7cca80u)/0x114u;CHECK(id==0x10&&transport==ActorPickupTransport::reliable&&bytes==std::vector<std::uint8_t>({std::uint8_t(slot),std::uint8_t(slot>>8)}));CHECK(!live.actor_at(carried)->active&&registrations[0].definition==0);order.push_back(5);}
        };
        bindings.local_flag=[&](auto& live,auto&){CHECK(live.runtime_for(carrier).link_90==mission_actor_source_address(group,carried.index));order.push_back(2);};
        bindings.remove=[&](auto& live,auto&,const auto& ref){CHECK(ref.group==carried.group&&ref.index==carried.index&&live.runtime_for(carrier).link_90==mission_actor_source_address(group,carried.index));order.push_back(4);lifecycle.remove(ref);};
        bind_mission_actor_collision(state,geometry,mission_pickup_live_effect(shared,bindings));
        CHECK(state.world.target_collision(carrier,carried)==1&&state.runtime_for(carrier).link_90==mission_actor_source_address(group,carried.index));
        CHECK(state.world.target_collision(carrier,base)==1&&state.runtime_for(carrier).link_90==0);
        CHECK(order==std::vector<U>({1,2,3,4,5})&&clients[2].deliveries_1bc==8&&!state.actor_at(carried)->active);
        state.runtime_for(carrier).link_90=mission_actor_source_address(group,carried.index);CHECK(state.world.target_collision(carrier,base)==1&&state.runtime_for(carrier).link_90==0&&clients[2].deliveries_1bc==8);
        for(auto invalid:{actor_handle(group,carried.index),mission_actor_source_address(group,carried.index)+1,mission_actor_source_address(0,3)}){state.runtime_for(carrier).link_90=invalid;bool rejected=false;try{state.world.target_collision(carrier,base);}catch(const MissionTickError&){rejected=true;}CHECK(rejected&&state.runtime_for(carrier).link_90==invalid&&clients[2].deliveries_1bc==8);}
        state.tables.groups[0].resize(1025);state.tables.groups[0][1024].active=true;state.tables.groups[0][1024].definition_index=2;
        state.runtime_for(carrier).link_90=0x822e80;bool runtime_pool_rejected=false;try{state.world.target_collision(carrier,base);}catch(const MissionTickError&){runtime_pool_rejected=true;}CHECK(runtime_pool_rejected&&state.runtime_for(carrier).link_90==0x822e80&&clients[2].deliveries_1bc==8);
    }
}
}
int main(){try{
    using namespace dfre;
    CharacterModel character;character.runtime_prepared=true;character.bone_map={0};
    MissionAnimations animations({{kCharacterInstanceBase,&character,{{0,0,0,false,0,0,0x1234,0x20000}}}});
    MissionTickState state;AiActorTables tables;tables.groups[3].resize(1);
    auto& actor=tables.groups[3][0];actor.active=true;actor.definition_index=1;
    actor.field_10=17;actor.field_18=kCharacterInstanceBase;
    actor.position[0]=10;actor.position[1]=20;actor.position[2]=30;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0);
    Model model;model.version=5;model.render_types[0]=0x676e7263;model.lods.resize(1);
    MissionGeometry geometry({{17,&model,{100,200},{},{}}});
    bind_mission_animations(state,animations);bind_mission_geometry(state,geometry);
    auto& runtime=state.runtime_for({3,0});
    CHECK(runtime.animation==&animations.controller(kCharacterInstanceBase));CHECK(runtime.driver.has_value());
    CHECK(state.world.frame_mask({3,0})==0);CHECK(state.world.bounds(17).height_60==200);
    auto initial=state.world.ground_entity({3,0},{10,20,30});CHECK(initial && initial->position[0]==10);
    const auto* retained=initial->definition;
    state.tables.groups[3][0].position[0]=50;
    auto current=state.world.ground_entity({3,0},{50,20,30});
    CHECK(current && current->position[0]==50 && current->definition==retained);
    CHECK(!state.world.ground_entity({3,0},{151,20,30}));
    runtime.link_90=123;
    const std::int32_t offset=-500;
    std::memcpy(state.stored_actor_images[3][0].data()+0x3c,&offset,4);
    const auto ground=state.world.ground_object(actor_handle(3,0));
    CHECK(ground && ground->position[0]==50 && ground->rotation==123 && ground->offset_x==-500);
    bool rejected=false;try{state.world.ground_object(1);}catch(const MissionTickError&){rejected=true;}CHECK(rejected);
    const TerrainGeometry terrain(AssetBytes(1024*1024),AssetBytes(1024*1024),std::vector<std::uint32_t>(65536));
    state.world.terrain=&terrain;
    const AssetBytes bounds(1024*1024);
    const auto data=mission_actor_query_data(state,{3,0},geometry,bounds,{},true,{});
    CHECK(data.weapon_frame_10(0)==0x1234 && data.weapon_frame_14(0)==2);
    CHECK(data.double_score_67f548_bit80 && data.sight_line(0)==1 && data.terrain_height(0,0)==0);
    std::int32_t sine=0,cosine=0;data.sin_cos_477820(0x21123456,sine,cosine);
    CHECK(sine==kSineTableQ22[0x21123456u>>21] && cosine==kSineTableQ22[(0x21123456u>>21)+512]);
    CHECK(data.raw_pointer(1)==0x7ccb94);
    auto updated=snapshot_mission_actor(state,{3,0});
    const auto put=[&](std::size_t at,std::uint32_t value){std::memcpy(updated.data()+at,&value,4);};
    put(0x24,0xffffff9c);put(0x3c,900);put(0x44,0xfffff000);put(0x74,0x12340063);put(0x98,0x87654321);
    updated[0x6e]=6;
    apply_mission_actor_image(state,{3,0},updated);
    CHECK(snapshot_mission_actor(state,{3,0})==updated);
    CHECK(state.extra_for({3,0}).field_74==99 && state.runtime_for({3,0}).vertical_44==-4096);
    CHECK(state.tables.groups[3][0].position[0]==0xffffff9c);
    auto invalid=updated;const std::uint32_t invalid_person=12345;std::memcpy(invalid.data()+0xc,&invalid_person,4);
    rejected=false;try{apply_mission_actor_image(state,{3,0},invalid);}catch(const MissionTickError&){rejected=true;}
    CHECK(rejected && snapshot_mission_actor(state,{3,0})==updated);
    apply_mission_actor_image(state,{3,0},AiActorImage{});
    CHECK(snapshot_mission_actor(state,{3,0})==AiActorImage{} && !state.runtime_for({3,0}).driver);
    {
        Model wall;wall.version=5;wall.render_types[0]=0x676e7263;wall.lods.resize(1);
        auto& lod=wall.lods[0];lod.meshes.resize(1);lod.meshes[0].raw.resize(112);
        std::array<std::uint8_t,72> volume{};
        const auto write=[](auto& bytes,std::size_t at,std::uint32_t value){std::memcpy(bytes.data()+at,&value,4);};
        write(volume,0,1);write(lod.meshes[0].raw,0x1c,1);
        for(std::size_t axis=0;axis<3;++axis){
            write(volume,32+axis*4,32768);write(volume,48+axis*8,std::uint32_t(-32768));write(volume,52+axis*8,32768);
            write(lod.meshes[0].raw,0x40+axis*8,std::uint32_t(-65536));write(lod.meshes[0].raw,0x44+axis*8,65536);
        }
        lod.collision_volumes={volume};
        MissionGeometry physical({{1,&wall,{131072,196608},{},{}}});
        MissionTickState joined;AiActorTables actors;actors.groups[3].resize(1);actors.groups[0].resize(1);
        actors.groups[3][0].active=true;actors.groups[3][0].definition_index=1;actors.groups[3][0].field_6c=1;
        actors.groups[0][0].active=true;actors.groups[0][0].definition_index=1;actors.groups[0][0].field_10=1;
        std::vector<AiItemDefinition> definitions(2);definitions[1].id=0xfaa;
        init_mission_tick(joined,{},std::move(actors),AiItemDefinitionTable(definitions),{},0);
        const auto target_seed=snapshot_mission_actor(joined,{0,0});
        bind_mission_actor_collision(joined,physical,{});
        CHECK(joined.world.target_collision({3,0},{0,0})==1 && joined.tables.groups[3][0].position[0]==0x14000);
        joined.tables.groups[3][0].position[0]=0;definitions[1].flags24=0x10;
        joined.definitions=AiItemDefinitionTable(definitions);
        std::vector<ActorCollisionSite> calls;
        bind_mission_actor_collision(joined,physical,[&](MissionTickState& live,const ActorCollisionCall& call)->std::optional<std::uint32_t>{
            calls.push_back(call.site);
            if(call.site==ActorCollisionSite::refill){
                CHECK(live.tables.groups[3][0].position[0]==0);
                live.extra_for({3,0}).field_74=77;live.tables.groups[3][0].position[0]=333;live.tables.groups[3][0].field_6c=0;
            }else{
                CHECK(call.site==ActorCollisionSite::remove && live.extra_for({3,0}).field_74==77 && live.tables.groups[3][0].position[0]==333);
                apply_mission_actor_image(live,{0,0},AiActorImage{});
            }
            return 17;
        });
        CHECK(joined.world.target_collision({3,0},{0,0})==1);
        CHECK(calls==std::vector<ActorCollisionSite>({ActorCollisionSite::refill,ActorCollisionSite::remove}));
        CHECK(joined.tables.groups[3][0].position[0]==333 && joined.extra_for({3,0}).field_74==77);
        CHECK(snapshot_mission_actor(joined,{0,0})==AiActorImage{});
        apply_mission_actor_image(joined,{0,0},target_seed);joined.tables.groups[3][0].position[0]=0;joined.tables.groups[3][0].field_6c=1;
        bind_mission_actor_collision(joined,physical,[](MissionTickState& live,const ActorCollisionCall&)->std::optional<std::uint32_t>{
            live.extra_for({3,0}).field_74=99;throw std::runtime_error("effect stopped");
        });
        rejected=false;try{joined.world.target_collision({3,0},{0,0});}catch(const std::runtime_error&){rejected=true;}
        CHECK(rejected && joined.extra_for({3,0}).field_74==99 && joined.tables.groups[0][0].active);
        bind_mission_actor_collision(joined,physical,{});
        rejected=false;try{joined.world.target_collision({3,0},{0,0});}catch(const MissionTickError&){rejected=true;}
        CHECK(rejected && joined.tables.groups[0][0].active && joined.extra_for({3,0}).field_74==99);
    }
    {
        MissionTickState live;AiActorTables actors;actors.groups[3].resize(1);
        actors.groups[3][0].person_slot=0;MissionTickConfig config;config.local_actor=0;
        init_mission_tick(live,config,std::move(actors),AiItemDefinitionTable{},std::vector<AiPersonState>(2),0);
        bind_mission_player_burst(live);
        live.person_extra(0).field_6c=7;live.person_extra(1).field_6c=9;
        live.weapons.select_slot(3);
        CHECK(live.person_extra(0).field_6c==0 && live.person_extra(1).field_6c==9);
        live.tables.groups[3][0].person_slot=1;live.weapons.select_slot(4);
        CHECK(live.person_extra(1).field_6c==0);
        live.tables.groups[3][0].person_slot.reset();
        rejected=false;try{live.weapons.burst_counter();}catch(const MissionTickError&){rejected=true;}
        CHECK(rejected);
    }
    pickup_collision_delivery();std::cout<<"mission live checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
