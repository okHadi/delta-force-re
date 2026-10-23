#include "dfre/mission_animation.hpp"
#include "dfre/player.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv){try{
    using namespace dfre;
    if(argc!=3)throw std::invalid_argument("Use: mission_animation_probe GAME_FOLDER MISSION");
    const AssetStore store(argv[1]);MissionAssetCache cache(store);
    const auto resources=cache.load_runtime(argv[2]);
    const auto terrain=load_terrain_runtime(store,argv[2],32);
    const auto start=initial_player(resources.mission,terrain.geometry);
    if(!start)throw std::runtime_error("Mission has no single-player start");
    MissionInitConfig config;config.mode=mission_theme(resources.mission);
    config.night=mission_night(resources.mission);config.player_object_id=start->marker_object_id;
    const auto state=assemble_mission_runtime(resources,cache.items(),terrain.geometry,nullptr,config,{},[&](std::string_view name)->const dfre::Model&{return cache.model(name);});
    MissionAnimations animations(state,cache);
    std::size_t active=0,updates=0,changed=0;
    for(std::size_t slot=0;slot<state.character_instances.size();++slot){
        const auto handle=kCharacterInstanceBase+std::uint32_t(slot);
        auto& controller=animations.controller(handle);
        if(!controller.has_animation())throw std::runtime_error("Character has no initial animation");
        if(animations.pose(handle)!=&controller.pose())throw std::runtime_error("Pose source uses a separate controller");
        ++active;
        const auto initial=controller.pose();
        AnimationDriver driver(controller,animations.actions(handle));
        const AnimationDriverAction* walking=nullptr;
        for(const auto& action:driver.actions())if(action.velocity_per_tick>0 && action.animation_number>0){
            walking=&action;break;
        }
        if(!walking)continue;
        AnimationDriverState input;input.update_flags=1;input.pending_action=std::int32_t(walking->item_index);
        for(unsigned tick=0;tick<30;++tick){driver.update(input);++updates;}
        bool different=initial.size()!=controller.pose().size();
        for(std::size_t bone=0;bone<initial.size() && bone<controller.pose().size();++bone)
            different|=initial[bone].angles!=controller.pose()[bone].angles || initial[bone].scale!=controller.pose()[bone].scale;
        changed+=different;
    }
    if(active!=animations.size())throw std::runtime_error("Missing character controller");
    if(updates && !changed)throw std::runtime_error("Animation updates did not change any character pose");
    std::cout<<"instances "<<animations.size()<<" active "<<active<<" updates "<<updates<<" changed_poses "<<changed<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
