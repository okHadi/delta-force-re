#include "dfre/mission_projectile_owner.hpp"
namespace dfre {
namespace {
struct Pool {std::uint8_t group;std::uint32_t first,count;};
constexpr Pool pools[]={{3,0,256},{0,256,1024},{4,1280,1024},{1,2304,1024},{2,3328,512}};
void loaded(const MissionTickState& state,const AiActorRef& ref){
    if(!state.actor_at(ref))throw MissionTickError("Projectile owner has no current actor record");
}
}
AiActorRef mission_projectile_owner_ref(const MissionTickState& state,std::int32_t index){
    if(index<0)throw MissionTickError("Projectile owner index is negative");
    const auto value=std::uint32_t(index);
    for(const auto& pool:pools)if(value>=pool.first&&value-pool.first<pool.count){
        const AiActorRef ref{pool.group,value-pool.first};loaded(state,ref);return ref;
    }
    throw MissionTickError("Projectile owner index exceeds the physical actor pools");
}
std::int32_t mission_projectile_owner_index(const MissionTickState& state,const AiActorRef& ref){
    for(const auto& pool:pools)if(ref.group==pool.group){
        if(ref.index>=pool.count)throw MissionTickError("Projectile owner exceeds its physical pool");
        loaded(state,ref);return std::int32_t(pool.first+ref.index);
    }
    throw MissionTickError("Projectile owner group is invalid");
}
AiActorRef mission_projectile_owner_source_ref(const MissionTickState& state,std::uint32_t source){
    if(source<0x7cca80u||source>=0x8cf680u||(source-0x7cca80u)%0x114u)
        throw MissionTickError("Projectile owner source is outside the aligned actor pools");
    return mission_projectile_owner_ref(state,std::int32_t((source-0x7cca80u)/0x114u));
}
}
