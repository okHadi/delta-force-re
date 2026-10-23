#include "dfre/mission_object_live.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;
namespace {std::uint32_t word(const AiActorImage& image,std::size_t at){std::uint32_t n;std::memcpy(&n,image.data()+at,4);return n;}
struct Fixture {MissionTickState state;Fixture(){AiActorTables tables;tables.groups[0].resize(1);auto& a=tables.groups[0][0];a.active=true;a.definition_index=1;a.person_slot=0;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(2),123);auto image=snapshot_mission_actor(state,{0,0});image[0x74]=100;apply_mission_actor_image(state,{0,0},image);state.person_extra(0).field_a4=1;state.people[0].field_ac=0;}}
;}
int main(){
    {Fixture f;bool failed=false;try{mission_object_veh0(f.state,{0,0},{});}catch(const MissionTickError&){failed=true;}assert(failed);}
    {Fixture f;MissionObjectLiveBindings b;unsigned calls=0;
        b.call=[&](auto& state,const auto&,const auto& action){++calls;assert(action.site==0x45c6a0);state.tables.groups[0][0].person_slot=1;state.person_extra(0).field_a4=0;state.person_extra(1).field_a4=7;state.behavior.rng=456;return std::array<std::uint32_t,6>{1,100,50,10,0,0};};
        mission_object_veh0(f.state,{0,0},b);assert(calls==1&&f.state.tables.groups[0][0].person_slot==1&&f.state.person_extra(0).field_a4==0&&f.state.person_extra(1).field_a4==7&&f.state.behavior.rng==456);assert(word(snapshot_mission_actor(f.state,{0,0}),0x88)==0);}
    {Fixture f;MissionObjectLiveBindings b;b.call=[](auto& state,const auto&,const auto&)->std::array<std::uint32_t,6>{state.people[0].field_b0=99;state.behavior.rng=789;throw MissionTickError("Foreign failure");};
        bool failed=false;try{mission_object_veh0(f.state,{0,0},b);}catch(const MissionTickError&){failed=true;}assert(failed&&f.state.people[0].field_b0==99&&f.state.behavior.rng==789);}
    {Fixture f;bind_mission_object_veh0(f.state,{});assert(f.state.update_callbacks.count(0x429bc0)==1&&f.state.secondary_callbacks.empty());bool failed=false;try{bind_mission_object_veh0(f.state,{});}catch(const MissionTickError&){failed=true;}assert(failed);}
    {Fixture f;MissionObjectLiveBindings b;unsigned ground=0,sine=0;
        b.call=[&](auto& state,const auto&,const auto& action){std::array<std::uint32_t,6> result{};
            if(action.site==0x477820){++sine;return result;}
            assert(action.site==0x47df40);
            if(ground==0)state.tables.groups[0][0].position[0]=100;
            if(ground==1)assert(action.args[0]==100);
            if(ground==2)assert(state.tables.groups[0][0].angle_pitch==0xfffff800u);
            result[0]=++ground;return result;};
        b.type_slot=[](auto&,auto slot,auto offset){assert(slot==0&&offset==0x3c);return 1u;};
        mission_object_veh0_move(f.state,{0,0},b);const auto image=snapshot_mission_actor(f.state,{0,0});
        assert(sine==1&&ground==4&&word(image,0x20)==0x10000&&word(image,0x24)==100&&word(image,0x38)==0xffffe000u&&word(image,0x2c)==1);
        b.type_slot=[](auto& state,auto,auto){state.tables.groups[0][0].field_7c=0x80000000;return 1u;};
        bool failed=false;try{mission_object_veh0_move(f.state,{0,0},b);}catch(const ObjectBehaviorError&){failed=true;}assert(failed&&f.state.tables.groups[0][0].field_7c==0x80000000);
    }
    {Fixture f;bind_mission_object_veh0_move(f.state,{});assert(f.state.secondary_callbacks.count(0x429ea0)==1&&f.state.update_callbacks.count(0x429bc0)==0);}
    {Fixture f;MissionObjectLiveBindings b;unsigned markers=0;
        b.call=[&](auto&,const auto&,const auto& action)->MissionObjectCallResult{
            MissionObjectCallResult result;if(action.site==0x42ee30)return result;
            assert(action.site==0x45c6a0);if(markers++==0)result.values={1,123,0,0,0,0};else result.output_mask=0;
            return result;};
        b.global=[](auto&,auto at){assert(at==0x78f284);return 3u;};b.type_slot=[](auto&,auto,auto){return 2u;};
        mission_object_veh0(f.state,{0,0},b);assert(markers==2&&word(snapshot_mission_actor(f.state,{0,0}),0x8c)==123);
    }
    {Fixture f;auto image=snapshot_mission_actor(f.state,{0,0});const auto put=[&](auto at,std::uint32_t n){std::memcpy(image.data()+at,&n,4);};
        put(0xe0,1);put(0xe4,0x30000);put(0xe8,8);put(0xec,1);put(0x8c,0x01000000);apply_mission_actor_image(f.state,{0,0},image);
        MissionObjectLiveBindings b;unsigned reads=0;b.definition=[&](auto&,auto index,auto at){assert(index==1&&at==0xe0);++reads;return 16u;};
        mission_object_slgt_move(f.state,{0,0},b);image=snapshot_mission_actor(f.state,{0,0});assert(reads==2&&word(image,0xec)==3&&word(image,0x30)==0x10000&&word(image,0x90)==0x10000);
        put(0x30,0);put(0x8c,0);put(0x34,0);put(0xf0,0);apply_mission_actor_image(f.state,{0,0},image);mission_object_slgt_move(f.state,{0,0},{});image=snapshot_mission_actor(f.state,{0,0});
        assert(word(image,0xe8)==0xfffffff8u&&word(image,0x8c)==0xfffffff9u&&word(image,0xf0)==0x30000&&(image[0x6d]&1));
    }
    {Fixture f;f.state.tables.groups[0][0].person_slot.reset();f.state.tables.groups[0][0].field_6c=4;bool failed=false;try{mission_object_slgt_move(f.state,{0,0},{});}catch(const MissionTickError&){failed=true;}assert(failed&&f.state.people.size()==2);}
    {Fixture f;f.state.tables.groups[3].resize(1);auto& target=f.state.tables.groups[3][0];target.active=true;target.definition_index=1;target.field_72=2;
        f.state.people[0].flags24=0xff;f.state.person_extra(1).field_44=0x1234;
        MissionObjectLiveBindings b;unsigned calls=0;b.call=[&](auto& current,const auto&,const auto& action){std::array<std::uint32_t,6> out{};
            if(calls++==0){assert(action.site==0x44f540);current.tables.groups[0][0].person_slot=1;out[0]=mission_actor_source_address(3,0);out[1]=0x7fffffff;}
            else{assert(action.site==0x42edd0&&action.args[0]==0&&action.args[1]==2);current.tables.groups[0][0].person_slot.reset();}
            return out;};
        mission_object_slgt_ai(f.state,{0,0},b);assert(calls==2&&!f.state.tables.groups[0][0].person_slot&&f.state.person_extra(0).field_38==8&&f.state.person_extra(0).field_44==mission_actor_source_address(3,0)&&f.state.people[0].flags24==0xbf&&f.state.person_extra(1).field_44==0x1234);
        bind_mission_object_slgt_ai(f.state,b);assert(f.state.update_callbacks.count(0x4519a0)==1);
    }
    {Fixture f;MissionObjectLiveBindings b;b.call=[](auto& current,const auto&,const auto&)->MissionObjectCallResult{current.person_extra(0).field_38=99;current.behavior.rng=456;throw MissionTickError("Searchlight query failure");};
        bool failed=false;try{mission_object_slgt_ai(f.state,{0,0},b);}catch(const MissionTickError&){failed=true;}assert(failed&&f.state.person_extra(0).field_38==99&&f.state.behavior.rng==456&&word(snapshot_mission_actor(f.state,{0,0}),0xe0)==1);
    }

}
