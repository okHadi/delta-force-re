#include "dfre/scene_models.hpp"

#include <iostream>
#include <stdexcept>

// Focused current large-pass/deferred/terrain buffer ownership check.
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

constexpr std::uint32_t kTextureLevel = 0;

dfre::ModelGammaTable test_gamma() {
    dfre::ModelGammaTable gamma{};
    for (std::size_t i = 0; i < gamma.size(); ++i) gamma[i] = std::uint8_t(i);
    return gamma;
}

dfre::ModelPaletteBanks32 test_banks() {
    dfre::ModelPaletteBanks32 banks;
    for (auto& bank : banks) {
        bank.gamma_rgb.assign(768, 64);
        bank.display_rgb.assign(768, 80);
        bank.matching_rgb.assign(768, 64);
        bank.shades.assign(64 * 256, 0xff808080u);
    }
    return banks;
}

// One LOD with `meshes` meshes of one triangle each, a flat material, and
// camera-facing geometry at 256 file units (1.0 camera depth).
dfre::Model make_flat_model(std::size_t meshes, std::uint32_t render_type,
    std::int32_t bounding_radius) {
    dfre::Model model;
    model.name = "fixture";
    model.version = 5;
    model.lod_thresholds = {0, 0, 0, 0};
    model.render_types = {render_type, render_type, render_type, render_type};
    model.bounding_radius = bounding_radius;
    dfre::ModelLod lod;
    lod.raw_header.assign(0xb8, 0);
    for (std::size_t mesh = 0; mesh < meshes; ++mesh) {
        dfre::ModelMesh record;
        record.vertex_start = static_cast<std::uint32_t>(mesh * 3);
        record.vertex_count = 3;
        record.face_start = static_cast<std::uint32_t>(mesh);
        record.face_count = 1;
        record.normal_start = 0;
        record.normal_count = 1;
        lod.meshes.push_back(record);
    }
    for (std::size_t mesh = 0; mesh < meshes; ++mesh) {
        const std::int16_t base = static_cast<std::int16_t>(mesh * 200);
        lod.vertices.push_back(dfre::ModelVertex{{256, base, 0}, 0});
        lod.vertices.push_back(dfre::ModelVertex{{256, static_cast<std::int16_t>(base + 200), 0}, 0});
        lod.vertices.push_back(dfre::ModelVertex{{256, base, 200}, 0});
        dfre::ModelFace face;
        const auto start = std::uint32_t(mesh * 3);
        face.vertices = {start, start + 1, start + 2};
        face.normals = {0, 0, 0};
        face.u = {0, 0, 0};
        face.v = {0, 0, 0};
        face.material = 0;
        face.flags = 0;
        face.face_normal = 0;
        lod.faces.push_back(face);
    }
    lod.normals.push_back(dfre::ModelNormal{{0, 0, 0}, 0});
    dfre::ModelMaterial material;
    material.name = "flat";
    material.flags = 2;
    material.raw[0x10] = 2;
    lod.materials.push_back(material);
    lod.hardpoints = {{{0, 0, 0}}, {{0x100, 0, 0}}, {{0x200, 0, 0}}};
    model.lods.push_back(std::move(lod));
    return model;
}

dfre::ActorImage make_image(std::uint32_t model, std::uint32_t instance, std::uint32_t definition,
    std::int32_t x, std::int32_t y) {
    dfre::ActorImage image{};
    const auto put = [&](std::size_t offset, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i) image[offset + i] = std::uint8_t(value >> (i * 8));
    };
    put(0x00, definition);
    put(0x10, model);
    put(0x18, instance);
    put(0x24, static_cast<std::uint32_t>(x));
    put(0x28, static_cast<std::uint32_t>(y));
    return image;
}

dfre::MissionRuntimeState make_state() {
    dfre::MissionRuntimeState state;
    state.definitions.resize(2);
    state.definitions[0].type = 6; // object
    state.definitions[1].type = 3; // person
    state.definitions[1].character = 0x72000000u;
    state.models.push_back({"fixture.3di", 0, 0x71000000u});
    state.character_instances.assign(1, 1);
    return state;
}


} // namespace

