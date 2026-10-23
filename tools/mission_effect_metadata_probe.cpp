#include "dfre/world_runtime.hpp"
#include <iostream>
#include <sstream>
using namespace dfre;using U=std::uint32_t;
namespace {void put(std::vector<std::uint8_t>& b,U at,U n){for(U i=0;i<4;++i)b.at(at+i)=std::uint8_t(n>>(i*8));}}
int main(){try{
 TerrainGeometry terrain(AssetBytes(1024*1024),AssetBytes(1024*1024),std::vector<U>(65536));MissionInitConfig config;config.networked=false;config.local_player_active=false;
 U width,height,type,loaded,lod;
 while(std::cin>>width>>height>>type>>loaded>>lod){
  ItemCatalog items(std::string("begin null\nid 100000\ntype object\nend\nbegin effect\nid 100001\ntype ")+(type==8?"effect":"object")+"\ngraphic sheet\nend\n");
  MissionRuntimeResources resources;if(loaded)resources.models.push_back({"sheet.3di",0});ResourceStep step;step.kind=ResourceKind::model;step.item_index=1;step.item_id=1;step.name="sheet";resources.plan.steps.push_back(step);
  Model model;if(lod){model.lods.resize(1);model.lods.front().raw_header.resize(184);put(model.lods.front().raw_header,0x40,width);put(model.lods.front().raw_header,0x58,height);}
  unsigned lookups=0;auto state=assemble_mission_runtime(resources,items,terrain,nullptr,config,{},[&](std::string_view name)->const Model&{if(name!="sheet.3di")throw AssetError("Unexpected effect model");++lookups;return model;});
  std::cout<<state.definitions.at(1).effect_count<<' '<<lookups<<'\n';
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
