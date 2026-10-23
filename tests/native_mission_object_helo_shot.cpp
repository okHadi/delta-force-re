#include "dfre/mission_object_helo_shot.hpp"
#include "dfre/mission_object_helo_pose.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/mission_projectile_spawn.hpp"
#include "dfre/player_weapon_runtime.hpp"
#include "dfre/model_specials.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::int32_t si(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
template<class B> U get(const B& bytes,U at){U n=0;for(U i=0;i<4;++i)n|=U(bytes.at(at+i))<<(8*i);return n;}
template<class B> void put(B& bytes,U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));}
struct Fixture {
    static constexpr U a=0x7cca80+0x114,p=0x794680,g=0x1301000,l=0x1302000,d=0x1303000,f=0x1304000,pf=0x1305000,hp=0x1306000;
    MissionTickState state;Model model;std::array<std::uint8_t,0x1c> frame{};std::array<std::uint8_t,0x80> pose_frame{};
    std::vector<RuntimeItemDefinition> items;MissionProjectileSpawnCells cells;MissionSpawnPacket packet{};
    bool change=false;unsigned shot_calls=0;
    Fixture(){AiActorTables tables;tables.groups[3].resize(3);for(unsigned i=0;i<3;++i){auto& actor=tables.groups[3][i];actor.active=true;actor.definition_index=1;actor.person_slot=i;actor.field_10=17;}
        auto& actor=tables.groups[3][1];actor.position[0]=0x20000;actor.position[1]=0x30000;actor.position[2]=0x40000;
        MissionTickConfig config;config.local_actor=0;init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(3),123);
        state.extra_for({3,1}).field_94=6;state.person_extra(1).field_48=0x60000;state.person_extra(1).field_4c=0x30000;state.person_extra(1).field_50=0x40000;
        state.projectiles.resize(512);state.globals.mission_67f528=1;model.lods.resize(1);model.lods[0].hardpoints.resize(9);model.lods[0].hardpoints[4]={10,20,30};
        items.resize(4);items[1].id=0xfa1;items[2].id=0xfc1;items[3].id=0xfca;
        for(auto& cls:cells.classes){cls.field_30=0x80;cls.field_20=0;cls.field_18=100;}
    }
    U read(U at)const{
        if(at>=a&&std::uint64_t(at-a)+4<=0x114){auto image=snapshot_mission_actor(state,{3,1});if(at==a+0x10)return g;if(at==a+4)return d;if(at==a+0xc)return p+U(*state.tables.groups[3][1].person_slot)*0xb4;return get(image,at-a);}
        if(at>=p&&std::uint64_t(at-p)+4<=0xb4*3){const auto off=at-p;const auto slot=off/0xb4,part=off%0xb4;auto image=ai_person_image(state.people.at(slot));const auto& extra=state.person_extras.at(slot);put(image,0x48,extra.field_48);put(image,0x4c,extra.field_4c);put(image,0x50,extra.field_50);return get(image,part);}
        if(at==g+0x50)return l;if(at==l+0xa4)return hp;if(at==d+0x20)return 0x7e2;
        if(at>=hp&&std::uint64_t(at-hp)+4<=model.lods[0].hardpoints.size()*12){const auto off=at-hp;return U(model.lods[0].hardpoints.at(off/12).at(off%12/4));}
        if(at>=f&&std::uint64_t(at-f)+4<=frame.size())return get(frame,at-f);
        if(at>=pf&&std::uint64_t(at-pf)+4<=pose_frame.size())return get(pose_frame,at-pf);
        throw MissionTickError("Native helicopter shot read has no owner");
    }
    void write(U at,U n){
        if(at>=a&&std::uint64_t(at-a)+4<=0x114){auto image=snapshot_mission_actor(state,{3,1});put(image,at-a,n);apply_mission_actor_image(state,{3,1},image);return;}
        if(at>=f&&std::uint64_t(at-f)+4<=frame.size()){put(frame,at-f,n);return;}
        if(at>=pf&&std::uint64_t(at-pf)+4<=pose_frame.size()){put(pose_frame,at-pf,n);return;}
        throw MissionTickError("Native helicopter shot store has no owner");
    }
    void run(){
        MissionProjectileSpawnBindings sb;sb.ground=[](auto&,auto,auto){return 0;};sb.loudness=[](auto&,auto){return 0;};
        sb.host_message=[&](auto& current,auto& bytes){packet=bytes;if(change){current.tables.groups[3][1].person_slot=0;current.person_extra(0).field_6c=27;}};
        MissionProjectileSpawn spawn(state,cells,items,sb);MissionObjectHeloBindings b;
        b.read=[&](U at,U width){check(width==4,"shot read width");return read(at);};b.write=[&](U at,U width,U n){check(width==4,"shot store width");write(at,n);};
        b.call=[&](U site,const auto& args)->U{
            if(site==0x450630){mission_object_helo_weapon_pose(b,args.at(0),args.at(1),pf,pose_frame.size());if(change){state.tables.groups[3][1].person_slot=2;state.extra_for({3,1}).field_94=7;state.person_extra(2).field_6c=1;}return 0;}
            if(site==0x477620)return query_isqrt_477620(args.at(0));
            if(site==0x477770)return query_angle_477770(si(args.at(0)),si(args.at(1)));
            if(site==0x477860)return behavior_rng_next(state.behavior);
            if(site==0x45ea00){++shot_calls;check(args==std::vector<U>{a,f},"shot dispatch used another owner/frame");MuzzleResult pose{{si(read(f)),si(read(f+4)),si(read(f+8))},{read(f+12),read(f+16),read(f+20)}};
                PlayerShotBindings calls;calls.random=[&]{return behavior_rng_next(state.behavior);};calls.burst_storage=[&]()->U&{return state.person_extra(*state.tables.groups[3][1].person_slot).field_6c;};calls.projectile=[&](U kind,MuzzleResult& current){spawn.request(actor_handle(3,1),kind,current);};
                execute_player_shot(state.extra_for({3,1}).field_94,pose,calls);write(f,U(pose.origin.x));write(f+4,U(pose.origin.y));write(f+8,U(pose.origin.z));write(f+12,pose.aim.yaw);write(f+16,pose.aim.pitch);write(f+20,pose.aim.roll);return 0;}
            if(site==0x45d1e0){const auto p0=args.at(1);const auto result=compose_pose({si(read(p0)),si(read(p0+4)),si(read(p0+8))},{read(p0+12),read(p0+16),read(p0+20)});for(U i=0;i<12;++i)write(args.at(0)+i*4,U(result.m[i]));return 0;}
            if(site==0x45d400){const auto p0=args.at(1);const auto result=model_special_pivot_matrix({si(read(p0)),si(read(p0+4)),si(read(p0+8))},{si(read(p0+12)),si(read(p0+16)),si(read(p0+20))});for(U i=0;i<12;++i)write(args.at(0)+i*4,U(result[i]));return 0;}
            if(site==0x45cb40){Matrix4 left,right;for(U i=0;i<12;++i){left.m[i]=si(read(args.at(0)+i*4));right.m[i]=si(read(args.at(1)+i*4));}const auto result=matrix_multiply(left,right);for(U i=0;i<12;++i)write(args.at(2)+i*4,U(result.m[i]));return 0;}
            if(site==0x45cda0){Matrix4 matrix;for(U i=0;i<12;++i)matrix.m[i]=si(read(args.at(0)+i*4));const auto p0=args.at(1);const auto result=transform_point(matrix,{si(read(p0)),si(read(p0+4)),si(read(p0+8))});write(args.at(2),U(result.x));write(args.at(2)+4,U(result.y));write(args.at(2)+8,U(result.z));return 0;}
            throw MissionTickError("Native helicopter shot required service has no owner");
        };mission_object_helo_shot(b,a,f,frame.size());
    }
};
}
int main(){try{
    {
        Fixture f;BehaviorState expected;expected.rng=f.state.behavior.rng;for(unsigned i=0;i<4;++i)behavior_rng_next(expected);f.run();
        check(f.shot_calls==1&&f.cells.ring_index==1&&f.packet[14]==14&&f.packet[8]==1,"supported group3 helicopter did not use shared request/ring");
        check(f.state.projectiles[1].owner_slot==1&&f.state.behavior.rng==expected.rng&&f.state.extra_for({3,0}).field_94==0,"helicopter shot used local actor or another random state");
    }
    {
        Fixture f;f.change=true;BehaviorState expected;expected.rng=f.state.behavior.rng;behavior_rng_next(expected);behavior_rng_next(expected);f.run();
        check(f.packet[14]==9&&f.state.behavior.rng==expected.rng&&f.state.person_extra(2).field_6c==1&&f.state.person_extra(0).field_6c==26,"shot did not reread current weapon/person at dispatch/decrement");
    }
    {
        Fixture f;MissionObjectHeloBindings b;b.read=[&](U at,U){return f.read(at);};b.write=[&](U at,U,U n){f.write(at,n);};
        bool failed=false;try{mission_object_helo_shot(b,Fixture::a,Fixture::f,f.frame.size());}catch(const MissionTickError&){failed=true;}
        check(failed&&f.state.extra_for({3,1}).field_94==12&&f.cells.ring_index==0,"missing pose service lost the initial weapon write or made a projectile");
    }
    std::cout<<"mission object helicopter shot native checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
