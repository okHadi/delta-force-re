#include "dfre/mission_live.hpp"
#include "dfre/player.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
    using namespace dfre;
    if(argc!=3)throw std::invalid_argument("Use: mission_live_probe GAME_FOLDER MISSION");
    const AssetStore store(argv[1]);MissionAssetCache cache(store);
    const auto resources=cache.load_runtime(argv[2]);const auto terrain=load_terrain_runtime(store,argv[2],32);
    const auto start=initial_player(resources.mission,terrain.geometry);
    if(!start)throw std::runtime_error("Mission has no single-player start");
    MissionInitConfig config;config.mode=mission_theme(resources.mission);config.night=mission_night(resources.mission);
    config.player_object_id=start->marker_object_id;
    const auto initial=assemble_mission_runtime(resources,cache.items(),terrain.geometry,nullptr,config);
    MissionAnimations animations(initial,cache);MissionGeometry geometry(initial,cache,store);
    MissionTickState state;init_mission_tick(state,{},initial,0);
    const auto matrices=initialized_mission_matrices(initial,cache,animations);
    bind_mission_animations(state,animations);bind_mission_geometry(state,geometry,matrices);
    state.world.terrain=&terrain.geometry;state.world.water_level=terrain.water_height;
    const auto images=snapshot_mission_actors(state);
    if(images!=initial_actor_images(initial))throw std::runtime_error("Bindings changed an initial actor record");
    std::size_t bound=0,models=0;
    for(std::size_t group=0;group<4;++group)for(std::size_t slot=0;slot<state.tables.groups[group].size();++slot){
        const auto& actor=state.tables.groups[group][slot];
        if(actor.field_18){
            const auto& controller=animations.controller(actor.field_18);
            if(state.runtime_for({std::uint8_t(group),slot}).animation!=&controller)
                throw std::runtime_error("Actor uses a separate animation controller");
            if(state.world.frame_mask({std::uint8_t(group),slot})!=controller.frame_mask())
                throw std::runtime_error("Actor frame mask is stale");
            ++bound;
        }
        if(actor.field_10){state.world.bounds(actor.field_10);++models;}
    }
    const auto height_bounds=build_terrain_height_bounds(terrain.geometry);
    if(!state.config.local_actor)throw std::runtime_error("Local actor is missing");
    const auto data=mission_actor_query_data(state,{3,*state.config.local_actor},geometry,height_bounds.front(),{},false,matrices);
    std::size_t rays=0;
    for(std::size_t slot=0;slot<state.tables.groups[3].size();++slot)if(state.tables.groups[3][slot].active){
        data.sight_line(slot);++rays;
    }
    std::cout<<"bound_actors "<<bound<<" model_bounds "<<models<<" actor_bytes_equal 1 sight_rays "<<rays<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
