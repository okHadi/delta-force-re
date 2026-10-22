#include "dfre/mission_muzzle.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
void put(dfre::AiActorImage& a,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(v>>(i*8));}
template<class F> void rejects(F f){bool bad=false;try{f();}catch(const std::invalid_argument&){bad=true;}check(bad,"missing muzzle state was accepted");}
}
int main(){try{
    dfre::Model primary,secondary,avatar;
    for(auto* m:{&primary,&secondary,&avatar}){m->render_types[0]=0x676e7263;m->lods.resize(1);}
    primary.lods[0].hardpoints.resize(8,{256,512,768});secondary.lods[0].hardpoints.resize(8,{1024,1280,1536});
    avatar.lods[0].meshes.resize(8);
    dfre::MissionMuzzleResources r;r.definitions.resize(3);
    const auto& weapon=dfre::weapon_definitions()[7];
    check(weapon.primary_item!=weapon.secondary_item,"test needs distinct weapon item ids");
    r.definitions[1].id=weapon.primary_item;r.definitions[1].model=1;
    r.definitions[2].id=weapon.secondary_item;r.definitions[2].model=2;
    r.models={{1,&primary},{2,&secondary},{3,&avatar}};
    dfre::MissionAnimations animation(std::vector<dfre::MissionAnimationBinding>{});
    dfre::MissionMuzzle muzzle(r,animation);
    check(muzzle.weapon(7).primary_definition==1 && muzzle.weapon(7).secondary_definition==2,"runtime item lookup changed");
    dfre::AiActorImage actor{};put(actor,0x10,3);put(actor,0x94,7);put(actor,0x24,65536);put(actor,0x34,0x20000000);
    auto normal=muzzle.normal(actor,7,{});
    dfre::MuzzleInputs inputs;inputs.actor_position.x=65536;inputs.actor_angles.pitch=0x20000000;
    inputs.weapon_muzzle={weapon.muzzle_x,weapon.muzzle_y,weapon.muzzle_z};
    inputs.weapon_aim={std::uint32_t(weapon.aim_x),std::uint32_t(weapon.aim_y),std::uint32_t(weapon.aim_z)};
    const auto local=dfre::compute_muzzle(inputs);
    const auto wanted=dfre::compute_barrel_tip(local.origin,local.aim,{256,512,768});
    check(normal.origin.x==wanted.x && normal.origin.y==wanted.y && normal.origin.z==wanted.z,"normal muzzle did not use primary model");
    unsigned calls=0;
    check(muzzle.animated({&actor,42},[&](auto owner,const auto&){++calls;check(owner.identity==42 && actor[0x37]==0x20,"shot ran before pitch restore");(void)muzzle.slot_seven();}),"animated shot was skipped");
    check(calls==1,"shot callback count changed");
    put(actor,0x10,0);
    check(!muzzle.animated({&actor,42},[](auto,const auto&){throw std::runtime_error("missing graphic fired");}),"null graphic guard changed");
    put(actor,0x10,3);
    primary.lods[0].hardpoints.clear();
    rejects([&]{muzzle.normal(actor,7,{});});
    primary.lods[0].hardpoints.resize(8,{256,512,768});
    avatar.lods[0].meshes.resize(1);
    dfre::MissionMuzzle short_table(r,animation);
    rejects([&]{short_table.animated({&actor,42},[](auto,const auto&){});});
    auto seed=dfre::matrix_identity();seed.m[3]=123456;short_table.set_slot_seven(seed);
    check(short_table.animated({&actor,42},[](auto,const auto&){}),"short callback did not retain seeded slot");
    const auto expected_slot=dfre::matrix_multiply(dfre::matrix_multiply(
        dfre::compose_pose({65536,0,0},{}),seed),short_table.attachment());
    check(short_table.slot_seven().m==expected_slot.m,"short callback replaced prior slot seven");
    avatar.render_types[0]=0x74726565;
    rejects([&]{muzzle.animated({&actor,42},[](auto,const auto&){});});
    auto shared=dfre::matrix_identity();muzzle.set_slot_source([&](auto,const auto&)->dfre::Matrix4&{return shared;});
    muzzle.animated({&actor,42},[](auto,const auto&){});
    check(shared.m!=dfre::matrix_identity().m,"animated muzzle did not write shared slot");
    put(actor,0x94,0);check(!muzzle.animated({&actor,42},[](auto,const auto&){throw std::runtime_error("guard fired shot");}),"zero weapon guard changed");
    rejects([&]{muzzle.normal(actor,39,{});});
    std::cout<<"mission muzzle native checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
