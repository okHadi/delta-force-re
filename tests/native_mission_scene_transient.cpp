#include "dfre/mission_scene_transient.hpp"
#include "dfre/mission_tick.hpp"
#include <cassert>
#include <memory>
using namespace dfre;
namespace {using U=std::uint32_t;void put(AiActorImage& image,std::size_t at,U value){for(std::size_t i=0;i<4;++i)image[at+i]=std::uint8_t(value>>(8*i));}template<class F>void rejects(F f){bool failed=false;try{f();}catch(const MissionTickError&){failed=true;}assert(failed);}}
int main(){
 MissionRuntimeState world;world.definitions.resize(3);world.definitions[1].type=6;world.definitions[2].type=3;
 auto images=std::make_unique<std::array<AiActorImage,1024>>();
 put((*images)[1],0,1);put((*images)[1],4,2);put((*images)[1],0x10,0x71000000);put((*images)[1],0x24,0x12345678);put((*images)[1],0x30,0xfedcba98);put((*images)[1],0x80,17);(*images)[1][0x113]=0x5a;
 put((*images)[1023],0,2);put((*images)[1023],4,1);put((*images)[1023],0x10,0x71000001);
 std::size_t reads=0;MissionSceneTransientSource source=[&](std::size_t slot){assert(slot==reads++);return (*images)[slot];};
 auto records=mission_scene_transient_records(world,source);assert(reads==1024&&records.size()==2);assert(records[0].slot==1&&records[0].group==kAiExtraTableGroup&&records[0].character);assert(records[1].slot==1023&&!records[1].character);assert(records[0].image==(*images)[1]&&records[1].image==(*images)[1023]);assert(records[0].identity==scene_model_identity(actor_handle(4,1))&&records[1].identity!=records[0].identity);for(std::size_t group=0;group<4;++group)assert(records[0].identity!=scene_model_identity(actor_handle(group,1)));
 put((*images)[1],0,0);put((*images)[1023],0x24,0x87654321);reads=0;records=mission_scene_transient_records(world,source);assert(records.size()==1&&records[0].slot==1023&&records[0].image==(*images)[1023]);
 reads=0;source=[&](std::size_t slot){assert(slot==reads++);if(slot==0){put((*images)[17],0,1);put((*images)[17],4,1);}if(slot==17)world.definitions[1].type=3;return (*images)[slot];};records=mission_scene_transient_records(world,source);assert(reads==1024&&records.size()==2&&records[0].slot==17&&records[0].character);
 rejects([&]{mission_scene_transient_records(world,MissionSceneTransientSource{});});source=[&](std::size_t slot){assert(slot==reads++);return (*images)[slot];};put((*images)[17],4,999);reads=0;rejects([&]{mission_scene_transient_records(world,source);});assert(reads==18);
 // Canonical group4 snapshots are supplied by MissionTickState, with no render owner copy.
 auto tick=std::make_unique<MissionTickState>();AiActorTables tables;tables.extra.resize(1024);tables.extra[1023].active=true;tables.extra[1023].definition_index=1;tables.extra[1023].field_10=0x71000000;tables.extra[1023].position[0]=0x22220000;tables.extra[1023].angle_yaw=0x12340000;init_mission_tick(*tick,{},std::move(tables),{}, {},0);auto& actor=tick->tables.extra[1023];
 records=mission_scene_transient_records(world,*tick);assert(records.size()==1&&records[0].slot==1023);actor.position[0]=0x33330000;actor.angle_yaw=0x56780000;auto current=mission_scene_transient_records(world,*tick);assert(current[0].image!=records[0].image&&current[0].image==snapshot_mission_actor(*tick,{4,1023}));actor.active=false;actor.definition_index=0;assert(mission_scene_transient_records(world,*tick).empty());
 SceneModelAssets assets;ModelPaletteBanks32 banks{};ModelGammaTable gamma{};SceneModelCatalog catalog(assets,banks,gamma);SceneModels32 renderer(catalog,world);SceneModelCollectInputs collect;
 auto has_gap=[](const SceneModelCollectResult& value){for(const auto& gap:value.gaps)if(gap.kind==SceneModelGapKind::runtime_pool_not_supplied)return true;return false;};
 assert(has_gap(renderer.collect(collect)));collect.runtime_records_supplied=true;auto empty=renderer.collect(collect);assert(!has_gap(empty)&&empty.runtime_records==0);
}
