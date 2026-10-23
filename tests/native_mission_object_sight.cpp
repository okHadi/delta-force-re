#include "dfre/mission_object_sight.hpp"
#include <cassert>
using namespace dfre;
int main(){MissionTickState s;s.tables.groups[3].resize(2);MissionObjectSightBindings b;assert(mission_object_sight(s,{3,0},{3,1},b)==1);s.tables.groups[3][1].position[0]=0x100000;bool failed=false;try{mission_object_sight(s,{3,0},{3,1},b);}catch(const MissionTickError&){failed=true;}assert(failed);b.terrain=[](const auto&,const auto&,int){return 1;};failed=false;try{mission_object_sight(s,{3,0},{3,1},b);}catch(const MissionTickError&){failed=true;}assert(failed);
 AssetBytes height(1024*1024),character(1024*1024),bounds(1024*1024);TerrainGeometry terrain(height,character,std::vector<unsigned>(65536));s.world.terrain=&terrain;for(auto& a:s.tables.groups[3])a.position[2]=0xa0000;
 Model model;model.render_types[0]=0x123;model.lods.resize(1);model.lods[0].meshes.resize(1);model.lods[0].meshes[0].raw.resize(112);MissionGeometryModel binding;binding.handle=1;binding.model=&model;binding.bounds.height_60=0x140000;binding.meshes.resize(1);MissionGeometry geometry({binding});s.tables.groups[1].resize(2);for(auto& a:s.tables.groups[1]){a.active=true;a.definition_index=1;a.field_10=1;a.position[0]=0x80000;a.position[2]=0xa0000;}
 unsigned callbacks=0;auto matrices=[&](const auto&,const auto&,const auto&){++callbacks;s.tables.groups[1][1].active=false;s.tables.groups[0].resize(1);s.tables.groups[0][0].active=false;s.world.water_level=0x100000;MissionGeometryMatrices m;m.matrices.resize(1);m.matrices[0][0]=m.matrices[0][5]=m.matrices[0][10]=0x400000;m.flags.resize(1);return m;};
 auto live=mission_object_sight_bindings(s,geometry,bounds,matrices);assert(mission_object_sight(s,{3,0},{3,1},live)==2);assert(callbacks==1);
 // Source skip gates precede model lookup, including a missing model handle.
 s.tables.groups[0][0].active=true;s.tables.groups[0][0].field_10=999;s.tables.groups[0][0].person_slot=std::nullopt;s.tables.groups[0][0].position[2]=0xa0000;s.tables.groups[1].clear();assert(mission_object_sight(s,{0,0},{3,1},live)==2);
}
