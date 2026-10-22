#include "dfre/player_step.hpp"
#include <iostream>
#define CHECK(x) do{if(!(x))throw std::runtime_error("player step: " #x);}while(false)
using namespace dfre;
int main(){
    PlayerStepState state;CHECK(!step_player(state,{}).ran);
    state.local_actor_c23820=1;bool failed=false;
    try{step_player(state,{});}catch(const PlayerStepError&){failed=true;}CHECK(failed);
    PlayerStepWorld world;unsigned calls=0;
    world.weapon_prefix=[&](auto& s){++calls;s.recoil_c239e8={0xffffffff,128,0x80000000};s.local_actor_c23820=2;};
    world.actor=[](std::uint32_t handle)->std::optional<PlayerStepActor>{
        if(handle==2)return PlayerStepActor{{0,0},2,0};
        if(handle==3)return PlayerStepActor{{0,0},0,100};
        return std::nullopt;
    };
    state.waypoint_count_c23b7c=2;state.waypoint_index_c23b80=1;state.waypoints_c23b84={3,3};
    const auto report=step_player(state,world);CHECK(calls==1 && report.ran && report.waypoint_advanced && report.waypoint_wrapped);
    CHECK(state.recoil_c239e8[0]==0 && state.recoil_c239e8[1]==0 && state.recoil_c239e8[2]==0x80000080);
    CHECK(state.waypoint_index_c23b80==0);CHECK(state.eye_c23a18==1966);
    world.weapon_prefix=[](auto&){};state.waypoints_c23b84.clear();failed=false;
    try{step_player(state,world);}catch(const PlayerStepError&){failed=true;}CHECK(failed);
    std::cout<<"player step checks passed\n";
}
