#include "dfre/mission_scene_transient.hpp"
#include "dfre/mission_tick.hpp"
namespace dfre {
namespace {std::uint32_t word(const AiActorImage& image,std::size_t at){return std::uint32_t(image[at])|(std::uint32_t(image[at+1])<<8)|(std::uint32_t(image[at+2])<<16)|(std::uint32_t(image[at+3])<<24);}}
std::vector<SceneModelRecord> mission_scene_transient_records(
 const MissionRuntimeState& world,const MissionSceneTransientSource& source){
 if(!source)throw MissionTickError("Transient scene records need current actor snapshots");
 std::vector<SceneModelRecord> records;
 // 4852EB..4852F5 passes a fixed 400hex physical slots to 485040.
 for(std::size_t slot=0;slot<1024;++slot){
  const auto image=source(slot);if(word(image,0)==0)continue;
  const auto definition=word(image,4);
  if(definition>=world.definitions.size())throw MissionTickError("Transient scene definition exceeds the current table");
  records.push_back({scene_model_identity(actor_handle(kAiExtraTableGroup,slot)),image,
   kAiExtraTableGroup,slot,world.definitions[definition].type==3});
 }
 return records;
}
std::vector<SceneModelRecord> mission_scene_transient_records(
 const MissionRuntimeState& world,const MissionTickState& tick){
 return mission_scene_transient_records(world,[&](std::size_t slot){
  return snapshot_mission_actor(tick,{kAiExtraTableGroup,slot});
 });
}
}
