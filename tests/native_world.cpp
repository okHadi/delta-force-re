#include "dfre/world.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {void check(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}}
int main(){try{
    dfre::TerrainGeometry terrain(dfre::AssetBytes(1024*1024,7),dfre::AssetBytes(1024*1024),std::vector<std::uint32_t>(65536));
    dfre::MissionData mission;dfre::MissionObject object;object.type_id=6001;object.object_id=77;object.position_raw={-1,65536,11};object.angle_degrees={240,-90};
    for(auto& group:mission.object_groups)group={object};
    auto hidden=object;hidden.flags=0x80;mission.object_groups[1].insert(mission.object_groups[1].begin(),hidden);
    auto placed=dfre::place_single_player_objects(mission,terrain);
    check(placed.size()==4 && placed[1].group==1 && placed[1].index==1,"World source order or disabled object filter changed");
    for(std::size_t i=0;i<4;++i){check(placed[i].pose.position[2]==(i==3?180235:114699),"Group terrain height offset changed");check(placed[i].pose.angles==std::array<std::uint32_t,3>{0x95560000,0xc0000000,0},"World angle conversion changed");}
    mission.object_groups[3][0].position_raw[2]=std::numeric_limits<std::int32_t>::max();placed=dfre::place_single_player_objects(mission,terrain);
    check(std::uint32_t(placed.back().pose.position[2])==0x8002bfffu,"World height addition did not wrap");
    std::cout<<"world placement checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
