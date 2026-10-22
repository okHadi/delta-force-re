#include "dfre/mission_player_live.hpp"
#include "dfre/mission_projectile_live.hpp"
#include "dfre/mission_projectile_spawn.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace dfre;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void put(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value){
    for(unsigned i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(value>>(8*i));
}
void combat(){
    AiActorTables tables;tables.groups[3].resize(1);tables.groups[1].resize(1);
    auto& shooter=tables.groups[3][0];shooter.active=true;shooter.definition_index=1;
    shooter.person_slot=0;shooter.field_6c=1;shooter.field_70=1;
    auto& target=tables.groups[1][0];target.active=true;target.definition_index=2;
    target.field_10=17;target.field_110=0x1234;
    std::vector<AiItemDefinition> definitions(3);definitions[1].field_28=3;definitions[1].flags24=4;
    definitions[2].field_84=0x1234;
    MissionTickConfig config;config.local_actor=0;MissionTickState state;
    init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(definitions),std::vector<AiPersonState>(1),1);
    state.globals.mission_67f528=1;state.world.world_center={0x4000000,0x4000000,0};
    state.extra_for({3,0}).field_94=4;state.extra_for({1,0}).field_74=100;
    state.projectiles.resize(512);
    std::array<std::uint8_t,kWeaponSlotCount> map{};map.fill(4);
    state.weapons.set_slot_map(map);state.weapons.resupply(false);

    MissionAnimations animations(std::vector<MissionAnimationBinding>{});
    MissionMuzzleResources muzzle_resources;muzzle_resources.definitions.resize(1);
    MissionMuzzle muzzle(std::move(muzzle_resources),animations);
    MissionProjectileSpawnCells spawn_cells;
    auto& cls=spawn_cells.classes[17];cls.field_0c=124;cls.field_10=1;cls.field_14=0;
    cls.field_18=10;cls.field_20=0;cls.field_30=1;
    std::vector<RuntimeItemDefinition> items(4);
    items[1].id=0xfa1;items[2].id=0xfc1;items[3].id=0xfca;
    for(std::size_t i=1;i<items.size();++i)items[i].model=std::uint32_t(100+i);
    unsigned packets=0,shots=0;
    MissionProjectileSpawnBindings spawn_bindings;
    spawn_bindings.ground=[](auto&,auto,auto){return 0;};
    spawn_bindings.host_message=[&](auto&,const auto& packet){
        check(packet[8]==0&&packet[9]==0&&packet[14]==17,"shot packet lost owner zero or class");
        check(spawn_cells.ring_index==0,"shot spawned before packet delivery");++packets;
    };
    spawn_bindings.loudness=[](auto&,auto){return 0;};
    MissionProjectileSpawn spawn(state,spawn_cells,items,spawn_bindings);
    MissionPlayerCells player_cells;player_cells.camera_mode_6ba3ec=0;
    MissionPlayerProviders player_bindings;
    player_bindings.crt_random=[](auto&,auto&){return 0u;};
    player_bindings.projectile=[&](auto&,auto&,auto owner,auto,auto kind,auto& result){
        check(owner==actor_handle(3,0)&&kind==17,"firing used a different owner or class");
        ++shots;spawn.request(owner,kind,result);
    };
    unsigned weapon_effects=0;
    state.world.weapon_effect=[&](auto&,const WeaponEvent& effect){
        check(effect.kind!=WeaponEventKind::shot,"firing used isolated shot storage");++weapon_effects;return true;
    };
    MissionPlayerLive player(state,player_cells,muzzle,player_bindings);player.install();
    MissionTickReport firing;state.current_report=&firing;
    check(state.weapons.fire(),"live firing was refused");
    for(int tick=0;tick<weapon_definitions()[4].fire_delay;++tick)state.weapons.tick(false);
    check(state.weapons.reload_press(),"live reload was refused");
    state.current_report=nullptr;
    if(!weapon_effects||firing.trace.size()!=weapon_effects)
        throw std::runtime_error("Live weapon effects: "+std::to_string(weapon_effects)+
            ", trace records: "+std::to_string(firing.trace.size()));
    check(shots==1&&packets==1&&spawn_cells.ring_index==1,"firing did not reach the projectile ring");
    auto& projectile=state.projectiles[1];
    check(projectile.owner_slot==0&&projectile.ttl==10&&projectile.velocity.x>0,"spawned shot fields differ");

    Model model;model.version=5;model.render_types[0]=0x676e7263;model.lods.resize(1);
    auto& lod=model.lods[0];
    lod.vertices={{{0,-16384,-16384},0},{{0,16384,-16384},0},{{0,0,16384},0}};
    lod.normals={{{16384,0,0},4}};ModelFace face{};face.vertices={0,1,2};lod.faces={face};lod.materials.resize(1);
    ModelMesh mesh{};mesh.face_count=1;mesh.vertex_count=3;mesh.normal_count=1;mesh.raw.resize(112);
    for(auto at:{0x48u,0x50u})put(mesh.raw,at,std::uint32_t(-0x400000));
    for(auto at:{0x4cu,0x54u})put(mesh.raw,at,0x400000);
    lod.meshes={mesh};
    MissionGeometryModel binding;binding.handle=17;binding.model=&model;binding.bounds={0,0x640000};
    binding.faces={{0,{0,0,-0x400000,0x400000,-0x400000,0x400000}}};
    MissionGeometry geometry({binding});
    auto& destination=state.tables.groups[1][0];
    destination.position[0]=std::uint32_t(projectile.position.x+projectile.velocity.x/2);
    destination.position[1]=std::uint32_t(projectile.position.y);
    destination.position[2]=std::uint32_t(projectile.position.z);
    MissionProjectileLiveResources resources;resources.damage_thresholds={0,0,0};resources.materials={{17,{73},{0}}};
    ProjectileDamageState damage;MissionProjectileLiveBindings hit_bindings;
    hit_bindings.globals=[](const auto&){return MissionProjectileLiveGlobals{};};
    hit_bindings.terrain=[](auto&,const auto&,const auto&,auto){return 1;};
    unsigned updates=0,impacts=0,moves=0;
    hit_bindings.update=[&](auto& live,const AiActorRef& ref,auto callback,auto message){
        check(ref.group==1&&ref.index==0&&callback==0x1234&&message==1,"impact chose the wrong actor update");
        check(live.extra_for(ref).field_74==98&&live.extra_for(ref).field_f8==actor_handle(3,0),"impact lost damage or owner zero");
        check(projectile.ttl==10,"projectile cleared before actor update");++updates;
    };
    hit_bindings.impact=[&](auto&,auto& current,const ProjectileImpactEffect& effect){
        check(current.ttl==10&&effect.site==ProjectileImpactSite::visual,"impact effect order differs");++impacts;
    };
    hit_bindings.move=[&](auto&,auto& current,const auto&){check(current.ttl==0,"movement ran before impact removal");++moves;};
    auto query=mission_projectile_live_query(state,geometry,animations,std::move(resources),damage,hit_bindings);
    std::vector<ProjectileEvent> events;tick_projectile(projectile,state.world.world_center,query,events);
    check(updates==1&&impacts==1&&moves==1&&damage.last_damage==2,"live combat did not complete one impact");
    check(projectile.ttl==-1&&projectile.item_main==0,"impact did not clear the projectile");
}
}
int main(){try{combat();std::cout<<"live combat integration checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
