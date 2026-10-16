#include "dfre/actor_queries.hpp"
#include <stdexcept>
#define CHECK(condition) do { if(!(condition)) throw std::runtime_error("actor query check: " #condition); } while(false)
#include <cstdint>
#include <iostream>
#include <limits>
using namespace dfre;
int main(){
    std::vector<AiActorState> actors(3);std::vector<AiPersonState> people(3);
    std::vector<BehaviorActorExtra> extras(3);std::vector<BehaviorPersonExtra> px(3);
    for(std::size_t i=0;i<3;++i){actors[i].active=true;actors[i].definition_index=std::uint32_t(i+1);actors[i].person_slot=i;actors[i].field_70=1;actors[i].field_72=std::uint16_t(i);}
    ActorQueryTable table{&actors,&people,&extras,&px};ActorQueryData data;BehaviorState state;
    data.raw_pointer=[](std::size_t i){return std::uint32_t(0x7cca80+i*0x114);};
    CHECK(query_angle_477770(0,0)==0);
    CHECK(query_angle_477770(1,0)==0);
    CHECK(query_angle_477770(0,1024)==0x40000000);
    CHECK(query_angle_477770(-1,0)==0x80000000);
    CHECK(query_angle_477770(0,-1024)==0xc0000000);
    CHECK(query_angle_477770(471133212,550039535)==589585457);
#if defined(__i386__) || defined(__x86_64__)
    std::uint16_t previous;asm volatile("fnstcw %0":"=m"(previous));
    const std::uint16_t changed=0x067f;asm volatile("fldcw %0"::"m"(changed));
    CHECK(query_angle_477770(471133212,550039535)==589585457);
    std::uint16_t after;asm volatile("fnstcw %0":"=m"(after));CHECK(after==changed);asm volatile("fldcw %0"::"m"(previous));
#endif
    CHECK(query_isqrt_477620(2)==2); // Original final rounding condition.
    CHECK(query_isqrt_477620(0xffffffff)==0xffffffff);
    CHECK(query_nearby_45f010(table,0)==0);
    actors[1].position[0]=0x30000;actors[2].position[0]=0x30000;
    CHECK(query_nearby_45f010(table,0)==1);
    people[0].field_9c=2;px[0].field_44=data.raw_pointer(2);px[0].field_30=100;px[0].field_48=42;
    people[1].field_94=101;actors[1].position[0]=0x10000;actors[2].field_6c=4;
    bool mark=false;state.on_effect=[&](const BehaviorAction& action){CHECK(action.site==behavior_site::kTypeSlotMark);CHECK(people[1].field_9c==2);CHECK(px[1].field_44==0);mark=true;px[0].field_48=99;};
    CHECK(query_selector_45f090(table,0,state)==1);CHECK(mark);CHECK(px[1].field_48==99);CHECK(px[1].field_44==px[0].field_44);
    state.on_effect={};state.actions.clear();
    data.zones={{0,0,-1,0x20000,-1,1,-1,1}};
    actors[0].angle_yaw=0x1234;query_zone_45fd90(table,0,data,state);CHECK(people[0].field_9c==2);CHECK(actors[0].angle_yaw==0);CHECK(state.actions.size()==1);
    CHECK(query_anim_id_460410(actors[0],2,3,1,0)==0xc9);
    CHECK(query_anim_id_460410(actors[0],2,3,2,0)==0xfd);
    CHECK(query_anim_id_460410(actors[0],2,3,3,0)==0xff);
    CHECK(query_anim_id_460410(actors[0],2,3,9,1)==0xfe);
    CHECK(query_anim_id_460410(actors[0],2,3,9,0,123)==123);
    bool missing=false;try{query_anim_id_460410(actors[0],0,0,0,0);}catch(const ActorQueryError&){missing=true;}CHECK(missing);
    // All 128 fallback samples fail. The scan returns the stored yaw.
    std::size_t samples=0;data.terrain_height=[&](auto,auto){++samples;return 0x100000;};
    data.sin_cos_477820=[](auto,auto& s,auto& c){s=0;c=4194304;};
    actors[0].angle_yaw=0x76543210;people[0].field_7c=0;people[0].flags24=4;
    CHECK(query_facing_45edd0(table,0,data,state)==0x76543210);CHECK(samples==8*2+128);
    // The shot sink sees temporary aim. Its writes are applied before return.
    px[0].field_44=data.raw_pointer(1);px[0].field_6c=1;px[0].field_48=0x10000;px[0].field_4c=0x20000;
    people[0].flags24=0;const auto old_yaw=actors[0].angle_yaw,old_pitch=actors[0].angle_pitch;state.rng=123;
    bool shot=false;state.on_effect=[&](const BehaviorAction& action){CHECK(action.site==0x45e7a0);shot=true;CHECK(actors[0].angle_yaw!=old_yaw || actors[0].angle_pitch!=old_pitch);px[0].field_6c=7;};
    query_combat_step_45f2c0(table,0,data,state);CHECK(shot);CHECK(px[0].field_6c==7);CHECK(actors[0].angle_yaw==old_yaw && actors[0].angle_pitch==old_pitch);
    ActorQueryProvider provider(table,data);BehaviorActor view{{3,0},data.raw_pointer(0),actors[0],people[0],extras[0],px[0]};
    BehaviorAction request{BehaviorCallKind::query,behavior_site::kCombatResult,{}};
    CHECK(provider.answer({3,0},request.site,request,view,state)->value==7);
    CHECK(!provider.answer({0,0},request.site,request,view,state));
    CHECK(!provider.perform_effect({3,0},{BehaviorCallKind::effect,0x1234,{}},state));
    std::cout<<"actor query checks passed\n";
}
