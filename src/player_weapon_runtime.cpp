#include "dfre/player_weapon_runtime.hpp"
#include <utility>
#include <cstring>

namespace dfre {
namespace {
class BindingsScope {
    WeaponState& state_;
    decltype(WeaponState::burst_storage) burst_;
    decltype(WeaponState::on_event) effect_;
    decltype(WeaponState::trigger_query) query_;
    WeaponShotMode mode_;
    decltype(WeaponState::view_body) view_;
public:
    BindingsScope(WeaponState& state,const PlayerWeaponBindings& bindings):state_(state),
        burst_(std::move(state.burst_storage)),effect_(std::move(state.on_event)),
        query_(std::move(state.trigger_query)),mode_(state.shot_mode),view_(std::move(state.view_body)){
        state.burst_storage=bindings.burst_storage;
        state.shot_mode=WeaponShotMode::live_body;state.view_body=bindings.view_body;
        state.on_event=[&bindings](const WeaponEvent& event){
            if(!bindings.effect)throw PlayerStepError("player weapon: effect binding missing");
            bindings.effect(event);
        };
        state.trigger_query=[&bindings]{
            if(!bindings.input_query)throw PlayerStepError("player weapon: input query missing (0x47A290)");
            return bindings.input_query(0x8c);
        };
    }
    ~BindingsScope(){state_.burst_storage=std::move(burst_);state_.on_event=std::move(effect_);
        state_.trigger_query=std::move(query_);state_.shot_mode=mode_;state_.view_body=std::move(view_);}
};
}
void player_weapon_prefix(WeaponState& state,const PlayerWeaponBindings& bindings){
    if(!bindings.burst_storage)throw PlayerStepError("player weapon: person burst source missing");
    BindingsScope scope(state,bindings);state.tick(false);
}
PlayerStepReport player_weapon_step(WeaponState& weapons,PlayerStepState& player,
    const PlayerWeaponBindings& bindings,
    const std::function<std::optional<PlayerStepActor>(std::uint32_t)>& actor){
    PlayerStepWorld world;world.actor=actor;
    world.weapon_prefix=[&](PlayerStepState&){player_weapon_prefix(weapons,bindings);};
    return step_player(player,world);
}
void toggle_player_aim(WeaponState& weapons,PlayerAimState& aim,
    const std::function<std::uint32_t()>& random){
    if(weapons.aim_block_raw()){weapons.set_aim_block_raw(0);return;}
    weapons.set_aim_block_raw(1);
    if(!random)throw PlayerStepError("player aim: random source missing");
    const auto angle=random()<<16;
    std::int32_t sine=0,cosine=0;sincos_q22(angle,sine,cosine);
    aim.sway_c23b68=std::uint32_t(sine)<<3;aim.sway_c23b6c=std::uint32_t(cosine)<<3;
}
void execute_player_shot(std::uint32_t weapon,MuzzleResult& muzzle,
    const PlayerShotBindings& bindings){
    if(weapon>=kWeaponCount)throw PlayerStepError("player shot: weapon outside the table");
    const auto& definition=weapon_definitions()[weapon];
    const auto spread=weapon_classes()[definition.weapon_class].field_1c;
    if(spread!=1){
        if(!bindings.random)throw PlayerStepError("player shot: random source missing");
        const auto a=bindings.random(),b=bindings.random();
        muzzle.aim.yaw+=(spread-1)*30u*(0x8000u-a);
        muzzle.aim.pitch+=(spread-1)*30u*(0x8000u-b);
    }
    const auto projectile=[&](std::uint32_t kind){
        if(!bindings.projectile)throw PlayerStepError("player shot: projectile binding missing");
        bindings.projectile(kind,muzzle);
    };
    const auto burst=[&]()->std::uint32_t&{
        if(!bindings.burst_storage)throw PlayerStepError("player shot: person burst source missing");
        return bindings.burst_storage();
    };
    const auto sequence=[&](std::uint32_t first,std::uint32_t next){
        if(burst()){projectile(next);--burst();}
        else{burst()=2;projectile(first);}
    };
    switch(weapon){
    case 1:if(!bindings.melee)throw PlayerStepError("player shot: melee binding missing");bindings.melee();return;
    case 4:case 5:case 10:case 28:case 33:projectile(definition.weapon_class);return;
    case 6:case 7:case 8:case 9:
        if(burst()){projectile(9);--burst();}
        else if(weapon==6 || weapon==8){burst()=2;projectile(8);}
        else projectile(7);
        return;
    case 12:case 13:projectile(14);return;
    case 15:projectile(15);return;
    case 20:case 22:projectile(4);return;
    case 21:sequence(5,6);return;
    case 32:projectile(22);return;
    case 35:sequence(23,24);return;
    case 36:sequence(25,26);return;
    default:return;
    }
}
} // namespace dfre

