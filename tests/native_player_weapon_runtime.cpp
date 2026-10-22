#include "dfre/player_weapon_runtime.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("player weapon: " #x);}while(false)
using namespace dfre;
int main(){try{
    {
        std::array<std::uint32_t,2> cells{0,27};unsigned person=0,spawns=0;
        PlayerShotBindings b;b.burst_storage=[&]()->std::uint32_t&{return cells[person];};
        b.projectile=[&](auto kind,auto&){++spawns;CHECK(kind==8 && cells[0]==2);cells[0]=9;};
        MuzzleResult muzzle;execute_player_shot(6,muzzle,b);CHECK(spawns==1 && cells[0]==9);
        cells[0]=3;b.projectile=[&](auto kind,auto&){++spawns;CHECK(kind==9 && cells[0]==3);cells[0]=9;};
        execute_player_shot(6,muzzle,b);CHECK(cells[0]==8);
        cells={3,27};b.projectile=[&](auto kind,auto&){CHECK(kind==9);person=1;};
        execute_player_shot(6,muzzle,b);CHECK(cells[0]==3 && cells[1]==26);
    }
    {
        WeaponState weapons;PlayerAimState aim{123,456};unsigned randoms=0;
        toggle_player_aim(weapons,aim,[&]{++randoms;CHECK(weapons.aim_block_raw()==1);weapons.set_aim_block_raw(7);return 0;});
        CHECK(randoms==1 && weapons.aim_block_raw()==7 && aim.sway_c23b68==0 && aim.sway_c23b6c==0x1fffff8);
        toggle_player_aim(weapons,aim,{});CHECK(weapons.aim_block_raw()==0 && aim.sway_c23b6c==0x1fffff8);
        bool failed=false;try{toggle_player_aim(weapons,aim,{});}catch(const PlayerStepError&){failed=true;}
        CHECK(failed && weapons.aim_block_raw()==1);
    }
    {
        WeaponState weapons;std::array<std::uint8_t,kWeaponSlotCount> map{};map.fill(6);weapons.set_slot_map(map);weapons.resupply(false);
        std::uint32_t burst=2;PlayerStepState player;player.local_actor_c23820=1;player.eye_c23a18=0xb333;unsigned shots=0;
        PlayerWeaponBindings b;b.burst_storage=[&]()->std::uint32_t&{return burst;};b.effect=[](const auto&){throw std::runtime_error("unexpected event");};
        b.view_body=[&](auto kind,auto id){
            CHECK(kind==WeaponViewKind::gun && id==6);
            PlayerGunBindings view;view.steady=[] {return false;};view.muzzle=[] {return MuzzleResult{};};view.crt_random=[] {return 0;};
            view.shot=[&](auto site,auto actor,auto& muzzle){
                CHECK(site==0x483198 && actor==1);
                PlayerShotBindings shot;shot.burst_storage=b.burst_storage;
                shot.projectile=[&](auto projectile,auto&){++shots;CHECK(projectile==9 && burst==2);burst=9;};
                execute_player_shot(6,muzzle,shot);
            };
            execute_player_view(kind,id,0,player,view);
        };
        const auto report=player_weapon_step(weapons,player,b,[](auto)->std::optional<PlayerStepActor>{return PlayerStepActor{};});
        CHECK(report.ran && shots==1 && burst==8 && weapons.fire_timer()==6 && weapons.pending(7)==0);
        CHECK(player.recoil_c239e8[0]==0xfffff880 && player.eye_c23a18==0xb333);
        CHECK(weapons.shot_mode==WeaponShotMode::isolated_fixture && !weapons.burst_storage && !weapons.view_body);
    }
    {
        WeaponState weapons;std::array<std::uint8_t,kWeaponSlotCount> map{};map.fill(12);weapons.set_slot_map(map);weapons.resupply(false);weapons.restore_timing({1,0,0});
        std::uint32_t burst=0,prior_burst=23;unsigned queries=0;
        weapons.burst_storage=[&]()->std::uint32_t&{return prior_burst;};weapons.trigger_query=[] {return true;};
        PlayerWeaponBindings b;b.burst_storage=[&]()->std::uint32_t&{return burst;};
        b.input_query=[&](auto command){CHECK(command==0x8c);++queries;weapons.set_aim_block_raw(1);return true;};
        player_weapon_prefix(weapons,b);CHECK(queries==1 && weapons.magazine(14)==199 && weapons.fire_timer()==0 && weapons.aim_blocked());
        CHECK(weapons.burst_counter()==23 && weapons.trigger_query());
        weapons.restore_timing({1,0,0});weapons.set_aim_block_raw(0);b.input_query={};bool failed=false;
        try{player_weapon_prefix(weapons,b);}catch(const PlayerStepError&){failed=true;}
        CHECK(failed && weapons.burst_counter()==23 && weapons.trigger_query());
    }
    {
        WeaponState weapons;std::uint32_t burst=0;PlayerWeaponBindings b;b.burst_storage=[&]()->std::uint32_t&{return burst;};
        weapons.restore_timing({std::int32_t(0x80000000u),std::int32_t(0x80000000u),std::int32_t(0x80000000u)});
        player_weapon_prefix(weapons,b);CHECK(weapons.fire_timer()==0x7fffffff && weapons.reload_timer()==0x7fffffff && weapons.raise_counter()==0);
        weapons.burst_storage=[]()->std::uint32_t&{throw PlayerStepError("resolver fixture");};bool failed=false;
        try{(void)weapons.burst_counter();}catch(const PlayerStepError&){failed=true;}CHECK(failed);
    }
    std::cout<<"player weapon runtime checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
