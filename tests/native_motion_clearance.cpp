#include "dfre/motion_clearance.hpp"
#include <iostream>
#include <stdexcept>
namespace {void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}}
// Expected values were read from the original Df.exe replay; see
// tools/check_motion_clearance_original.py and .local/opencode/motion-clearance-work/original-values.txt.
int main(){try{
 dfre::GroundEntity entity;
 require(dfre::ground_clearance({},0)==dfre::clearance_none,"Empty pose list keeps the empty clearance");
 require(dfre::ground_clearance({{100,200,300},{400,500,600}},0)==364,"Original clearance takes the lowest pose word");
 require(dfre::ground_clearance({{100,200,300},{400,500,600}},0x30000)==dfre::clearance_none,"Frame mask bits 16 and 17 skip both bones");
 require(dfre::ground_clearance({{-5,-0x100000,7}},0)==-1911552,"Negative pose word keeps its signed product");
 {
  std::vector<dfre::PosePosition> poses;
  for(std::int32_t i=0;i<33;++i)poses.push_back({i,i*200,i});
  require(dfre::ground_clearance(poses,0)==0,"Bone 33 wraps the shift count to bit 16");
  require(dfre::ground_clearance(poses,0xfffe0000)==0,"Bones up to 15 keep bit 16 clear");
 }
 dfre::GroundDefinition definition;definition.radius=0x100000;
 dfre::GroundPiece piece;piece.bounds={-0x10000,0x10000,-0x10000,0x10000};
 dfre::GroundVolume volume;volume.bounds={-0x10000,0x10000,-0x10000,0x10000};volume.accepted=true;volume.height=1000;
 piece.volumes={volume};definition.pieces={piece};
 entity.position={0,0};entity.definition=&definition;
 {
  const std::vector<dfre::GroundEntity> entities{entity};
  const auto result=dfre::ground_entity_height(0,{0,0,0},0x7fffffff,entities);
  require(result.height==1000&&result.entity==&entities[0],"Original entity accept");
 }
 {
  auto skipped=definition;skipped.pieces[0].flags=1;
  dfre::GroundEntity hidden;hidden.position={0,0};hidden.definition=&skipped;
  const std::vector<dfre::GroundEntity> hidden_list{hidden};
  const auto result=dfre::ground_entity_height(0,{0,0,0},0x7fffffff,hidden_list);
  require(result.height==0&&result.entity==nullptr,"Piece flags bits 0 and 1 skip the piece");
 }
 {
  dfre::GroundEntity far;far.position={0,0};far.definition=&definition;
  const std::vector<dfre::GroundEntity> far_list{far};
  const auto result=dfre::ground_entity_height(0,{0x400000,0,0},0x7fffffff,far_list);
  require(result.height==0&&result.entity==nullptr,"Radius gate rejects the entity");
 }
 {
  const std::vector<dfre::GroundEntity> single{entity};
  const auto result=dfre::ground_entity_height(0,{0,0,0},10,single);
  require(result.height==0&&result.entity==nullptr,"Depth gate rejects surfaces above position+depth");
 }
 {
  auto lowered=definition;lowered.pieces[0].volumes[0].height=500;
  dfre::GroundEntity low;low.position={0,0};low.definition=&lowered;
  const std::vector<dfre::GroundEntity> single{low};
  const auto result=dfre::ground_entity_height(1000,{0,0,0},0x7fffffff,single);
  require(result.height==1000&&result.entity==nullptr,"Lower surface keeps the terrain height");
 }
 {
  auto rejected=definition;rejected.pieces[0].volumes[0].accepted=false;
  dfre::GroundEntity first;first.position={0,0};first.definition=&rejected;
  dfre::GroundEntity second;second.position={0,0};second.definition=&definition;second.flags=0x1008;
  definition.pieces[0].volumes[0].height=2000;
  const std::vector<dfre::GroundEntity> both{first,second};
  const auto result=dfre::ground_entity_height(0,{0,0,0},0x7fffffff,both);
  require(result.height==2000&&result.entity==&both[1],"Second entity wins after the first is rejected");
 }
 {
  dfre::GroundDefinition alternate;alternate.radius=0x100000;alternate.pieces=definition.pieces;
  dfre::GroundEntity chosen;chosen.position={0,0};chosen.flags=8;chosen.definition=&definition;chosen.alternate=&alternate;
  const std::vector<dfre::GroundEntity> chosen_list{chosen};
  const auto result=dfre::ground_entity_height(0,{0,0,0},0x7fffffff,chosen_list);
  require(result.height==2000&&result.entity==&chosen_list[0],"Flag bit 8 uses the alternate definition");
  alternate.pieces.clear();
  const auto empty=dfre::ground_entity_height(0,{0,0,0},0x7fffffff,chosen_list);
  require(empty.height==0&&empty.entity==nullptr,"Alternate definition data decides the walk");
 }
 std::cout<<"motion clearance checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
