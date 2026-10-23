#include "dfre/scene_models.hpp"
#include "dfre/player.hpp"
#include "dfre/water.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::invalid_argument("Use: live_world_probe GAME_FOLDER MISSION");
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
        auto state=dfre::assemble_mission_runtime(resources,cache.items(),terrain.terrain.geometry,nullptr,config,{},[&](std::string_view name)->const dfre::Model&{return cache.model(name);});
        auto seed=dfre::seed_mission_actors(state);
        std::size_t slots=0;
        for(std::size_t group=0;group<4;++group)for(std::size_t slot=0;slot<seed.images[group].size();++slot){
            const auto image=dfre::merge_live_actor_image(seed.images[group][slot],seed.tables.groups[group][slot],seed.extras[group][slot],seed.person_encoding[group][slot]);
            if(image!=seed.images[group][slot])throw std::runtime_error("Actor seed changed a stored byte");
            ++slots;
        }
        if(!state.local_player || !state.actors)throw std::runtime_error("Mission has no local player");
        const auto& actor=state.actors->actor(state.local_player->actor);
        dfre::VoxelCamera camera;
        for(std::size_t i=0;i<3;++i)camera.position[i]=actor.pose[i];
        const auto eye=std::uint32_t(camera.position[2])+0xb333u;
        std::memcpy(&camera.position[2],&eye,sizeof(eye));
        camera.yaw=std::uint32_t(actor.pose[3]);camera.pitch=std::uint32_t(actor.pose[4]);
        const auto view=dfre::make_scene_model_view(camera.position,{camera.yaw,camera.pitch,0},{0,0,639,479},90);
        camera.focal_distance=view.camera.projection.focal_length;
        dfre::MissionSceneFrame32 baseline;
        {dfre::MissionScene32 renderer(state,cache,terrain,gamma_bytes);baseline=renderer.render(terrain,camera);}
        dfre::MissionScene32 renderer(state,cache,terrain,gamma_bytes);
        dfre::MissionSceneLiveInputs32 live;live.actors=&seed.images;
        const auto current=renderer.render(terrain,camera,{},live);
        if(current.terrain.pixels!=baseline.terrain.pixels || current.terrain.depths!=baseline.terrain.depths)
            throw std::runtime_error("Live initial scene differs from the loaded scene");
        auto hidden=seed.images;
        for(auto& group:hidden)for(auto& image:group)for(std::size_t byte=0;byte<4;++byte)image[byte]=0;
        live.actors=&hidden;
        const auto removed=renderer.render(terrain,camera,{},live);
        if(removed.records!=0 || removed.pixel_writes!=0)throw std::runtime_error("Scene read a removed actor from initial state");
        std::size_t changed=0;
        for(std::size_t i=0;i<baseline.terrain.pixels.size();++i)changed+=baseline.terrain.pixels[i]!=removed.terrain.pixels[i];
        live.actors=&seed.images;
        std::size_t pose_calls=0;
        live.pose=[&](std::uint32_t)->const std::vector<dfre::CharacterBonePose>*{++pose_calls;return nullptr;};
        const auto no_pose=renderer.render(terrain,camera,{},live);
        if(pose_calls!=state.character_instances.size())throw std::runtime_error("Live pose source was not used");
        std::size_t pose_gaps=0;
        for(const auto& gap:no_pose.gaps)pose_gaps+=gap.kind==dfre::SceneModelGapKind::missing_pose;
        std::cout<<"slots "<<slots<<" records "<<current.records<<" items "<<current.items
                 <<" writes "<<current.pixel_writes<<" pose_calls "<<pose_calls<<" missing_poses "<<pose_gaps<<" changed_pixels "<<changed<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
