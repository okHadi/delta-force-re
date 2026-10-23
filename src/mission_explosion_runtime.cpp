#include "dfre/mission_explosion_runtime.hpp"
namespace dfre {
namespace {
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
bool accepts(const MissionTickState& state){return (!state.globals.multiplayer_67f520||state.globals.mission_67f528)&&state.pending_spawns.size()<30;}
void append(MissionTickState& state,std::uint32_t kind,Fixed3 p,std::uint32_t owner){
    MissionPendingSpawn row;row.owner=owner;row.type=si(kind);row.position[0]=p.x;row.position[1]=p.y;row.position[2]=p.z;state.pending_spawns.push_back(row);
}
std::uint32_t owner_identity(const MissionTickState& state,std::uint32_t source){
    if(!source)return 0;std::optional<std::uint32_t> result;
    for(std::size_t group=0;group<4;++group){
        const auto base=mission_actor_source_address(group,0);if(source<base)continue;const auto offset=source-base;
        if(offset%0x114u||offset/0x114u>=state.tables.groups[group].size())continue;
        if(result)throw MissionTickError("Explosion source owner overlaps actor tables");result=actor_handle(group,offset/0x114u);
    }
    if(!result)throw MissionTickError("Explosion source owner is not in a current actor table");return *result;
}
}
bool mission_explosion_enqueue(MissionTickState& state,std::uint32_t kind,Fixed3 position,std::uint32_t owner){
    if(!accepts(state))return false;append(state,kind,position,owner);return true;
}
bool mission_explosion_enqueue_source(MissionTickState& state,std::uint32_t kind,Fixed3 position,std::uint32_t owner){
    if(!accepts(state))return false;const auto identity=owner_identity(state,owner);append(state,kind,position,identity);return true;
}
}
