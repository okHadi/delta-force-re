#include "dfre/resources.hpp"
#include "dfre/scripts.hpp"
namespace dfre {
namespace {
std::string lower(std::string_view value) {
    std::string out(value);
    for(auto& c:out)if(c>='A'&&c<='Z')c=char(c+32);
    return out;
}
std::string filename(std::string_view value,std::string_view suffix) {
    if(value.empty())return {};
    auto out=lower(value);
    if(out.size()<suffix.size() || out.compare(out.size()-suffix.size(),suffix.size(),suffix)!=0)out+=suffix;
    return out;
}
template<class T> const T& required(const std::map<std::string,T>& cache,std::string_view name) {
    const auto found=cache.find(lower(name));
    if(found==cache.end())throw AssetError("Mission asset is not loaded: "+std::string(name));
    return found->second;
}
}
MissionAssetCache::MissionAssetCache(const AssetStore& assets):assets_(assets),items_(load_item_catalog(assets)){}
const ObjectAppearance& MissionAssetCache::appearance(std::uint32_t item_id) {
    const auto& item=items_.find(item_id);
    if(const auto found=appearances_.find(item.id);found!=appearances_.end())return found->second;
    ObjectAppearance result{item.id,filename(item.value("graphic"),".3di"),filename(item.value("husk"),".3di"),
        filename(item.value("chr_file"),".chr"),filename(item.value("anim_def"),".anm")};
    for(const auto* name:{&result.model,&result.husk})if(!name->empty()&&models_.find(*name)==models_.end()) {
        auto decoded=decode_model(assets_.read(*name).bytes);
        if(decoded.version==5)for(auto& lod:decoded.lods)lod=localize_model_lod(lod);
        models_.emplace(*name,std::move(decoded));
    }
    if(!result.character.empty()&&characters_.find(result.character)==characters_.end())
        characters_.emplace(result.character,prepare_character_runtime(decode_character(assets_.read(result.character).bytes)));
    if(!result.animations.empty()&&animations_.find(result.animations)==animations_.end())
        animations_.emplace(result.animations,decode_game_script(assets_.read(result.animations).bytes));
    return appearances_.emplace(item.id,std::move(result)).first->second;
}
MissionScene MissionAssetCache::load(std::string_view name) {
    MissionScene scene{load_mission(assets_,name),{}};
    for(std::size_t group=0;group<scene.objects.size();++group) {
        auto& bindings=scene.objects[group];bindings.reserve(scene.mission.object_groups[group].size());
        for(const auto& object:scene.mission.object_groups[group])bindings.push_back(appearance(object.type_id));
    }
    return scene;
}
MissionRuntimeResources MissionAssetCache::load_runtime(std::string_view name,const ResourcePlanOptions& options) {
    if(!appearances_.empty() || !models_.empty() || !characters_.empty() || !animations_.empty())
        throw AssetError("Runtime scene requires an empty asset cache");
    MissionRuntimeResources result;
    result.mission=load_mission(assets_,name);
    ResourcePlanInput input;input.items=&items_;input.mission=&result.mission;input.options=options;
    result.plan=build_resource_plan(input);
    for(const auto& step:result.plan.steps)if(step.first_load) {
        if(step.kind==ResourceKind::model || step.kind==ResourceKind::husk) {
            const auto key=filename(step.name,".3di");
            bool present=false;
            for(const auto& archive:assets_.archives())for(const auto& entry:archive.entries())
                if(!entry.deleted && lower(entry.name)==key)present=true;
            if(!present){result.missing_models.push_back(key);continue;}
            auto decoded=decode_model(assets_.read(key).bytes);
            if(decoded.version!=5)throw AssetError("Runtime model requires 3DI version 5: "+key);
            for(auto& lod:decoded.lods)lod=localize_model_lod(lod);
            if(models_.emplace(key,std::move(decoded)).second)result.models.push_back({key,step.bank});
        } else if(step.kind==ResourceKind::character) {
            const auto key=filename(step.name,".chr");
            characters_.emplace(key,prepare_character_runtime(decode_character(assets_.read(key).bytes)));
        } else {
            const auto key=filename(step.name,".anm");
            animations_.emplace(key,decode_game_script(assets_.read(key).bytes));
        }
    }
    return result;
}
const Model& MissionAssetCache::model(std::string_view name) const{return required(models_,name);}
const CharacterModel& MissionAssetCache::character(std::string_view name) const{return required(characters_,name);}
std::string_view MissionAssetCache::animations(std::string_view name) const{return required(animations_,name);}
}
