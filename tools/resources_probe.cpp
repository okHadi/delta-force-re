#include "dfre/resources.hpp"
#include <iostream>
int main(int argc,char** argv){
    if(argc<2||argc>3)return 2;
    try {
        const dfre::AssetStore assets(argv[1]);dfre::MissionAssetCache cache(assets);
        const auto names=argc==3?std::vector<std::string>{argv[2]}:assets.missions();std::size_t objects=0,visible=0;
        for(const auto& name:names){const auto scene=cache.load(name);
            for(std::size_t group=0;group<scene.objects.size();++group){
                if(scene.objects[group].size()!=scene.mission.object_groups[group].size())throw dfre::AssetError("Missing mission object binding");
                for(const auto& binding:scene.objects[group]){++objects;if(!binding.model.empty()){++visible;cache.model(binding.model);}if(!binding.character.empty())cache.character(binding.character);if(!binding.animations.empty()&&cache.animations(binding.animations).empty())throw dfre::AssetError("Empty character animation definitions");}
            }
        }
        std::cout<<names.size()<<" missions; "<<objects<<" objects; "<<visible<<" model bindings; "<<cache.model_count()<<" models; "<<cache.character_count()<<" character files; "<<cache.animation_count()<<" animation definitions\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
