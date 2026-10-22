#include "dfre/player_step.hpp"
#include <cstring>

namespace dfre {
namespace {
std::int32_t signed_word(std::uint32_t word) noexcept {
    std::int32_t result;std::memcpy(&result,&word,4);return result;
}
std::uint32_t magnitude(std::uint32_t difference) noexcept {
    const auto sign=(difference&0x80000000u)?0xffffffffu:0u;
    return (difference^sign)-sign;
}
std::uint32_t half_signed(std::uint32_t value) noexcept {
    return (value>>1)|(value&0x80000000u);
}
PlayerStepActor actor(const PlayerStepWorld& world,std::uint32_t handle) {
    if(!world.actor)throw PlayerStepError("player step: actor source missing");
    const auto result=world.actor(handle);
    if(!result)throw PlayerStepError("player step: actor handle missing");
    return *result;
}
}
PlayerStepReport step_player(PlayerStepState& state,const PlayerStepWorld& world) {
    PlayerStepReport report;
    if(!state.local_actor_c23820)return report;
    if(!world.weapon_prefix)throw PlayerStepError("player step: weapon prefix missing (0x482B11)");
    report.ran=true;world.weapon_prefix(state);
    for(auto& word:state.recoil_c239e8){
        if(signed_word(word)>0){word-=0x80;if(signed_word(word)<0)word=0;}
        else if(word){word+=0x80;if(signed_word(word)>0)word=0;}
    }
    const auto local=actor(world,state.local_actor_c23820);
    const std::uint32_t target=(local.flags_6e&2)?0x4cccu:(state.prone_c23828?0x6666u:0xb333u);
    const auto product=std::int64_t(signed_word(state.eye_c23a18-target))*0xe666;
    const auto scaled=product>=0?product/65536:-((-product+65535)/65536);
    state.eye_c23a18=target+std::uint32_t(scaled);
    if(signed_word(state.waypoint_count_c23b7c)<=1 || state.game_type_67f53c)return report;
    if(state.waypoint_index_c23b80>=state.waypoints_c23b84.size())
        throw PlayerStepError("player step: waypoint index outside the table");
    const auto point=actor(world,state.waypoints_c23b84[state.waypoint_index_c23b80]);
    auto dx=magnitude(local.position[0]-point.position[0]);
    auto dy=magnitude(local.position[1]-point.position[1]);
    if(point.waypoint_radius_7c<dx && point.waypoint_radius_7c<dy)return report;
    if(signed_word(dy)<signed_word(dx))dy=half_signed(dy);else dx=half_signed(dx);
    if(dx+dy<point.waypoint_radius_7c){
        ++state.waypoint_index_c23b80;report.waypoint_advanced=true;
        if(signed_word(state.waypoint_count_c23b7c)<=signed_word(state.waypoint_index_c23b80)){
            state.waypoint_index_c23b80=0;report.waypoint_wrapped=true;
        }
    }
    return report;
}
} // namespace dfre
