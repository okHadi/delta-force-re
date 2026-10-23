#include "dfre/mission_object_helo.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/motion_queries.hpp"
#include "dfre/world_runtime.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class B> U get(const B& bytes,U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;}
template<class B> void put(B& bytes,U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));}
struct Fixture {
    static constexpr U a=0x7dde80,p=0x794680,d=0x1302000;
    MissionTickState state;std::array<std::uint8_t,0x13c> definition{};
    TerrainGeometry terrain{AssetBytes(1024*1024),AssetBytes(1024*1024),std::vector<U>(65536)};
    bool switch_person=false;unsigned random_calls=0,ground_calls=0;
    Fixture(){
        AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];
        actor.active=true;actor.definition_index=1;actor.person_slot=0;actor.field_10c=0x451590;
        actor.position[0]=0x20000;actor.position[1]=0x30000;actor.position[2]=0x50000;
        init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(2),123);
        state.globals.mission_67f528=1;state.world.terrain=&terrain;
        put(definition,0xdc,0x100);put(definition,0xe0,32);
        auto image=snapshot_mission_actor(state,{0,0});put(image,0x98,0x14000000);put(image,0x88,0x10000);apply_mission_actor_image(state,{0,0},image);
    }
    AiActorImage raw_actor(){auto image=snapshot_mission_actor(state,{0,0});put(image,4,d);
        const auto person=state.tables.groups[0][0].person_slot;put(image,0xc,person?p+U(*person)*0xb4:0);return image;}
    AiPersonImage person(U slot){auto image=ai_person_image(state.people.at(slot));put(image,0x44,state.person_extra(slot).field_44);put(image,0x6c,state.person_extra(slot).field_6c);return image;}
    MissionObjectHeloBindings bindings(){MissionObjectHeloBindings b;
        b.read=[&](U at,U width){
            if(at>=a&&std::uint64_t(at-a)+width<=0x114)return get(raw_actor(),at-a,width);
            if(at>=p&&std::uint64_t(at-p)+width<=0xb4*2){const auto offset=at-p;return get(person(offset/0xb4),offset%0xb4,width);}
            if(at>=d&&std::uint64_t(at-d)+width<=definition.size())return get(definition,at-d,width);
            if(at==0x67f528&&width==4)return state.globals.mission_67f528;
            throw MissionTickError("Native helicopter read has no owner");
        };
        b.write=[&](U at,U width,U n){
            if(at>=a&&std::uint64_t(at-a)+width<=0x114){auto image=snapshot_mission_actor(state,{0,0});put(image,at-a,n,width);apply_mission_actor_image(state,{0,0},image);return;}
            if(at>=p&&std::uint64_t(at-p)+width<=0xb4*2){const auto offset=at-p,slot=offset/0xb4;auto image=person(slot);put(image,offset%0xb4,n,width);
                state.person_extra(slot).field_6c=get(image,0x6c);state.people.at(slot).field_90=get(image,0x90);return;}
            throw MissionTickError("Native helicopter write has no owner");
        };
        b.call=[&](U site,const auto& args)->U{
            if(site==0x477860){++random_calls;if(switch_person)state.tables.groups[0][0].person_slot=1;return behavior_rng_next(state.behavior);}
            if(site==0x47df40){++ground_calls;return U(ground_height_query(terrain,std::int32_t(args.at(0)),std::int32_t(args.at(1))));}
            throw MissionTickError("Native helicopter required body has no owner");
        };return b;
    }
};
}
int main(){try{
    {
        Fixture f;bind_mission_object_helo_move(f.state,f.bindings());
        AiActorAction action{AiActorActionKind::secondary,{0,0},0x451590};
        f.state.secondary_callbacks.at(0x451590)(f.state,{0,0},action);
        check(f.state.tables.groups[0].size()==1&&f.state.people.size()==2,"helicopter created another actor/person pool");
        const auto image=f.raw_actor();check(get(image,0x9c)==0x14000000&&get(image,0x7c)==0x100,"helicopter did not update canonical rotor/speed");
        check(f.state.tables.groups[0][0].position[0]==0x20003&&f.state.tables.groups[0][0].angle_pitch==0xffff0000,
            "helicopter did not integrate canonical position/pitch");
        bool failed=false;try{bind_mission_object_helo_move(f.state,f.bindings());}catch(const MissionTickError&){failed=true;}check(failed,"duplicate helicopter callback was accepted");
    }
    {
        Fixture f;f.switch_person=true;f.state.person_extra(0).field_44=1;
        auto image=snapshot_mission_actor(f.state,{0,0});put(image,0xc4,0);put(image,0xcc,0);apply_mission_actor_image(f.state,{0,0},image);
        mission_object_helo_move(f.bindings(),Fixture::a);
        check(f.random_calls==2&&f.state.tables.groups[0][0].person_slot==1,"helicopter random callback did not use current state");
        check(f.state.people[0].field_90>=64&&f.state.person_extra(0).field_6c>=16,"helicopter lost its captured source person");
        check(f.state.people[1].field_90==0&&f.state.person_extra(1).field_6c==0,"helicopter changed the replacement person");
    }
    {
        Fixture f;auto image=snapshot_mission_actor(f.state,{0,0});put(image,0x2c,0);put(image,0xac,0xffffe000);apply_mission_actor_image(f.state,{0,0},image);
        bool failed=false;try{mission_object_helo_move(f.bindings(),Fixture::a);}catch(const MissionTickError&){failed=true;}
        check(failed&&f.ground_calls==1&&f.state.tables.groups[0][0].position[2]==0x3f000,"missing crash body lost the source terrain/clamp prefix");
        check((f.state.tables.groups[0][0].field_6c&4)==0,"missing crash body reported a completed crash");
    }
    std::cout<<"mission object helicopter native checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
