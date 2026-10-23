#include "dfre/mission_projectile_spawn.hpp"
#include <limits>
#include "dfre/mission_projectile_owner.hpp"

namespace dfre {
namespace {
template<class F> const F& need(const F& fn,const char* message){if(!fn)throw MissionTickError(message);return fn;}
std::uint32_t bits(std::int32_t n){return std::uint32_t(n);}
std::int32_t signed_bits(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::uint32_t read(const std::uint8_t* p,unsigned at,unsigned width){std::uint32_t n=0;for(unsigned i=0;i<width;++i)n|=std::uint32_t(p[at+i])<<(8*i);return n;}
void put(MissionSpawnPacket& p,unsigned at,std::uint32_t n,unsigned width){for(unsigned i=0;i<width;++i)p[at+i]=std::uint8_t(n>>(8*i));}
MissionSpawnPacket packet(const SpawnRecord& r){MissionSpawnPacket p{};put(p,0,r.packed_x,4);put(p,4,r.packed_y,4);put(p,8,r.actor_slot,2);put(p,10,r.aim_yaw_word,2);put(p,12,r.aim_pitch_word,2);p[14]=r.kind;return p;}
SpawnRecord record(const std::uint8_t* p){return {read(p,0,4),read(p,4,4),std::uint16_t(read(p,8,2)),std::uint16_t(read(p,10,2)),std::uint16_t(read(p,12,2)),p[14]};}
}
MissionProjectileSpawn::MissionProjectileSpawn(MissionTickState& state,MissionProjectileSpawnCells& cells,
    const std::vector<RuntimeItemDefinition>& definitions,MissionProjectileSpawnBindings bindings)
    :state_(state),cells_(cells),definitions_(definitions),bindings_(std::move(bindings)) {
    if(state_.projectiles.size()!=512)throw MissionTickError("Projectile spawn needs the initialized 512-slot pool");
    constexpr std::array<std::uint32_t,3> ids{0xfa1,0xfc1,0xfca};
    for(unsigned i=0;i<ids.size();++i){
        bool found=false;for(std::size_t j=0;j<definitions_.size();++j)if(definitions_[j].id==ids[i]){item_indexes_[i]=std::uint32_t(j);found=true;break;}
        if(!found)throw MissionTickError("Projectile spawn item is not loaded");
    }
}
AiActorRef MissionProjectileSpawn::owner_ref(std::uint32_t handle) const {
    if(handle<kActorHandleBase)throw MissionTickError("Projectile owner handle is invalid");
    const auto key=handle-kActorHandleBase;
    if(key/0x10000u>4)throw MissionTickError("Projectile owner group is invalid");
    const AiActorRef ref{std::uint8_t(key/0x10000u),key%0x10000u};
    (void)mission_projectile_owner_index(state_,ref);return ref;
}
void MissionProjectileSpawn::request(std::uint32_t owner,std::uint32_t kind,MuzzleResult& muzzle){
    // Original clears caller bytes before 47DF40 and reads the caller again
    // after that call. Ground and delivery providers can change that storage.
    const auto ref=owner_ref(owner);
    muzzle.origin.x=signed_bits(bits(muzzle.origin.x)&0xffffff00u);
    muzzle.origin.y=signed_bits(bits(muzzle.origin.y)&0xffffff00u);
    muzzle.origin.z=signed_bits(bits(muzzle.origin.z)&0xffffff00u);
    const auto height=need(bindings_.ground,"Projectile terrain height is not bound")(state_,muzzle.origin.x,muzzle.origin.y);
    const auto delta=bits(muzzle.origin.z)-bits(height);
    auto r=build_spawn_record({},muzzle.aim,std::uint16_t(mission_projectile_owner_index(state_,ref)),std::uint8_t(kind),[](auto,auto){return 0;});
    r.packed_x=bits(muzzle.origin.x)|((delta>>16)&0xffu);r.packed_y=bits(muzzle.origin.y)|((delta>>8)&0xffu);
    auto bytes=packet(r);
    if(state_.globals.mission_67f528)need(bindings_.host_message,"Projectile host delivery is not bound")(state_,bytes);
    else need(bindings_.queue,"Projectile message queue is not bound")(state_,9,{bytes.begin(),bytes.end()});
    spawn(owner,kind,muzzle);
}
void MissionProjectileSpawn::receive(MissionSpawnPacket& bytes){
    receive(bytes.data(),bytes.size());
}
void MissionProjectileSpawn::receive(std::uint8_t* bytes,std::size_t length){
    if(!bytes||length!=15)throw MissionTickError("Projectile receive needs fifteen mutable bytes");
    const auto input=record(bytes);
    const auto signed_slot=input.actor_slot<0x8000u?std::int32_t(input.actor_slot):std::int32_t(input.actor_slot)-0x10000;
    // The dispatcher saves the owner before terrain. Aim and kind remain live.
    std::int32_t height=0;
    auto decoded=decode_spawn_record(input,[&](auto x,auto y){height=need(bindings_.ground,"Projectile terrain height is not bound")(state_,x,y);return 0;});
    decoded.origin.z=signed_bits(bits(decoded.origin.z)+bits(height));
    const auto ref=mission_projectile_owner_ref(state_,signed_slot);
    MuzzleResult muzzle{decoded.origin,{read(bytes,10,2)<<16,read(bytes,12,2)<<16,0}};
    if(!state_.actor_at(ref)->active){
        const auto current_slot=read(bytes,8,2);
        need(bindings_.queue,"Projectile message queue is not bound")(state_,0x17,{std::uint8_t(current_slot),std::uint8_t(current_slot>>8)});
    }
    spawn(actor_handle(ref.group,ref.index),bytes[14],muzzle);
}
std::uint32_t MissionProjectileSpawn::spawn(std::uint32_t owner,std::uint32_t kind,MuzzleResult& muzzle){
    return spawn_current(owner,kind,[&muzzle]{return muzzle;});
}
std::uint32_t MissionProjectileSpawn::spawn_view(std::uint32_t owner,std::uint32_t kind,std::uint8_t* bytes,std::size_t length){
    if(!bytes||length!=20)throw MissionTickError("Projectile spawn needs twenty mutable pose bytes");
    return spawn_current(owner,kind,[bytes]{return MuzzleResult{{signed_bits(read(bytes,0,4)),signed_bits(read(bytes,4,4)),signed_bits(read(bytes,8,4))},{read(bytes,12,4),read(bytes,16,4),0}};});
}
std::uint32_t MissionProjectileSpawn::spawn_current(std::uint32_t owner,std::uint32_t kind,const std::function<MuzzleResult()>& current){
    if(kind>=cells_.classes.size())throw MissionTickError("Projectile class exceeds the loaded table");
    if(state_.projectiles.size()!=512)throw MissionTickError("Projectile pool size changed");
    const auto ref=owner?std::optional<AiActorRef>(owner_ref(owner)):std::nullopt;
    auto& cls=cells_.classes[kind];const auto index=(cells_.ring_index+1u)&511u;cells_.ring_index=index;
    const auto team=ref?state_.actor_at(*ref)->field_70:0u;
    const auto color=team==1?0xb9a0a0ffu:team==2?0xb9ffa0a0u:0xffffffffu;
    const auto f=cls.field_30;
    std::array<std::int32_t,2> items{};std::int32_t model=0;std::optional<unsigned> choice;
    if(f&1)choice=0;else if(f&0x10)choice=1;else if(f&0x40)choice=2;
    if(choice){const auto item=item_indexes_[*choice];items={std::int32_t(item),std::int32_t(item)};model=signed_bits(definitions_[item].model);}
    const auto initial=current();
    state_.projectiles[index]=spawn_projectile(initial.origin,initial.aim,{cls.field_0c,cls.field_10,cls.field_14,cls.field_18,f},items,model,
        ref?mission_projectile_owner_index(state_,*ref):-1,color,(cells_.rules_8cf982&0x10)!=0);
    if(choice&&*choice==2){
        const auto& item=definitions_[item_indexes_[2]];
        if(item.ambient_handles[0])need(bindings_.attached_sound,"Projectile attached sound is not bound")(state_,index,item.ambient_handles[0],item.ambient_sound[0],0);
    }
    if(cls.field_20&&!(cells_.rules_67dbc9&2))need(bindings_.positional_sound,"Projectile positional sound is not bound")(state_,cls.field_20,current().origin);
    const auto loudness=need(bindings_.loudness,"Projectile loudness is not bound")(state_,cls.field_20);
    if(loudness){
        if(!ref)throw MissionTickError("Projectile alert needs an owner");
        const auto image=snapshot_mission_actor(state_,*ref);
        const auto team_word=std::uint32_t(image[0x70])|std::uint32_t(image[0x71])<<8;
        const auto team_signed=team_word<0x8000u?std::int32_t(team_word):std::int32_t(team_word)-0x10000;
        need(bindings_.alert,"Projectile alert is not bound")(state_,current().origin,loudness>>1,team_signed,2);
    }
    if((cls.field_30&6)||!(cls.field_30&0x80)){
        if(!ref)throw MissionTickError("Projectile class flags need an owner");
        auto& extra=state_.extra_for(*ref);
        if(cls.field_30&2)extra.field_6d|=8;
        if(cls.field_30&4)extra.field_6d|=0x10;
        if(!(cls.field_30&0x80))extra.field_6d|=0x40;
    }
    return cells_.ring_index;
}
} // namespace dfre
