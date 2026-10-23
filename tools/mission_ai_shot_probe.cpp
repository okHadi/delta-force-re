#include "dfre/mission_ai_shot.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace dfre;
int main(){try{
    std::uint32_t weapon,burst,seed,flags,yaw,pitch,roll,x,y,z,mutation,graphic,model_present;
    while(std::cin>>weapon>>burst>>seed>>flags>>yaw>>pitch>>roll>>x>>y>>z>>mutation>>graphic>>model_present){
        MissionTickState state;AiActorTables tables;tables.groups[3].resize(3);
        for(unsigned i=0;i<3;++i){auto& a=tables.groups[3][i];a.active=true;a.definition_index=1;a.person_slot=i;}
        init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(3),seed);
        state.projectiles.resize(512);state.globals.mission_67f528=1;
        auto& actor=state.tables.groups[3][1];actor.field_10=graphic?1:0;
        actor.position[0]=x;actor.position[1]=y;actor.position[2]=z;
        actor.angle_yaw=yaw;actor.angle_pitch=pitch;state.runtime_for({3,1}).roll_38=roll;
        actor.field_6c=std::uint8_t(flags);state.extra_for({3,1}).field_94=weapon;
        state.person_extra(1).field_6c=burst;
        std::vector<RuntimeItemDefinition> items(4);items[1].id=0xfa1;items[2].id=0xfc1;items[3].id=0xfca;
        for(const auto& w:weapon_definitions())if(w.secondary_item){bool found=false;
            for(auto& item:items)if(item.id==w.secondary_item){item.model=model_present?1:0;found=true;}
            if(!found){RuntimeItemDefinition item;item.id=w.secondary_item;item.model=model_present?1:0;items.push_back(item);}
        }
        Model model;model.lods.resize(1);model.lods[0].hardpoints.resize(32,{10,20,30});
        MissionAnimations animations(std::vector<MissionAnimationBinding>{});
        MissionMuzzleResources resources;resources.definitions=items;resources.models={{1,&model}};
        MissionMuzzle muzzle(resources,animations);auto slot=matrix_identity();std::vector<std::uint32_t> trace;
        muzzle.set_slot_source([&](auto,const auto&)->Matrix4&{
            trace.push_back(1);
            if(mutation==1||mutation==3||mutation==5){state.extra_for({3,1}).field_94=mutation==5?1:7;
                actor.angle_yaw=0xaabbccdd;actor.angle_pitch=0x10203040;actor.person_slot=2;state.person_extra(2).field_6c=3;}
            if(mutation==6){actor.field_6c^=4;actor.angle_yaw=0x12345678;actor.angle_pitch=0x87654321;actor.position[0]+=0x10000;}
            if(mutation==3)throw std::runtime_error("graphic stopped");return slot;
        });
        MissionProjectileSpawnCells cells;for(auto& cls:cells.classes){cls.field_30=0x80;cls.field_20=0;cls.field_18=100;}
        MissionProjectileSpawnBindings b;
        b.ground=[&](auto&,auto gx,auto gy){trace.insert(trace.end(),{2,std::uint32_t(gx),std::uint32_t(gy)});return 0;};
        b.host_message=[&](auto&,auto& packet){trace.push_back(3);for(auto byte:packet)trace.push_back(byte);
            if(mutation==2||mutation==4){actor.person_slot=0;state.person_extra(0).field_6c=27;}
            if(mutation==4)throw std::runtime_error("delivery stopped");};
        b.loudness=[&](auto&,auto){const auto& p=state.projectiles.at(cells.ring_index);
            trace.insert(trace.end(),{4,std::uint32_t(p.position.x),std::uint32_t(p.position.y),std::uint32_t(p.position.z),p.aim_yaw,p.aim_pitch});return 0;};
        MissionProjectileSpawn spawn(state,cells,items,b);MissionAiShotBindings effects;
        effects.melee=[&](auto&,const auto&){trace.push_back(5);};MissionAiShot shot(state,muzzle,spawn,effects);
        bool failed=false;try{shot.fire({3,1});}catch(const std::runtime_error&){failed=true;}
        std::cout<<failed<<' '<<state.behavior.rng;
        for(unsigned i=0;i<3;++i)std::cout<<' '<<state.person_extra(i).field_6c;
        std::cout<<' '<<state.extra_for({3,1}).field_94<<' '<<actor.angle_yaw<<' '<<actor.angle_pitch<<' '<<*actor.person_slot;
        for(auto value:slot.m)std::cout<<' '<<std::uint32_t(value);
        std::cout<<' '<<trace.size();for(auto value:trace)std::cout<<' '<<value;std::cout<<'\n';
    }return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
