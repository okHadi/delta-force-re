#include "dfre/resources.hpp"
#include "dfre/world_runtime.hpp"
#include <iostream>
#include <set>
int main(int argc,char** argv){
    if(argc<2||argc>4)return 2;
    try {
        if(argc>=3 && std::string(argv[2])=="--projectiles"){
            const dfre::AssetStore assets(argv[1]);
            const auto names=argc==4?std::vector<std::string>{argv[3]}:assets.missions();
            for(const auto& name:names){
                dfre::MissionAssetCache cache(assets);auto resources=cache.load_runtime(name);
                const auto before_calls=resources.plan.calls.size(),before_models=resources.models.size();
                cache.load_projectile_runtime(resources);
                const auto terrain=dfre::load_terrain_runtime(assets,name);
                const auto state=dfre::assemble_mission_runtime(resources,cache.items(),terrain.geometry,nullptr,{});
                std::cout<<"{\"mission\":\""<<name<<"\",\"before_models\":"<<before_models<<",\"models\":"<<resources.models.size()<<",\"loads\":[";
                for(std::size_t i=0;i<3;++i){const auto& call=resources.plan.calls.at(before_calls+i);
                    const auto& definition=state.definitions.at(call.item_index);
                    const auto handle=definition.model;std::string model;unsigned bank=0;
                    for(const auto& binding:state.models)if(binding.handle==handle){model=binding.name;bank=binding.bank;}
                    std::cout<<(i?",":"")<<"{\"id\":"<<call.item_id<<",\"index\":"<<call.item_index<<",\"handle\":"<<handle<<",\"name\":\""<<model<<"\",\"bank\":"<<bank<<'}';
                }
                const auto count=resources.models.size(),steps=resources.plan.steps.size();cache.load_projectile_runtime(resources);
                if(resources.models.size()!=count || cache.model_count()!=count || resources.plan.steps.size()!=steps)
                    throw dfre::AssetError("Repeated projectile pass changed the loaded registry");
                std::cout<<"]}\n";
            }
            return 0;
        }
        if(argc>=3 && std::string(argv[2])=="--runtime") {
            const dfre::AssetStore assets(argv[1]);
            const auto names=argc==4?std::vector<std::string>{argv[3]}:assets.missions();
            std::set<std::string> unique;std::size_t loaded=0,missing=0;
            for(const auto& name:names) {
                dfre::MissionAssetCache cache(assets);const auto scene=cache.load_runtime(name);
                if(scene.models.size()!=cache.model_count())throw dfre::AssetError("Runtime model registry count differs");
                for(const auto& entry:scene.models) {
                    if(entry.bank>=6 || cache.model(entry.name).version!=5)throw dfre::AssetError("Invalid runtime model binding");
                    unique.insert(entry.name);
                }
                loaded+=scene.models.size();missing+=scene.missing_models.size();
                for(const auto& model:scene.missing_models)std::clog<<name<<": null model handle for "<<model<<'\n';
            }
            std::cout<<names.size()<<" missions; "<<loaded<<" model loads; "<<unique.size()<<" unique models; "<<missing<<" null model handles\n";
            return 0;
        }
        if(argc==4)return 2;
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
