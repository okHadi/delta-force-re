#include "dfre/mission_live.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("mission live: " #x);}while(false)
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
    std::cout<<"mission live checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
