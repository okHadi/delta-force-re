#include "dfre/scene_models.hpp"
#include "dfre/player.hpp"
#include "dfre/water.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::invalid_argument("Use: scene_models_probe GAME_FOLDER MISSION OUTPUT.ppm");
        const dfre::AssetStore store(argv[1]);
        dfre::MissionAssetCache cache(store);
        const auto resources=cache.load_runtime(argv[2]);
        const auto gamma_bytes=dfre::load_terrain_gamma(std::filesystem::path(argv[1])/"INTRO.KDV");
        auto terrain=dfre::prepare_terrain_scene32(dfre::load_terrain_runtime(store,argv[2],32),gamma_bytes);
        const auto start=dfre::initial_player(resources.mission,terrain.terrain.geometry);
        if(!start)throw std::runtime_error("Mission has no single-player start");
        dfre::MissionInitConfig config;
        config.mode=dfre::mission_theme(resources.mission);
        config.night=dfre::mission_night(resources.mission);
        config.player_object_id=start->marker_object_id;
        auto state=dfre::assemble_mission_runtime(resources,cache.items(),terrain.terrain.geometry,nullptr,config);
        dfre::MissionScene32 renderer(state,cache,terrain,gamma_bytes);
        if(!state.local_player || !state.actors)throw std::runtime_error("Mission has no local player");
        const auto& actor=state.actors->actor(state.local_player->actor);
        dfre::VoxelCamera camera;
        for(std::size_t i=0;i<3;++i)camera.position[i]=actor.pose[i];
        const auto eye=std::uint32_t(camera.position[2])+0xb333u;
        std::memcpy(&camera.position[2],&eye,sizeof(eye));
        camera.yaw=std::uint32_t(actor.pose[3]);camera.pitch=std::uint32_t(actor.pose[4]);
        const auto view=dfre::make_scene_model_view(camera.position,{camera.yaw,camera.pitch,0},{0,0,639,479},90);
        camera.focal_distance=view.camera.projection.focal_length;
        const auto result=renderer.render(terrain,camera);
        const auto& frame=result.terrain;
        std::ofstream image(argv[3],std::ios::binary);
        if(!image)throw std::runtime_error("Cannot open image output");
        image<<"P6\n640 480\n255\n";
        for(auto pixel:frame.pixels)for(unsigned shift:{16u,8u,0u})image.put(char(pixel>>shift));
        if(!image)throw std::runtime_error("Cannot write image output");
        std::cout<<"models "<<result.models<<" characters "<<result.characters
            <<" records "<<result.records<<" large "<<result.large<<" near "<<result.near
            <<" people "<<result.people<<" items "<<result.items<<" writes "<<result.pixel_writes<<'\n';
        for(const auto& gap:result.gaps)
            std::cout<<"gap "<<dfre::scene_model_gap_name(gap.kind)<<" "<<gap.handle<<" "<<gap.note<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
