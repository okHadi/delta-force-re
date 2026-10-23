#include "dfre/mission_ai_shot.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/world_runtime.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace dfre;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Fixture {
    MissionTickState state;
    MissionAnimations animations{std::vector<MissionAnimationBinding>{}};
    Model model;
    std::vector<RuntimeItemDefinition> items;
    MissionMuzzleResources resources;
    MissionProjectileSpawnCells cells;
    Matrix4 slot=matrix_identity();
    unsigned deliveries=0;
    MissionSpawnPacket packet{};
    Fixture(){
        AiActorTables tables;tables.groups[3].resize(3);
        for(unsigned i=0;i<3;++i){auto& a=tables.groups[3][i];a.active=true;a.definition_index=1;
            a.person_slot=i;a.field_10=1;a.position[0]=i*0x10000u;a.position[1]=0x20000;a.position[2]=0x30000;}
        MissionTickConfig config;config.local_actor=0;
        init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),
            std::vector<AiPersonState>(3),0x12345678);
        state.projectiles.resize(512);state.globals.mission_67f528=1;
        state.extra_for({3,1}).field_94=6;
        items.resize(4);items[1].id=0xfa1;items[2].id=0xfc1;items[3].id=0xfca;
        for(const auto& w:weapon_definitions())if(w.secondary_item){
            bool found=false;for(auto& item:items)if(item.id==w.secondary_item){item.model=1;found=true;}
            if(!found){RuntimeItemDefinition item;item.id=w.secondary_item;item.model=1;items.push_back(item);}
        }
        model.lods.resize(1);model.lods[0].hardpoints.resize(8,{10,20,30});
        resources.definitions=items;resources.models={{1,&model}};
        for(auto& cls:cells.classes){cls.field_30=0x80;cls.field_20=0;cls.field_18=100;}
    }
    MissionProjectileSpawnBindings spawn_bindings(){
        MissionProjectileSpawnBindings b;b.ground=[](auto&,auto,auto){return 0;};
        b.host_message=[&](auto&,auto& bytes){++deliveries;packet=bytes;};
        b.loudness=[](auto&,auto){return 0;};return b;
    }
};
}
int main(){try{
    {
        Fixture f;MissionMuzzle muzzle(f.resources,f.animations);
        muzzle.set_slot_source([&](auto,const auto&)->Matrix4&{return f.slot;});
        MissionProjectileSpawn spawn(f.state,f.cells,f.items,f.spawn_bindings());MissionAiShot shot(f.state,muzzle,spawn);
        f.state.tables.groups[3][1].angle_yaw=0x12345678;f.state.tables.groups[3][1].angle_pitch=0x11223344;
        const auto seed=f.state.behavior.rng;
        check(shot.fire({3,1}),"NPC shot did not run");
        check(f.deliveries==1&&f.cells.ring_index==1&&f.state.projectiles[1].owner_slot==1,"NPC used a different pool or owner");
        check(f.packet[8]==1&&f.packet[9]==0&&f.packet[14]==8,"NPC request used the wrong wire owner or kind");
        check(f.state.person_extra(1).field_6c==2&&f.state.person_extra(0).field_6c==0,"NPC used the local person");
        check(f.state.tables.groups[3][1].angle_yaw==0x12345678&&f.state.tables.groups[3][1].angle_pitch==0x11223344,"NPC shot restored or changed caller aim");
        check(f.state.behavior.rng==seed,"unit spread consumed a random value");
    }
    {
        Fixture f;MissionMuzzle muzzle(f.resources,f.animations);
        muzzle.set_slot_source([&](auto,const auto&)->Matrix4&{
            f.state.extra_for({3,1}).field_94=7;f.state.tables.groups[3][1].angle_yaw=0xaabbccdd;
            f.state.tables.groups[3][1].angle_pitch=0x10203040;f.state.tables.groups[3][1].person_slot=2;
            f.state.person_extra(2).field_6c=3;return f.slot;
        });
        auto bindings=f.spawn_bindings();bindings.host_message=[&](auto& state,auto& bytes){
            check(bytes[14]==9,"shot did not reread weapon/person after graphic callback");
            state.tables.groups[3][1].person_slot=0;state.person_extra(0).field_6c=27;
        };
        MissionProjectileSpawn spawn(f.state,f.cells,f.items,bindings);MissionAiShot shot(f.state,muzzle,spawn);
        shot.fire({3,1});
        check(f.state.person_extra(2).field_6c==3&&f.state.person_extra(0).field_6c==26,"burst decrement did not use current person");
        check(f.state.projectiles[1].aim_yaw!=0&&f.state.tables.groups[3][1].angle_yaw==0xaabbccdd,"callback aim was lost");
    }
    {
        Fixture f;MissionMuzzle muzzle(f.resources,f.animations);
        muzzle.set_slot_source([&](auto,const auto&)->Matrix4&{return f.slot;});
        auto bindings=f.spawn_bindings();bindings.host_message=[&](auto& state,auto&){
            state.tables.groups[3][1].angle_pitch=0xabcdef01;throw std::runtime_error("delivery stopped");};
        MissionProjectileSpawn spawn(f.state,f.cells,f.items,bindings);MissionAiShot shot(f.state,muzzle,spawn);
        bool failed=false;try{shot.fire({3,1});}catch(const std::runtime_error& e){failed=std::string(e.what())=="delivery stopped";}
        check(failed&&f.cells.ring_index==0&&f.state.person_extra(1).field_6c==2&&f.state.tables.groups[3][1].angle_pitch==0xabcdef01,
            "delivery failure did not retain the source write prefix");
    }
    {
        Fixture f;MissionMuzzle muzzle(f.resources,f.animations);
        muzzle.set_slot_source([&](auto,const auto&)->Matrix4&{return f.slot;});
        MissionProjectileSpawn spawn(f.state,f.cells,f.items,f.spawn_bindings());MissionAiShot shot(f.state,muzzle,spawn);
        ActorQueryTable table{&f.state.tables.groups[3],&f.state.people,&f.state.actor_extras[3],&f.state.person_extras};
        ActorQueryData data;data.raw_pointer=[](std::size_t i){return mission_actor_source_address(3,i);};
        auto& person=f.state.person_extra(1);person.field_44=mission_actor_source_address(3,2);
        person.field_48=0x90000;person.field_4c=0x30000;person.field_50=0x40000;person.field_6c=1;
        auto& actor=f.state.tables.groups[3][1];actor.angle_yaw=0x12345678;actor.angle_pitch=0x87654321;
        const auto yaw=actor.angle_yaw,pitch=actor.angle_pitch;unsigned effects=0;
        f.state.behavior.on_effect=[&](const BehaviorAction& action){check(action.site==0x45e7a0,"combat emitted another effect");++effects;shot.fire({3,1});};
        query_combat_step_45f2c0(table,1,data,f.state.behavior);
        check(effects==1&&f.cells.ring_index==1&&actor.angle_yaw==yaw&&actor.angle_pitch==pitch,"combat temporary aim did not reach the shared shot");
        f.state.behavior.on_effect=[&](const BehaviorAction&){shot.fire({3,1});throw std::runtime_error("combat stopped");};
        bool failed=false;try{query_combat_step_45f2c0(table,1,data,f.state.behavior);}
        catch(const std::runtime_error& error){failed=std::string(error.what())=="combat stopped";}
        check(failed&&(actor.angle_yaw!=yaw||actor.angle_pitch!=pitch),"failed combat restored its temporary aim");
    }
    {
        Fixture f;MissionMuzzle muzzle(f.resources,f.animations);
        muzzle.set_slot_source([&](auto,const auto&)->Matrix4&{return f.slot;});
        MissionProjectileSpawn spawn(f.state,f.cells,f.items,f.spawn_bindings());MissionAiShot shot(f.state,muzzle,spawn);
        f.state.extra_for({3,1}).field_94=1;bool failed=false;
        try{shot.fire({3,1});}catch(const MissionTickError&){failed=true;}
        check(failed&&f.cells.ring_index==0,"missing melee body was accepted");
        f.state.tables.groups[3][1].field_10=0;
        check(!shot.fire({3,1}),"missing graphic guard changed");
        failed=false;try{shot.fire({0,0});}catch(const MissionTickError&){failed=true;}
        check(failed,"AI shot accepted another actor group");
    }
    std::cout<<"mission AI shot native checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
