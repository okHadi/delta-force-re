#include "dfre/mission_fragment_spawn.hpp"
#include <utility>
namespace dfre {
namespace {
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::int32_t multiply(std::int32_t a,std::int32_t b){const auto value=std::uint64_t(std::int64_t(a)*b);return si(std::uint32_t(value>>22));}
}
MissionFragmentSpawn::MissionFragmentSpawn(MissionTickState& state,MissionProjectileSpawnCells& cells,MissionFragmentSpawnBindings bindings)
    :state_(state),cells_(cells),bindings_(std::move(bindings)){
    if(state_.projectiles.size()!=512)throw MissionTickError("Fragment spawn needs the shared512-slot projectile pool");
}
std::uint32_t MissionFragmentSpawn::spawn(const std::array<std::uint32_t,6>& position,std::uint32_t owner,std::uint32_t speed){
    if(state_.projectiles.size()!=512)throw MissionTickError("Fragment projectile pool size changed");
    const auto index=(cells_.ring_index+1u)&511u;cells_.ring_index=index;state_.projectiles[index]={};
    const auto pitch=position[4]&0xffff0000u,yaw=position[3]&0xffff0000u;
    if(!bindings_.sincos_4776c0)throw MissionTickError("Fragment sine/cosine body is not bound");
    std::int32_t sin_pitch=0,cos_pitch=0,sin_yaw=0,cos_yaw=0;
    bindings_.sincos_4776c0(pitch,sin_pitch,cos_pitch);bindings_.sincos_4776c0(yaw,sin_yaw,cos_yaw);
    if(state_.projectiles.size()!=512)throw MissionTickError("Fragment callback changed the projectile pool size");
    const auto step=si(speed)>>6;auto& output=state_.projectiles[index];
    output.velocity.x=multiply(multiply(cos_yaw,cos_pitch),step);output.velocity.y=multiply(multiply(sin_yaw,cos_pitch),step);
    output.class_flags=0x20;output.velocity.z=multiply(sin_pitch,step);
    output.aim_yaw=position[3];output.aim_pitch=position[4];output.position.x=si(position[0]);output.position.y=si(position[1]);
    const auto z=position[2];output.ttl=400;output.field_84=400;output.field_7c=0x40;output.field_80=1;output.color=0x01010101;
    output.item_main=0;output.item_alt=0;output.item_field=0;output.position.z=si(z);
    if(!owner)output.owner_slot=-1;
    else{
        const auto offset=owner-0x7cca80u;
        if(owner<0x7cca80u||offset%0x114u||offset/0x114u>=state_.tables.groups[3].size())throw MissionTickError("Fragment owner exceeds the current player table");
        output.owner_slot=std::int32_t(offset/0x114u);
    }
    return cells_.ring_index;
}
}
