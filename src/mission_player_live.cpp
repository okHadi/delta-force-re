#include "dfre/mission_player_live.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <utility>

namespace dfre {
namespace {
std::uint32_t word(const AiActorImage& image,std::size_t at){
    std::uint32_t value;std::memcpy(&value,image.data()+at,4);return value;
}
std::int32_t signed_word(std::uint32_t value){
    std::int32_t result;std::memcpy(&result,&value,4);return result;
}
}
MissionPlayerLive::MissionPlayerLive(MissionTickState& state,MissionPlayerCells& cells,
    MissionMuzzle& muzzle,MissionPlayerProviders providers):state_(state),cells_(cells),
    muzzle_(muzzle),providers_(std::move(providers)){}
MissionPlayerLive::~MissionPlayerLive(){uninstall();}
void MissionPlayerLive::install(){
    if(installed_)return;
    prior_burst_=std::move(state_.weapons.burst_storage);
    prior_view_=std::move(state_.weapons.view_body);
    prior_effect_=std::move(state_.weapons.on_event);
    prior_query_=std::move(state_.weapons.trigger_query);
    prior_mode_=state_.weapons.shot_mode;
    state_.weapons.burst_storage=[this]()->std::uint32_t&{return mission_player_burst(state_);};
    state_.weapons.view_body=[this](auto kind,auto weapon){view(kind,weapon);};
    state_.weapons.on_event=[this](const WeaponEvent& event){boundary([&]{
        mission_weapon_effect(state_,event);
    });};
    state_.weapons.trigger_query=[this]{bool held=false;boundary([&]{
        if(!state_.world.trigger_held_47a290)throw MissionTickError("Player input provider is required");
        held=state_.world.trigger_held_47a290(0x8c);
    });return held;};
    state_.weapons.shot_mode=WeaponShotMode::live_body;installed_=true;
}
void MissionPlayerLive::uninstall() noexcept{
    if(!installed_)return;
    state_.weapons.burst_storage=std::move(prior_burst_);
    state_.weapons.view_body=std::move(prior_view_);
    state_.weapons.on_event=std::move(prior_effect_);
    state_.weapons.trigger_query=std::move(prior_query_);
    state_.weapons.shot_mode=prior_mode_;installed_=false;
}
AiActorRef MissionPlayerLive::ref(std::uint32_t handle) const{
    if(handle)for(std::size_t group=0;group<4;++group)
        for(std::size_t slot=0;slot<state_.tables.groups[group].size();++slot)
            if(actor_handle(group,slot)==handle)return {std::uint8_t(group),slot};
    throw MissionTickError("Player needs a current mission actor");
}
std::uint32_t MissionPlayerLive::local() const{
    return state_.config.local_actor?actor_handle(3,*state_.config.local_actor):0;
}
void MissionPlayerLive::load(PlayerStepState& player) const{
    player.local_actor_c23820=local();player.prone_c23828=std::uint32_t(state_.movement.prone_c23828);
    player.game_type_67f53c=state_.game_type_67f53c;
    player.recoil_c239e8=cells_.recoil_c239e8;player.eye_c23a18=cells_.eye_c23a18;
    player.waypoint_count_c23b7c=cells_.waypoint_count_c23b7c;
    player.waypoint_index_c23b80=cells_.waypoint_index_c23b80;
    player.waypoints_c23b84=cells_.waypoints_c23b84;
}
void MissionPlayerLive::save(const PlayerStepState& player){
    cells_.recoil_c239e8=player.recoil_c239e8;cells_.eye_c23a18=player.eye_c23a18;
    cells_.waypoint_count_c23b7c=player.waypoint_count_c23b7c;
    cells_.waypoint_index_c23b80=player.waypoint_index_c23b80;
    cells_.waypoints_c23b84=player.waypoints_c23b84;
}
void MissionPlayerLive::boundary(const std::function<void()>& call){
    if(active_)save(*active_);
    const auto previous=in_foreign_;in_foreign_=true;
    try{call();}catch(...){in_foreign_=previous;if(active_)load(*active_);throw;}
    in_foreign_=previous;
    if(active_)load(*active_);
}
void MissionPlayerLive::context(const std::function<void(PlayerStepState&)>& call){
    if(active_){
        if(!in_foreign_){call(*active_);return;}
        load(*active_);in_foreign_=false;
        try{call(*active_);}catch(...){in_foreign_=true;save(*active_);throw;}
        in_foreign_=true;save(*active_);return;
    }
    PlayerStepState player;load(player);active_=&player;
    const auto previous=in_foreign_;in_foreign_=false;
    try{call(player);}catch(...){active_=nullptr;in_foreign_=previous;save(player);throw;}
    active_=nullptr;in_foreign_=previous;save(player);
}
void MissionPlayerLive::shot(std::uint32_t site,std::uint32_t handle,MuzzleResult& muzzle){
    const auto actor_ref=ref(handle);
    const auto image=snapshot_mission_actor(state_,actor_ref);
    PlayerShotBindings bindings;
    bindings.random=[this]{return behavior_rng_next(state_.behavior);};
    bindings.burst_storage=[this,actor_ref]()->std::uint32_t&{
        const auto* actor=state_.actor_at(actor_ref);
        if(!actor || !actor->person_slot || *actor->person_slot>=state_.people.size())
            throw MissionTickError("Player shot needs its current person");
        return state_.person_extra(*actor->person_slot).field_6c;
    };
    bindings.projectile=[&,this](std::uint32_t kind,MuzzleResult& result){boundary([&]{
        if(!providers_.projectile)throw MissionTickError("Player projectile provider is required");
        providers_.projectile(state_,cells_,handle,site,kind,result);
    });};
    bindings.melee=[&,this]{boundary([&]{
        if(!providers_.melee)throw MissionTickError("Player melee provider is required");
        providers_.melee(state_,cells_,handle,site);
    });};
    execute_player_shot(word(image,0x94),muzzle,bindings);
}
PlayerWeaponBindings MissionPlayerLive::weapon_bindings(){
    PlayerWeaponBindings bindings;
    bindings.burst_storage=[this]()->std::uint32_t&{return mission_player_burst(state_);};
    bindings.effect=[this](const WeaponEvent& event){boundary([&]{
        mission_weapon_effect(state_,event);
    });};
    bindings.input_query=[this](std::uint32_t command){bool result=false;boundary([&]{
        if(!state_.world.trigger_held_47a290)throw MissionTickError("Player input provider is required");
        result=state_.world.trigger_held_47a290(command);
    });return result;};
    bindings.view_body=[this](WeaponViewKind kind,std::uint32_t weapon){view(kind,weapon);};
    return bindings;
}
void MissionPlayerLive::view(WeaponViewKind kind,std::uint32_t weapon){
    context([&](PlayerStepState& player){
        PlayerGunBindings bindings;
        bindings.steady=[this]{return mission_steady(state_)!=0;};
        bindings.actor=[this](std::uint32_t handle)->std::optional<PlayerGunActor>{
            const auto image=snapshot_mission_actor(state_,ref(handle));
            return PlayerGunActor{{signed_word(word(image,0x24)),signed_word(word(image,0x28)),signed_word(word(image,0x2c))},
                {word(image,0x30),word(image,0x34),word(image,0x38)}};
        };
        bindings.muzzle=[&,this]{
            const auto image=snapshot_mission_actor(state_,ref(local()));
            const auto& recoil=player.recoil_c239e8;
            return muzzle_.normal(image,state_.weapons.current_weapon(),
                {{signed_word(recoil[0]),signed_word(recoil[1]),signed_word(recoil[2])},signed_word(player.eye_c23a18)});
        };
        bindings.shot=[this](auto site,auto handle,MuzzleResult& result){shot(site,handle,result);};
        bindings.animated=[this](std::uint32_t handle){
            auto image=snapshot_mission_actor(state_,ref(handle));
            boundary([&]{muzzle_.animated({&image,handle},[this](ProjectileHitActor actor,const MuzzleResult& result){
                if(active_)load(*active_);
                auto current=result;shot(0x45e946,actor.identity,current);
            });});
        };
        bindings.translate_actor=[this](std::uint32_t handle,std::array<std::int32_t,2> delta){
            auto* actor=state_.actor_at(ref(handle));
            actor->position[0]+=std::uint32_t(delta[0]);actor->position[1]+=std::uint32_t(delta[1]);
        };
        bindings.place=[this](auto item,auto position,auto yaw){boundary([&]{
            if(!providers_.place)throw MissionTickError("Player placement provider is required");
            providers_.place(state_,cells_,item,position,yaw);
        });};
        bindings.crt_random=[this]{std::uint32_t result=0;boundary([&]{
            if(!providers_.crt_random)throw MissionTickError("Player CRT random provider is required");
            result=providers_.crt_random(state_,cells_);
        });return result;};
        const bool uses_camera=kind==WeaponViewKind::gun || kind==WeaponViewKind::launcher || kind==WeaponViewKind::melee;
        if(uses_camera && !cells_.camera_mode_6ba3ec)
            throw MissionTickError("Player view needs camera mode 6BA3EC");
        execute_player_view(kind,weapon,uses_camera?*cells_.camera_mode_6ba3ec:0,player,bindings);
    });
}
std::optional<std::uint32_t> MissionPlayerLive::stage(const MissionStageCall& call){
    if(call.address==0x484c20){
        toggle_player_aim(state_.weapons,cells_.aim,[this]{return behavior_rng_next(state_.behavior);});return 0;
    }
    if(call.address!=0x482b00)return std::nullopt;
    context([&](PlayerStepState& player){
        player_weapon_step(state_.weapons,player,weapon_bindings(),[this](std::uint32_t handle)->std::optional<PlayerStepActor>{
            const auto image=snapshot_mission_actor(state_,ref(handle));
            return PlayerStepActor{{word(image,0x24),word(image,0x28)},image[0x6e],word(image,0x7c)};
        });
    });return 0;
}
} // namespace dfre
