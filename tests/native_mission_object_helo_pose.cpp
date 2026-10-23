#include "dfre/mission_object_helo_pose.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/model_specials.hpp"
#include "dfre/projectiles.hpp"
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
    static constexpr U a=0x7dde80,g=0x1301000,l=0x1302000,o=0x1303000,f=0x1304000,hp=0x1306000;
    MissionTickState state;Model model;std::array<std::uint8_t,0x18> output{};std::array<std::uint8_t,0x80> frame{};
    bool mutate=false;
    Fixture(){AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];actor.active=true;actor.definition_index=1;actor.field_10=17;
        actor.position[0]=100000;actor.position[1]=200000;actor.position[2]=300000;
        init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},123);
        model.lods.resize(1);model.lods[0].hardpoints.resize(9);model.lods[0].hardpoints[2]={1,2,3};model.lods[0].hardpoints[4]={10,20,30};model.lods[0].hardpoints[8]={40,50,60};frame.fill(0xa5);output.fill(0xcc);
    }
    U read(U at)const{
        if(at>=a&&std::uint64_t(at-a)+4<=0x114){const auto image=snapshot_mission_actor(state,{0,0});if(at==a+0x10){check(get(image,0x10)==17,"pose graphic codec lost its current model");return g;}return get(image,at-a);}
        if(at==g+0x50)return l;if(at==l+0xa4)return hp;
        if(at>=hp&&std::uint64_t(at-hp)+4<=model.lods[0].hardpoints.size()*12){const auto off=at-hp;check(!(off&3),"unaligned hardpoint");return U(model.lods[0].hardpoints.at(off/12).at(off%12/4));}
        if(at>=o&&std::uint64_t(at-o)+4<=output.size())return get(output,at-o);
        if(at>=f&&std::uint64_t(at-f)+4<=frame.size())return get(frame,at-f);
        throw MissionTickError("Native helicopter pose read has no owner");
    }
    void write(U at,U n){
        if(at>=hp&&std::uint64_t(at-hp)+4<=model.lods[0].hardpoints.size()*12){const auto off=at-hp;check(!(off&3),"unaligned hardpoint store");model.lods[0].hardpoints.at(off/12).at(off%12/4)=si(n);return;}
        if(at>=o&&std::uint64_t(at-o)+4<=output.size()){put(output,at-o,n);return;}
        if(at>=f&&std::uint64_t(at-f)+4<=frame.size()){put(frame,at-f,n);return;}
        throw MissionTickError("Native helicopter pose store has no owner");
    }
    MissionObjectHeloBindings bindings(){MissionObjectHeloBindings b;b.read=[&](U at,U width){check(width==4,"pose read width");return read(at);};b.write=[&](U at,U width,U n){check(width==4,"pose store width");write(at,n);};
        b.call=[&](U site,const auto& args)->U{
            if(site==0x45d1e0){const auto p=args.at(1);const auto result=compose_pose({si(read(p)),si(read(p+4)),si(read(p+8))},{read(p+12),read(p+16),read(p+20)});for(U i=0;i<12;++i)write(args.at(0)+i*4,U(result.m[i]));
                if(mutate){state.tables.groups[0][0].position[0]=999;auto image=snapshot_mission_actor(state,{0,0});put(image,0xc4,0x40000000);apply_mission_actor_image(state,{0,0},image);}return 0;}
            if(site==0x45d400){const auto p=args.at(1);const auto result=model_special_pivot_matrix({si(read(p)),si(read(p+4)),si(read(p+8))},{si(read(p+12)),si(read(p+16)),si(read(p+20))});for(U i=0;i<12;++i)write(args.at(0)+i*4,U(result[i]));return 0;}
            if(site==0x45cb40){Matrix4 left,right;for(U i=0;i<12;++i){left.m[i]=si(read(args.at(0)+i*4));right.m[i]=si(read(args.at(1)+i*4));}const auto result=matrix_multiply(left,right);for(U i=0;i<12;++i)write(args.at(2)+i*4,U(result.m[i]));return 0;}
            if(site==0x45cda0){Matrix4 matrix;for(U i=0;i<12;++i)matrix.m[i]=si(read(args.at(0)+i*4));const auto p=args.at(1);const auto result=transform_point(matrix,{si(read(p)),si(read(p+4)),si(read(p+8))});write(args.at(2),U(result.x));write(args.at(2)+4,U(result.y));write(args.at(2)+8,U(result.z));return 0;}
            throw MissionTickError("Native helicopter pose matrix body is not bound");
        };return b;
    }
};
}
int main(){try{
    {
        Fixture f;mission_object_helo_smoke_pose(f.bindings(),Fixture::a,Fixture::o,Fixture::f,f.frame.size());
        check(get(f.output,0)==100001&&get(f.output,4)==200002&&get(f.output,8)==300003,"smoke pose lost its current model hardpoint");
        mission_object_helo_weapon_pose(f.bindings(),Fixture::a,Fixture::o,Fixture::f,f.frame.size());
        check(get(f.output,0)==100040&&get(f.output,4)==200050&&get(f.output,8)==300060,"weapon pose lost its current model hardpoint");
        check(f.state.tables.groups[0].size()==1&&f.state.people.empty()&&f.model.lods[0].hardpoints.size()==9,"pose helper created another canonical owner");
    }
    {
        Fixture f;f.mutate=true;mission_object_helo_weapon_pose(f.bindings(),Fixture::a,Fixture::o,Fixture::f,f.frame.size());
        check(get(f.output,12)==0x40000000&&get(f.frame,0x4c)==100000&&f.state.tables.groups[0][0].position[0]==999,"pose callback lost current aim or the captured world matrix");
    }
    {
        Fixture f;f.model.lods[0].hardpoints[4]={INT32_MIN,INT32_MIN,INT32_MIN};
        mission_object_helo_weapon_pose(f.bindings(),Fixture::a,Fixture::hp+0x30,Fixture::f,f.frame.size());
        check(f.model.lods[0].hardpoints[4]==std::array<std::int32_t,3>{100040,200050,300060},"INT_MIN pivot or aliased model output changed");
        for(const auto& pivot:{std::array<std::int32_t,3>{INT32_MIN,0,0},{0,INT32_MIN,0},{0,0,INT32_MIN},{INT32_MIN,INT32_MAX,-1}})
            check(model_special_pivot_matrix(pivot,{0,0,0})==matrix_identity().m,"signed pivot negation does not wrap like sourceNEG");
    }
    {
        Fixture f;auto bindings=f.bindings();bindings.call={};bool failed=false;
        try{mission_object_helo_smoke_pose(bindings,Fixture::a,Fixture::o,Fixture::f,f.frame.size());}catch(const MissionTickError&){failed=true;}
        check(failed&&get(f.output,0)==0xcccccccc&&get(f.frame,0)==0xa5a5a5a5,"missing matrix body published a fake pose");
    }
    std::cout<<"mission object helicopter pose native checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
