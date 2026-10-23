#include "dfre/scene_models.hpp"
#include <cassert>
using namespace dfre;
namespace {using U=std::uint32_t;void put(ActorImage& image,std::size_t at,U value){for(U i=0;i<4;++i)image[at+i]=std::uint8_t(value>>(8*i));}template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::invalid_argument&){failed=true;}assert(failed);}}
int main(){
 Model model;model.version=5;model.bounding_radius=0x10000;model.lod_thresholds={0,0,0,0};model.render_types={0x676e7263,0x676e7263,0x676e7263,0x676e7263};ModelLod lod;lod.raw_header.resize(0xb8);lod.meshes.push_back({});model.lods.push_back(lod);
 ModelPaletteBanks32 banks;for(auto& bank:banks){bank.gamma_rgb.assign(768,64);bank.display_rgb.assign(768,64);bank.matching_rgb.assign(768,64);bank.shades.assign(64*256,0xff808080);}ModelGammaTable gamma{};for(std::size_t i=0;i<gamma.size();++i)gamma[i]=std::uint8_t(i);
 SceneModelAssets assets;assets.models={{0x71000000,0,&model},{0x71000001,0,&model}};SceneModelCatalog catalog(assets,banks,gamma);MissionRuntimeState world;SceneModels32 renderer(catalog,world);
 const auto view=make_scene_model_view({0,0,0},{0,0,0},{0,0,63,63},90);SceneModelCollectInputs input;input.view=view;input.runtime_records_supplied=true;input.effects_supplied=true;
 auto contains_gap=[](const SceneModelCollectResult& value){for(const auto& gap:value.gaps)if(gap.kind==SceneModelGapKind::effect_table_not_supplied)return true;return false;};assert(!contains_gap(renderer.collect(input)));
 SceneEffectRecord effect;effect.identity=scene_model_identity(0x9f3098);effect.position={0x70000000,0,0};effect.image=ActorImage{};auto& image=*effect.image;put(image,0,1);put(image,0x10,0x71000000);put(image,0x14,0x71000001);put(image,0x6c,10);put(image,0xfc,1);put(image,0x30,0x12340000);put(image,0x34,0x23450000);put(image,0x38,0x34560000);input.effects={effect};
 auto collected=renderer.collect(input);assert(collected.near.size()==1&&collected.large.empty()&&collected.effect_records==1);SceneModelPassInputs pass;pass.view=view;auto plan=renderer.plan_after_terrain(pass);assert(plan.calls.size()==1&&plan.calls[0].identity==effect.identity&&plan.calls[0].model_handle==0x71000001&&plan.calls[0].matrices.size()==1);
 ModelInstanceFields fields;fields.angles={0x12340000,0x23450000,0x34560000};auto expected=model_multiply_matrix(view.view.world_to_camera,model_instance_local_matrix(fields));assert(plan.calls[0].matrices[0]==expected);
 put(*input.effects[0].image,0x30,0x56780000);renderer.collect(input);auto moved=renderer.plan_after_terrain(pass);assert(moved.calls.size()==1&&moved.calls[0].matrices[0]!=plan.calls[0].matrices[0]);
 put(*input.effects[0].image,0,0);assert(renderer.collect(input).near.empty());put(*input.effects[0].image,0,1);put(*input.effects[0].image,0xfc,0xffffffff);assert(renderer.collect(input).near.empty());put(*input.effects[0].image,0xfc,1);input.effects[0].identity=nullptr;rejects([&]{renderer.collect(input);});
 input.effects[0].identity=effect.identity;input.effects[0].image.reset();input.effects[0].position={0,0,0};input.effects[0].model=0x71000000;input.effects[0].counter_fc=1;assert(renderer.collect(input).near.size()==1);rejects([&]{renderer.plan_after_terrain(pass);});
}
