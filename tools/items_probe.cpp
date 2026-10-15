#include "dfre/items.hpp"
#include "dfre/mission.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        const dfre::AssetStore assets(argv[1]);const auto catalog=dfre::load_item_catalog(assets);
        std::size_t missions=0,objects=0,fallbacks=0;
        for(const auto& name:assets.missions()) {
            const auto mission=dfre::load_mission(assets,name);++missions;
            for(const auto& group:mission.object_groups)for(const auto& object:group) {if(!catalog.find_exact(object.type_id))++fallbacks;catalog.find(object.type_id);++objects;}
        }
        std::cout<<catalog.entries().size()<<" items; "<<missions<<" missions; "<<objects<<" objects; "<<fallbacks<<" original null fallbacks\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
