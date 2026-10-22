#include "dfre/mission_muzzle.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <filesystem>
#include <map>
#include <set>
#include <cctype>
namespace {
dfre::AssetBytes read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::invalid_argument("missing muzzle asset: "+p.string());return {std::istreambuf_iterator<char>(f),{}};}
void put(dfre::AiActorImage& a,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(v>>(i*8));}
void print(const dfre::MuzzleResult& r){std::cout<<' '<<r.origin.x<<' '<<r.origin.y<<' '<<r.origin.z<<' '<<r.aim.yaw<<' '<<r.aim.pitch<<' '<<r.aim.roll;}
}
int main(int argc,char** argv){try{
    if(argc!=5)throw std::invalid_argument("usage: mission_muzzle_probe ITEMS.DEF MODELS_DIR AVATAR.3DI CHARACTER.CHR");
    const auto text=read(argv[1]);dfre::ItemCatalog items(std::string(text.begin(),text.end()));
    dfre::MissionMuzzleResources r;r.definitions=dfre::build_runtime_items(items);
    std::map<std::string,dfre::Model> models;std::map<std::string,std::uint32_t> handles;
    std::map<std::string,std::filesystem::path> files;
    for(const auto& f:std::filesystem::directory_iterator(argv[2])){auto name=f.path().filename().string();for(auto& c:name)c=char(std::toupper(static_cast<unsigned char>(c)));files[name]=f.path();}
    auto load=[&](std::string name){
        for(auto& c:name)c=char(std::toupper(static_cast<unsigned char>(c)));
        if(std::filesystem::path(name).extension().empty())name+=".3DI";
        if(handles.count(name))return handles.at(name);
        if(!files.count(name))throw std::invalid_argument("missing model "+name);
        auto m=dfre::decode_model(read(files.at(name)));for(auto& lod:m.lods)lod=dfre::localize_model_lod(lod);
        auto& stored=models.emplace(name,std::move(m)).first->second;
        const auto handle=std::uint32_t(0x2200000+handles.size()*0x1000);handles[name]=handle;r.models.push_back({handle,&stored});return handle;
    };
    std::set<std::size_t> required;
    for(const auto& w:dfre::weapon_definitions())for(auto id:{w.primary_item,w.secondary_item})required.insert(dfre::runtime_item_index(r.definitions,id));
    for(auto index:required){auto& d=r.definitions[index];const std::string name(d.graphic);if(!name.empty() && name!="null")d.model=load(name);}
    const auto avatar=load(std::filesystem::path(argv[3]).filename().string());
    auto character=dfre::prepare_character_runtime(dfre::decode_character(read(argv[4])));
    r.characters.push_back({dfre::kCharacterInstanceBase,&character});
    dfre::MissionAnimations animations({{dfre::kCharacterInstanceBase,&character,{}}});
    dfre::MissionMuzzle muzzle(r,animations);
    std::string line;
    while(std::getline(std::cin,line)){
        std::istringstream in(line);std::string mode;in>>mode;
        if(mode=="bindings"){
            std::cout<<"B";for(std::uint32_t i=0;i<dfre::kWeaponCount;++i){const auto b=muzzle.weapon(i);std::cout<<' '<<b.primary_definition<<' '<<b.secondary_definition;}std::cout<<'\n';continue;
        }
        if(mode=="models"){
            for(const auto& entry:handles)std::cout<<"M "<<entry.second<<' '<<entry.first<<'\n';continue;
        }
        if(mode=="attachment"){std::cout<<"T";for(auto v:muzzle.attachment().m)std::cout<<' '<<v;std::cout<<'\n';continue;}
        dfre::AiActorImage actor{};std::uint32_t weapon,flags;in>>weapon>>flags;put(actor,0x94,weapon);put(actor,0x6c,flags);put(actor,0x10,avatar);put(actor,0x18,dfre::kCharacterInstanceBase);
        for(std::size_t at=0x24;at<0x3c;at+=4){std::int64_t v;if(!(in>>v))throw std::invalid_argument("actor fields missing");put(actor,at,std::uint32_t(v));}
        if(mode=="normal"){
            dfre::MissionMuzzleView view;in>>view.view_delta.x>>view.view_delta.y>>view.view_delta.z>>view.eye_height;
            std::cout<<"N";print(muzzle.normal(actor,weapon,view));std::cout<<'\n';
        }else if(mode=="animated"){
            std::uint32_t slot,frame,ticks;in>>slot>>frame>>ticks;
            auto& controller=animations.controller(dfre::kCharacterInstanceBase);controller.restart();controller.select_action(slot,frame);
            for(std::uint32_t t=0;t<ticks;++t)controller.tick({});
            unsigned shots=0;dfre::MuzzleResult result;
            const bool fired=muzzle.animated({&actor,77},[&](auto,const auto& shot){++shots;result=shot;});
            std::cout<<"A "<<fired<<' '<<shots;print(result);
            if(fired)for(auto v:muzzle.slot_seven().m)std::cout<<' '<<v;
            std::cout<<' '<<controller.pose().size();
            for(const auto& bone:controller.pose()){for(auto v:bone.angles)std::cout<<' '<<v;for(auto v:bone.scale)std::cout<<' '<<v;}
            std::cout<<'\n';
        }else throw std::invalid_argument("unknown muzzle command");
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