int main(){try{
 using namespace dfre;
 AssetBytes height(1024*1024,0),character(height.size(),0);std::vector<std::uint32_t> detail(65536,0);
 TerrainRuntime runtime{TerrainGeometry(height,character,detail),{},{},{},{},{},{},{},0,32};
 runtime.color.palette.assign(768,0);runtime.detail_palette.assign(768,0);runtime.sky_palette.palette.assign(768,0);runtime.color.pixels.assign(1024*1024,0);
 const auto gamma=test_gamma();const auto banks=test_banks();const auto terrain=prepare_terrain_scene32(runtime,AssetBytes(gamma.begin(),gamma.end()));
 VoxelCamera camera;camera.width=camera.row_stride=64;camera.height=64;camera.right=camera.bottom=63;camera.focal_distance=32;camera.position[2]=12*65536;
 const RasterClip clip{0,0,63,63};const auto view=make_scene_model_view(camera.position,{0,0,0},clip,90);const auto lighting=make_scene_model_lighting(view);
 auto state=make_state();state.definitions[1].type=6;state.character_instances.clear();std::size_t visible_cases=0,queued_cases=0;
 for(const std::uint32_t material_flags:{2u,0x402u,0xau,0x82u,0x8002u}){
  auto model=make_flat_model(1,0x676e7263u,0x100000);model.lods[0].materials[0].flags=material_flags;
  for(unsigned i=0;i<4;++i)model.lods[0].materials[0].raw[0x10+i]=std::uint8_t(material_flags>>(i*8));
  SceneModelAssets assets;assets.models.push_back({0x71000001u,0,&model});SceneModelCatalog catalog(assets,banks,gamma,kTextureLevel);SceneModels32 renderer(catalog,state);
  auto image=make_image(0x71000001u,0,1,0x20000,0);image[4]=1;for(unsigned i=0;i<4;++i)image[0x2c+i]=std::uint8_t(std::uint32_t(camera.position[2])>>(i*8));
  SceneModelCollectInputs collect;collect.view=view;collect.runtime_records_supplied=collect.effects_supplied=true;MissionActorImages current;current[0].push_back(image);collect.records=scene_model_records(state,current);check(collect.records.size()==1,"Current active actor was not supplied");renderer.begin_frame();
  const auto queues=renderer.collect(collect);check(queues.large.size()==1&&queues.near.empty(),"Controlled model is not in actual large list");SceneModelPassInputs pass;pass.view=view;const auto plan=renderer.plan_large_pass(pass);check(plan.calls.size()==1,"Controlled large plan is empty");
  ModelDeferredBridge bridge;bridge.reset(clip,64,64);bridge.clear_queues();bridge.set_effect_mode(true);
  TerrainFrame32 frame;frame.pixels.assign(64*64,0x12345678);frame.depths.assign(64*64,0xabcd);const auto initial_colour=frame.pixels;const auto initial_depth=frame.depths;
  SceneModelDrawInputs draw;draw.width=draw.height=64;draw.colour=&frame.pixels;draw.depth=&frame.depths;draw.lighting=lighting;draw.deferred=&bridge;
  const auto stats=renderer.draw_plan(plan,draw);check(stats.items==1&&stats.pixel_writes==0,"Large mode used direct raster output");check(frame.pixels==initial_colour&&frame.depths==initial_depth,"Large queue changed pre-terrain colour/depth");
  std::size_t count=0;for(const auto route:{EffectRoute::deferred,EffectRoute::deferred_effect,EffectRoute::effect_plain,EffectRoute::deferred_no_context})count+=bridge.queue(route).size();check(count>0,"Large pass queued no visible face");++queued_cases;
  frame=render_terrain_frame32(terrain,camera,{false,0});const auto background=frame.pixels;const auto terrain_depth=frame.depths;
  check(frame.pixels!=initial_colour&&frame.depths!=initial_depth,"Actual terrain replacement did not occur");
  std::size_t retained=0;for(const auto route:{EffectRoute::deferred,EffectRoute::deferred_effect,EffectRoute::effect_plain,EffectRoute::deferred_no_context})retained+=bridge.queue(route).size();check(retained==count,"Terrain replacement lost queued faces");
  const auto mixed=bridge.draw_mixed(clip,64,64,frame.pixels,frame.depths);bridge.set_effect_mode(false);const auto no_context=bridge.draw_no_context(clip,64,64,frame.pixels,frame.depths);const auto effect=bridge.draw_deferred_effect(clip,64,64,frame.pixels,frame.depths);
  if(material_flags==2||material_flags==0x402){check(mixed+no_context+effect>0&&frame.pixels!=background,"Large model is not visible after terrain replacement");++visible_cases;}
  check(draw.colour==&frame.pixels&&draw.depth==&frame.depths,"Terrain move changed vector-object ownership");
  // Direct opaque path is an explicit control, with source effect mode disabled.
  if(material_flags==2){bridge.clear_queues();frame.pixels=background;frame.depths=terrain_depth;const auto direct=renderer.draw_plan(plan,draw);check(direct.pixel_writes>0&&frame.pixels!=background,"Direct control wrote no pixels");check(frame.depths!=terrain_depth,"Direct control wrote no depth");}
 }
 for(std::uint32_t material=0;material<0x10000;++material)for(const auto face:{0u,0x100u,0x200u,0x400u,0x700u})check(route_effect_face({face,material,true,true})!=EffectRoute::draw,"Enabled large mode has a direct route");
 std::cout<<queued_cases<<" large material routes retain faces; "<<visible_cases<<" visible post-terrain controls; direct control passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
