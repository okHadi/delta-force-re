#include "dfre/mission_player_live.hpp"
#include "dfre/world_runtime.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("mission player: " #x);}while(false)
using namespace dfre;
namespace {
MissionStageCall stage_call(std::uint32_t address){MissionStageCall call;call.address=address;return call;}
struct Fixture {
    MissionTickState state;
    MissionPlayerCells cells;
    MissionAnimations animations{std::vector<MissionAnimationBinding>{}};
    MissionMuzzle muzzle{MissionMuzzleResources{{RuntimeItemDefinition{}},{},{}},animations};
    Fixture(){
        AiActorTables tables;tables.groups[3].resize(2);
        for(std::size_t i=0;i<2;++i){auto& a=tables.groups[3][i];
            a.active=true;a.definition_index=1;a.person_slot=i;a.position[0]=std::uint32_t(100+i*1000);
        }
        MissionTickConfig config;config.local_actor=0;
        init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),
            std::vector<AiPersonState>(2),0x12345678);
        std::array<std::uint8_t,kWeaponSlotCount> map{};map.fill(6);
        state.weapons.set_slot_map(map);state.weapons.resupply(false);
        state.extra_for({3,0}).field_94=6;state.extra_for({3,1}).field_94=6;
        cells.camera_mode_6ba3ec=0;
    }
};
}
int main(){try{
    {
        Fixture f;unsigned shots=0,crts=0;
        f.state.person_extra(0).field_6c=2;
        MissionPlayerProviders p;
        p.projectile=[&](auto& state,auto& cells,auto actor,auto site,auto kind,auto&){
            ++shots;CHECK(actor==actor_handle(3,0) && site==0x483198 && kind==9);
            CHECK(state.person_extra(0).field_6c==2 && cells.recoil_c239e8[0]==0);
            state.tables.groups[3][0].person_slot=1;state.person_extra(1).field_6c=27;
            state.config.local_actor=1;state.movement.prone_c23828=1;state.game_type_67f53c=1;
            cells.eye_c23a18=0x7777;cells.recoil_c239e8[0]=123;
        };
        p.crt_random=[&](auto&,auto& cells){++crts;CHECK(cells.recoil_c239e8[0]==0xfffff800);
            cells.recoil_c239e8[0]=0x300;return 0u;};
        MissionPlayerLive live(f.state,f.cells,f.muzzle,p);
        CHECK(live.stage(stage_call(0x482b00)).has_value());
        CHECK(shots==1 && crts==1 && f.state.person_extra(0).field_6c==2 && f.state.person_extra(1).field_6c==26);
        CHECK(f.cells.recoil_c239e8[0]==0x280 && f.cells.eye_c23a18!=0x7777 && f.state.config.local_actor==1);
        CHECK(f.state.weapons.shot_mode==WeaponShotMode::isolated_fixture && !f.state.weapons.view_body);
        CHECK(!live.stage(stage_call(0x111111)));
    }
    {
        Fixture f;MissionPlayerProviders p;
        p.projectile=[](auto& state,auto& cells,auto,auto,auto,auto&){
            state.config.local_actor=1;cells.eye_c23a18=12345;cells.recoil_c239e8[2]=999;
            throw std::runtime_error("spawn stopped");
        };
        MissionPlayerLive live(f.state,f.cells,f.muzzle,p);
        bool failed=false;try{live.view(WeaponViewKind::gun,6);}catch(const std::runtime_error&){failed=true;}
        CHECK(failed && f.cells.eye_c23a18==12345 && f.cells.recoil_c239e8[2]==999 && f.state.config.local_actor==1);
        CHECK(f.state.person_extra(0).field_6c==2);
        f.state.person_extra(1).field_6c=0;
        failed=false;try{live.stage(stage_call(0x482b00));}catch(const std::runtime_error&){failed=true;}
        CHECK(!failed); // Context is restored; an idle prefix needs no spawn or CRT.
    }
    {
        Fixture f;MissionPlayerLive live(f.state,f.cells,f.muzzle,{});
        f.state.config.local_actor.reset();CHECK(live.stage(stage_call(0x482b00))==0);
        const auto before=f.state.behavior.rng;
        CHECK(live.stage(stage_call(0x484c20))==0 && f.state.weapons.aim_block_raw()==1 && f.state.behavior.rng!=before);
        const auto after=f.state.behavior.rng;
        live.stage(stage_call(0x484c20));CHECK(f.state.weapons.aim_block_raw()==0 && f.state.behavior.rng==after);
        f.state.config.local_actor=0;f.cells.camera_mode_6ba3ec.reset();
        bool failed=false;try{live.view(WeaponViewKind::gun,6);}catch(const MissionTickError&){failed=true;}
        CHECK(failed && f.state.person_extra(0).field_6c==0);
        f.cells.camera_mode_6ba3ec=0;
        failed=false;try{live.view(WeaponViewKind::gun,6);}catch(const MissionTickError&){failed=true;}
        CHECK(failed && f.state.person_extra(0).field_6c==2);
    }
    {
        Fixture f;std::array<std::uint8_t,kWeaponSlotCount> map{};map.fill(12);f.state.weapons.set_slot_map(map);
        f.state.weapons.resupply(false);f.state.weapons.restore_timing({1,0,0});
        unsigned queries=0;f.state.world.trigger_held_47a290=[&](auto command){
            CHECK(command==0x8c);++queries;f.state.weapons.set_aim_block_raw(1);
            f.state.config.local_actor=1;f.cells.eye_c23a18=0x6666;return true;
        };
        MissionPlayerLive live(f.state,f.cells,f.muzzle,{});live.stage(stage_call(0x482b00));
        CHECK(queries==1 && f.state.weapons.aim_blocked() && f.state.config.local_actor==1);
        CHECK(f.state.weapons.fire_timer()==0 && f.cells.eye_c23a18!=0xb333);
    }
    {
        Fixture f;f.cells.camera_mode_6ba3ec=1;
        Model graphic;graphic.lods.resize(1);graphic.lods[0].hardpoints.resize(8,{10,20,30});
        MissionMuzzleResources resources;resources.definitions.resize(2);
        resources.definitions[1].id=weapon_definitions()[6].secondary_item;resources.definitions[1].model=1;
        resources.models={{1,&graphic}};
        MissionMuzzle animated(resources,f.animations);
        f.state.tables.groups[3][0].field_10=1;
        auto scratch=matrix_identity();unsigned slots=0,shots=0;
        animated.set_slot_source([&](auto,const auto& image)->Matrix4&{
            ++slots;CHECK(image[0x34]==0 && f.cells.recoil_c239e8[0]==0);
            f.state.config.local_actor=1;f.state.extra_for({3,0}).field_94=7;
            f.state.person_extra(0).field_6c=3;f.cells.eye_c23a18=0x9999;return scratch;
        });
        MissionPlayerProviders p;
        p.projectile=[&](auto& state,auto& cells,auto actor,auto site,auto kind,auto&){
            ++shots;CHECK(actor==actor_handle(3,0) && site==0x45e946 && kind==9);
            CHECK(cells.eye_c23a18==0x9999 && state.config.local_actor==1);
            state.tables.groups[3][0].person_slot=1;state.person_extra(1).field_6c=11;
        };
        p.crt_random=[](auto&,auto&){return 0u;};
        MissionPlayerLive live(f.state,f.cells,animated,p);live.view(WeaponViewKind::gun,6);
        CHECK(slots==1 && shots==1 && f.state.person_extra(0).field_6c==3 && f.state.person_extra(1).field_6c==10);
        CHECK(f.cells.eye_c23a18==0x9999 && f.state.config.local_actor==1);
    }
    {
        Fixture f;f.cells.recoil_c239e8={1234,5678,0xffffd8f1};
        const auto expected=f.muzzle.normal(snapshot_mission_actor(f.state,{3,0}),6,
            {{1234,5678,-9999},0xb333});
        unsigned shots=0;MissionPlayerProviders p;
        p.projectile=[&](auto&,auto&,auto,auto,auto,MuzzleResult& muzzle){
            ++shots;CHECK(muzzle.origin.x==expected.origin.x && muzzle.origin.y==expected.origin.y &&
                muzzle.origin.z==expected.origin.z && muzzle.aim.yaw==expected.aim.yaw &&
                muzzle.aim.pitch==expected.aim.pitch && muzzle.aim.roll==expected.aim.roll);
        };
        p.crt_random=[](auto&,auto&){return 0u;};
        MissionPlayerLive live(f.state,f.cells,f.muzzle,p);live.view(WeaponViewKind::gun,6);
        CHECK(shots==1);
    }
    {
        Fixture f;MissionPlayerLive live(f.state,f.cells,f.muzzle,{});
        for(const auto kind:{WeaponViewKind::melee,WeaponViewKind::claymore,WeaponViewKind::satchel}){
            bool failed=false;try{live.view(kind,kind==WeaponViewKind::melee?1:37);}
            catch(const MissionTickError&){failed=true;}CHECK(failed);
        }
        auto binding=live.weapon_bindings();bool failed=false;
        try{binding.input_query(0x8c);}catch(const MissionTickError&){failed=true;}CHECK(failed);
        f.state.world.weapon_effect=[](auto&,const auto&){return false;};failed=false;
        try{binding.effect({WeaponEventKind::sound,1});}catch(const MissionTickError&){failed=true;}CHECK(failed);
        MissionPlayerProviders p;p.projectile=[](auto&,auto&,auto,auto,auto,auto&){};
        MissionPlayerLive no_random(f.state,f.cells,f.muzzle,p);failed=false;
        try{no_random.view(WeaponViewKind::gun,6);}catch(const MissionTickError&){failed=true;}
        CHECK(failed && f.cells.recoil_c239e8[0]==0xfffff800);
    }
    {
        Fixture f;unsigned shots=0,effects=0,previous=0;
        std::array<std::uint8_t,kWeaponSlotCount> map{};map.fill(10);
        f.state.weapons.set_slot_map(map);f.state.weapons.resupply(false);f.state.extra_for({3,0}).field_94=10;
        f.state.weapons.on_event=[&](const auto&){++previous;};
        f.state.world.weapon_effect=[&](auto&,const WeaponEvent& event){
            CHECK(event.kind!=WeaponEventKind::shot);++effects;return true;
        };
        MissionPlayerProviders p;
        p.projectile=[&](auto&,auto&,auto actor,auto,auto kind,auto&){
            ++shots;CHECK(actor==actor_handle(3,0) && kind==13);
        };
        p.crt_random=[](auto&,auto&){return 0u;};
        {
            MissionPlayerLive live(f.state,f.cells,f.muzzle,p);live.install();live.install();
            CHECK(live.installed() && f.state.weapons.shot_mode==WeaponShotMode::live_body);
            // The actual command pump fires before its full player prefix.
            f.state.globals.pause_6e4714=1;
            f.state.world.bounds=[](auto){return AiActorBounds{};};
            f.state.world.external_stage=[&](auto&,const auto& call)->std::optional<std::uint32_t>{
                const auto result=live.stage(call);return result?result:std::optional<std::uint32_t>{0};
            };
            MissionTickInputs inputs;DispatchResult fire;
            fire.calls.push_back({DispatchCallKind::weapon_fire,0x483c70,{},false});inputs.dispatch.push_back(fire);
            const auto report=mission_tick(f.state,inputs);
            CHECK(shots==1);CHECK(effects==1);CHECK(previous==0);CHECK(f.state.person_extra(0).field_6c==0);
            unsigned traced=0;for(const auto& entry:report.trace)if(entry.kind==MissionTraceKind::weapon_event)++traced;
            CHECK(traced==effects && f.state.weapons.shot_mode==WeaponShotMode::live_body);
            live.uninstall();CHECK(!live.installed() && f.state.weapons.shot_mode==WeaponShotMode::isolated_fixture);
            live.install();
        }
        CHECK(f.state.weapons.shot_mode==WeaponShotMode::isolated_fixture && !f.state.weapons.view_body &&
            !f.state.weapons.burst_storage && !f.state.weapons.trigger_query);
        f.state.weapons.on_event({WeaponEventKind::sound,1});CHECK(previous==1);
    }
    std::cout<<"mission player live checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
