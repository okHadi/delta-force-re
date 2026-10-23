#include "dfre/mission_object_update.hpp"
#include "dfre/mission_object_cactus.hpp"
#include "dfre/mission_object_sound.hpp"
#include "dfre/mission_object_wood_small.hpp"
#include "dfre/mission_object_wood_large.hpp"
#include "dfre/mission_object_generic.hpp"
namespace dfre {
void mission_object_update(MissionTickState& state,const AiActorRef& ref,std::uint32_t callback,
    std::uint32_t message,const MissionObjectLiveBindings& bindings){
 switch(callback){
 case 0x429bc0:mission_object_veh0(state,ref,bindings,message);return;
 case 0x4519a0:mission_object_slgt_ai(state,ref,bindings,message);return;
 case 0x4533f0:mission_object_generic(state,ref,bindings,message);return;
 case 0x453930:mission_object_wood_large(state,ref,bindings,message);return;
 case 0x453810:mission_object_wood_small(state,ref,bindings,message);return;
 case 0x4534a0:mission_object_cactus(state,ref,bindings,message);return;
 case 0x45c340:case 0x45c3a0:case 0x45c400:mission_object_sound(state,ref,callback,bindings,message);return;
 default:throw MissionTickError("Actor update body is not owned: "+std::to_string(callback));
 }
}
}
