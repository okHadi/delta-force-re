#include "dfre/mission_peer_projectile.hpp"
#include "dfre/motion_queries.hpp"
#include "dfre/mission_projectile_owner.hpp"
namespace dfre {
namespace {
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
void count(const MissionPeerParserCall& c,unsigned n){if(c.count!=n)throw MissionTickError("Peer projectile call argument count is invalid");}
void payload(const MissionPeerParserCall& c,unsigned index,std::size_t bytes){if(c.payload_argument!=int(index)||!c.payload||c.length!=bytes)throw MissionTickError("Peer projectile call needs its complete mutable source view");}
}
MissionPeerProjectile::MissionPeerProjectile(MissionTickState& state,MissionProjectileSpawn& spawn,const MissionPeerRequestBindings& requests):state_(state),spawn_(spawn),requests_(requests){}
bool MissionPeerProjectile::owns(std::uint32_t address)noexcept{return address==0x47df40||address==0x445b80||address==0x462f20;}
std::uint32_t MissionPeerProjectile::call(const MissionPeerParserCall& c){
    switch(c.address){
    case 0x47df40:
        count(c,2);if(!state_.world.terrain)throw MissionTickError("Peer projectile terrain is not bound");
        return std::uint32_t(ground_height_query(*state_.world.terrain,si(c.args[0]),si(c.args[1])));
    case 0x445b80:
        count(c,1);payload(c,0,15);mission_peer_record_shot(requests_,c.payload,c.length);return 0;
    case 0x462f20:{
        count(c,3);payload(c,0,20);std::uint32_t owner=0;
        if(c.args[1]){
            const auto ref=mission_projectile_owner_source_ref(state_,c.args[1]);owner=actor_handle(ref.group,ref.index);
        }
        return spawn_.spawn_view(owner,c.args[2],c.payload,c.length);
    }
    default:throw MissionTickError("Peer projectile adapter does not own this source body");
    }
}
}