namespace dfre {
void execute_player_view(WeaponViewKind kind,std::uint32_t weapon,std::uint32_t camera_mode,
    PlayerStepState& player,const PlayerGunBindings& bindings){
    const auto actor=[&]{
        if(!bindings.actor)throw PlayerStepError("player gun: actor source missing");
        const auto result=bindings.actor(player.local_actor_c23820);
        if(!result)throw PlayerStepError("player gun: actor handle missing");
        return *result;
    };
    if(kind==WeaponViewKind::claymore || kind==WeaponViewKind::satchel){
        const auto view=actor();std::int32_t sine=0,cosine=0;
        sincos_q22(view.angles.yaw,sine,cosine);
        auto position=view.position;
        const auto x=std::uint32_t(position.x)+std::uint32_t((std::int64_t(cosine)*0x10000)>>22);
        const auto y=std::uint32_t(position.y)+std::uint32_t((std::int64_t(sine)*0x10000)>>22);
        std::memcpy(&position.x,&x,4);std::memcpy(&position.y,&y,4);
        if(!bindings.place)throw PlayerStepError("player view: placement body missing");
        bindings.place(kind==WeaponViewKind::satchel?0xfb4u:(weapon==37?0xfaeu:0xfc8u),position,view.angles.yaw);
    } else if(camera_mode && kind!=WeaponViewKind::grenade){
        if(!bindings.animated)throw PlayerStepError("player gun: animated body missing (0x45E7A0)");
        bindings.animated(player.local_actor_c23820);
    } else {
        if(kind==WeaponViewKind::gun && !bindings.steady)throw PlayerStepError("player gun: steady query missing");
        MuzzleResult muzzle;std::uint32_t site=kind==WeaponViewKind::gun?0x483198u:
            kind==WeaponViewKind::launcher?0x483573u:kind==WeaponViewKind::melee?0x4839dfu:0x483748u;
        if(kind==WeaponViewKind::gun && bindings.steady()){
            const auto view=actor();muzzle.origin=view.position;muzzle.aim=view.angles;
            const auto z=std::uint32_t(muzzle.origin.z)+player.eye_c23a18-0x200;
            std::memcpy(&muzzle.origin.z,&z,4);site=0x48314b;
        } else {
            if(!bindings.muzzle)throw PlayerStepError("player gun: muzzle body missing");
            muzzle=bindings.muzzle();
        }
        if(!bindings.shot)throw PlayerStepError("player gun: full shot body missing (0x45EA00)");
        bindings.shot(site,player.local_actor_c23820,muzzle);
    }
    if(kind==WeaponViewKind::gun)player.recoil_c239e8[0]=0xfffff800;
    if(!bindings.crt_random)throw PlayerStepError("player gun: CRT random source missing (0x49AF6C)");
    const auto random=bindings.crt_random();
    std::int32_t sine=0,cosine=0;sincos_q22(random<<17,sine,cosine);
    player.recoil_c239e8[1]=std::uint32_t((std::int64_t(sine)*0x400)>>22);
    player.recoil_c239e8[2]=std::uint32_t((std::int64_t(cosine)*0x400)>>22);
    if(kind!=WeaponViewKind::gun){
        const auto amount=kind==WeaponViewKind::launcher?0x4000u:kind==WeaponViewKind::melee?0x2000u:0x1000u;
        player.recoil_c239e8[2]-=kind==WeaponViewKind::launcher?0x4000u:0x2000u;
        player.recoil_c239e8[0]-=amount;
    }
    if(kind==WeaponViewKind::gun && weapon==15){
        sincos_q22(actor().angles.yaw,sine,cosine);
        if(!bindings.translate_actor)throw PlayerStepError("player gun: actor movement binding missing");
        bindings.translate_actor(player.local_actor_c23820,{
            -std::int32_t((std::int64_t(cosine)*0x800)>>22),
            -std::int32_t((std::int64_t(sine)*0x800)>>22)});
    }
}
} // namespace dfre
