#include "dfre/world_runtime.hpp"
#include "dfre/mission_tick.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void put(std::vector<std::uint8_t>& b,U at,U n){for(U i=0;i<4;++i)b.at(at+i)=std::uint8_t(n>>(8*i));}
}
int main(int argc,char** argv){try{
 TerrainGeometry terrain(AssetBytes(1024*1024),AssetBytes(1024*1024),std::vector<U>(65536));MissionInitConfig config;config.networked=false;config.local_player_active=false;
 ItemCatalog items("begin null\nid 100000\ntype object\nend\nbegin sheet\nid 100001\ntype effect\ngraphic sheet\nend\n");MissionRuntimeResources resources;resources.models.push_back({"sheet.3di",0});ResourceStep step;step.kind=ResourceKind::model;step.item_index=1;step.name="sheet";resources.plan.steps.push_back(step);
 Model loaded;loaded.lods.resize(1);loaded.lods[0].raw_header.resize(184);put(loaded.lods[0].raw_header,0x40,12u<<16);put(loaded.lods[0].raw_header,0x58,8u<<16);unsigned calls=0;
 auto resolver=[&](std::string_view name)->const Model&{check(name=="sheet.3di","wrong loaded model name");++calls;return loaded;};
 auto state=assemble_mission_runtime(resources,items,terrain,nullptr,config,{},resolver);check(calls==1&&state.definitions[1].effect_count==96&&state.definitions[1].model==kModelHandleBase,"effect metadata not assembled");
 put(loaded.lods[0].raw_header,0x40,0x8000ffff);put(loaded.lods[0].raw_header,0x58,0xffff1234);state=assemble_mission_runtime(resources,items,terrain,nullptr,config,{},resolver);check(state.definitions[1].effect_count==32768,"signed metadata product differs");
 loaded.lods.clear();state=assemble_mission_runtime(resources,items,terrain,nullptr,config,{},resolver);check(!state.definitions[1].effect_count,"null LOD produced frames");resources.models.clear();const auto before=calls;state=assemble_mission_runtime(resources,items,terrain,nullptr,config,{},resolver);check(calls==before&&!state.definitions[1].model&&!state.definitions[1].effect_count,"missing model reached metadata provider");
 resources.models.push_back({"sheet.3di",0});loaded.lods.resize(1);loaded.lods[0].raw_header.resize(16);bool failed=false;try{assemble_mission_runtime(resources,items,terrain,nullptr,config,{},resolver);}catch(const AssetError&){failed=true;}check(failed,"short loaded header accepted");
 if(argc==2){AssetStore store(argv[1]);MissionAssetCache cache(store);const auto actual=cache.load_runtime("C06M01.BMS");auto stock=assemble_mission_runtime(actual,cache.items(),terrain,nullptr,config,{},[&](std::string_view name)->const Model&{return cache.model(name);});MissionTickState live;init_mission_tick(live,{},stock,123);unsigned checked=0;
  for(const auto& d:stock.definitions)if(d.type==8&&d.model){const std::string name=d.graphic;U expected=0;if(name=="bexp")expected=96;if(name=="splash2")expected=80;if(name=="splash1")expected=40;if(name=="smoke_a")expected=480;if(expected){check(d.effect_count==expected,"stock effect frame count differs");check(live.definitions.entries().at(std::size_t(&d-stock.definitions.data())).field_9c==expected,"live definition lost effect count");++checked;std::cout<<name<<' '<<d.effect_count<<'\n';}}
  check(checked==4,"stock effect fixtures are not all loaded");
 }
 std::cout<<"effect metadata checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
