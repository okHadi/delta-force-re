#include "dfre/object_live_callbacks.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;
namespace {void put(ObjectBehaviorActor& actor,std::size_t at,std::uint32_t value){std::memcpy(actor.image.data()+at,&value,4);}std::uint32_t word(const ObjectBehaviorActor& actor,std::size_t at){std::uint32_t n;std::memcpy(&n,actor.image.data()+at,4);return n;}}
int main(){
    ObjectBehaviorActor actor;ObjectBehaviorState state;ObjectBehaviorWorld world;
    object_behavior::veh0_ai(actor,state,world,0);assert(word(actor,0xfc)==0x10);
    put(actor,0x7c,1);bool failed=false;try{object_behavior::veh0_ai(actor,state,world,0);}catch(const ObjectBehaviorError&){failed=true;}assert(failed);
    state.call=[](auto&,auto&,const auto& action){ObjectBehaviorInput result;result.site=action.site;return result;};
    object_behavior::veh0_ai(actor,state,world,0);assert(state.actions.size()==1&&state.actions[0].site==0x43ef80);
    state.call=[](auto&,auto&,const auto&){return ObjectBehaviorInput{};};failed=false;try{object_behavior::veh0_ai(actor,state,world,0);}catch(const ObjectBehaviorError&){failed=true;}assert(failed);
}
