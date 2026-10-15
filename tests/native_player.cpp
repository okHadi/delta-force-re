#include "dfre/player.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}}
int main(){try{
 dfre::TerrainGeometry terrain(dfre::AssetBytes(1024*1024,7),dfre::AssetBytes(1024*1024),std::vector<std::uint32_t>(65536));dfre::MissionData mission;
 require(!dfre::initial_player(mission,terrain),"Missing start is explicit");
 dfre::MissionObject marker;marker.type_id=6001;marker.object_id=42;marker.position_raw={-1,65536,11};marker.angle_degrees={240,-90};
 auto excluded=marker;excluded.flags=0x10;mission.object_groups[2]={excluded,marker};const auto player=dfre::initial_player(mission,terrain);
 require(player&&player->marker_index==1&&player->marker_object_id==42,"First enabled SP start");require(player->actor.position==std::array<std::int32_t,3>{-1,65536,114699},"Base height added to raw position");require(player->camera.position[2]==160574,"Original class3 eye height");
 require(player->actor.angles==std::array<std::uint32_t,3>{0x95560000,0xc0000000,0},"Original signed angle conversion");
 for(unsigned flags=0;flags<256;++flags)require(dfre::single_player_object_enabled(flags)==!(flags&0x90),"SP flag filter");
 marker.position_raw[2]=std::numeric_limits<std::int32_t>::max();mission.object_groups[2]={marker};const auto wrapped=dfre::initial_player(mission,terrain);require(wrapped&&std::uint32_t(wrapped->actor.position[2])==0x8001bfffu&&std::uint32_t(wrapped->camera.position[2])==0x80027332u,"Position and eye-height wrap");
 std::cout<<"player checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
