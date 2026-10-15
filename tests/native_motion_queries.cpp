#include "dfre/motion_queries.hpp"
#include <iostream>
#include <stdexcept>
namespace {void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}}
// The map is the same stripe pattern the original replay checker uses:
// byte(x,y) = (x*3 + y*5) & 0xff. Expected values below were read from the
// original Df.exe replay, see tools/check_motion_queries_original.py.
int main(){try{
 dfre::AssetBytes map(1024u*1024u);
 for(std::size_t y=0;y<1024;++y)for(std::size_t x=0;x<1024;++x)map[y*1024+x]=std::uint8_t((x*3+y*5)&0xff);
 const dfre::TerrainGeometry terrain(std::move(map),dfre::AssetBytes(1024u*1024u),std::vector<std::uint32_t>(65536u));
 require(dfre::ground_height_query(terrain,0,0)==0,"Original 47DF40 at the map origin");
 require(dfre::ground_height_query(terrain,0x10000,0x10000)==131072,"Original 47DF40 at one cell");
 require(dfre::ground_height_query(terrain,0xffff,0xffff)==130944,"Original 47DF40 inside the first cell");
 require(dfre::ground_height_query(terrain,-1,-1)==0,"Original 47DF40 wraps the cell index at -1");
 require(dfre::ground_height_query(terrain,0x50000,0x30000)==491520,"Original 47DF40 away from the origin");
 const auto origin=dfre::ground_slope_query(terrain,0,0);
 require(origin==dfre::GroundSlope{-999424,-966656},"Original 47E5D0 at the map origin");
 const auto cell=dfre::ground_slope_query(terrain,0x10000,0x10000);
 require(cell==dfre::GroundSlope{49152,-966656},"Original 47E5D0 at one cell");
 require(dfre::ground_slope_query(terrain,-1,-1)==cell,"Original 47E5D0 wraps the cell index at -1");
 dfre::ActorGroundQuery plain;
 require(dfre::ground_height_for_actor(terrain,plain,0,0)==0,"Height source without a ground entity uses 47DF40");
 dfre::ActorGroundQuery entity;entity.has_ground_entity=true;entity.entity_height=12345;
 require(dfre::ground_height_for_actor(terrain,entity,0,0)==12345,"Height source with a ground entity uses the 486FD0 input");
 entity.entity_height=-1;
 require(dfre::ground_height_for_actor(terrain,entity,0x10000,0x10000)==-1,"Entity height passes through unchanged");
 require(dfre::empty_clearance==0xfffffff,"Empty clearance scan value");
 std::cout<<"motion query checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
