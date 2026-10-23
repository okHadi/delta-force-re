#include "dfre/mission_explosion_runtime.hpp"
#include <iostream>
using namespace dfre;
namespace {
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::uint32_t raw_owner(std::uint32_t id){if(!id)return 0;const auto key=id-kActorHandleBase;return mission_actor_source_address(key/0x10000u,key%0x10000u);}
}
int main(){unsigned seed;while(std::cin>>seed){
 MissionTickState state;for(unsigned g=0;g<4;++g)state.tables.groups[g].resize(8);
 state.globals.multiplayer_67f520=std::array<std::uint32_t,4>{0,1,0xffffffff,0x80000000}[seed%4];state.globals.mission_67f528=(seed/4)%3;
 const auto count=seed%33;for(unsigned i=0;i<count;++i)state.pending_spawns.push_back({{si(seed+i*3),si(0u-seed-i),si(seed^i)},si(i),i%5?actor_handle(i%4,i%8):0});
 std::uint32_t random=seed+1;std::array<std::uint32_t,4> args{};for(auto& x:args){random=random*1664525u+1013904223u;x=random;}
 const auto source=seed%5?mission_actor_source_address((seed/5)%4,seed%8):0;
 const auto accepted=mission_explosion_enqueue_source(state,args[0],{si(args[1]),si(args[2]),si(args[3])},source);
 std::cout<<"{\"accepted\":"<<(accepted?1:0)<<",\"count\":"<<state.pending_spawns.size()<<",\"rows\":[";
 for(std::size_t i=0;i<state.pending_spawns.size();++i){if(i)std::cout<<',';const auto& r=state.pending_spawns[i];std::cout<<'['<<std::uint32_t(r.position[0])<<','<<std::uint32_t(r.position[1])<<','<<std::uint32_t(r.position[2])<<','<<std::uint32_t(r.type)<<','<<raw_owner(r.owner)<<']';}std::cout<<"]}\n";
}}
