#include "dfre/mission_animation_effect.hpp"
#include "dfre/mission_live.hpp"
#include <cassert>
#include <cstring>
int main(){using namespace dfre;using U=std::uint32_t;MissionTickState state;AiActorTables tables;tables.groups[3].resize(1);tables.groups[3][0].active=true;tables.groups[3][0].definition_index=1;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0);std::array<U,3> args{0xffffffff,0,0x7cca80};U type=2,water_reads=0;unsigned sounds=0,visuals=0;MissionAnimationEffectBindings b;
 b.read=[&](U at,U n)->std::uint64_t{if(at==0xbd5248){assert(n==4);++water_reads;return U(state.world.water_level);}if(at>=0x7cca80&&at<0x7ccb94){assert(n==4);const auto image=snapshot_mission_actor(state,{3,0});U value;std::memcpy(&value,image.data()+at-0x7cca80,4);return value;}if(at==0x4ccb2c){assert(n==8);return 0xc0e999999999999aull;}if(at==0xbd52ac)return 0x1220000u;if(at==0x1220000&&n==1)return 7u;if(at==0xbd533b&&n==1)return type;throw MissionTickError("Animation effect source has no current owner");};
 b.call=[&](U site,const auto& values){assert((site==0x43e7d0&&values==std::array<U,4>({0x8c,0,0,0})));++sounds;};mission_animation_effect(b,args);assert(sounds==1&&water_reads==1&&visuals==0);
 state.world.water_level=1;water_reads=0;b.call=[&](U site,const auto& values){if(site==0x45dc70){assert((values==std::array<U,4>({2,0,0,1})));++visuals;state.world.water_level=-4096;args[1]=1;state.tables.groups[3][0].position[0]=0x12345678;return;}assert((site==0x43e7d0&&values==std::array<U,4>({0x88,0,0,0xfffff000})));++sounds;};mission_animation_effect(b,args);assert(sounds==2&&visuals==1&&water_reads==3);
 state.world.water_level=1;b.call=[&](U site,const auto&){assert(site==0x45dc70);throw MissionTickError("Wet visual source body is required");};bool failed=false;try{mission_animation_effect(b,args);}catch(const MissionTickError&){failed=true;}assert(failed&&sounds==2);
 b.call={};state.world.water_level=0;state.tables.groups[3][0].position[0]=0;type=0;mission_animation_effect(b,args);type=1;failed=false;try{mission_animation_effect(b,args);}catch(const std::runtime_error&){failed=true;}assert(failed);
}
