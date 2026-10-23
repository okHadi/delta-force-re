#include "dfre/mission_person_allocate.hpp"
#include "dfre/mission_tick.hpp"
#include <iostream>
using namespace dfre;using U=std::uint32_t;
namespace {void check(bool n){if(!n)throw MissionActorCreateError("Person allocation check failed");}}
int main(){try{
    MissionTickState state;state.people.resize(1280);state.person_extras.resize(1280);
    for(auto& p:state.people)p.owner=0x7cca80;state.people[17].owner=0;state.people[17].name="old";state.people[17].flags24=255;state.person_extras[17].field_6c=99;
    MissionActorCreateBindings b;unsigned clears=0;bool callback_fault=false;
    const auto slot=[&](U at){check(at>=0x794680&&(at-0x794680)%0xb4==0);const auto index=(at-0x794680)/0xb4;check(index<state.people.size());return index;};
    b.read=[&](U at,U width){check(width==4);return state.people[slot(at)].owner;};
    b.write=[&](U at,U width,U owner){check(width==4);state.people[slot(at)].owner=owner;};
    b.call=[&](U site,const std::vector<U>& args)->U{check(site==0x49a620&&args.size()==3&&args[1]==0&&args[2]==0xb4);const auto index=slot(args[0]);state.people[index]={};state.person_extras[index]={};++clears;state.people[index].owner=0xfeedbeef;if(callback_fault)throw MissionActorCreateError("Clear provider fault");return args[0];};
    const auto person=mission_person_allocate(b,0x7ccb94);check(person==0x794680+17*0xb4&&clears==1&&state.people[17].owner==0x7ccb94&&state.people[17].name.empty()&&state.people[17].flags24==0&&state.person_extras[17].field_6c==0);
    check(mission_person_allocate(b,0x7cca80)==0&&clears==1);
    state.people[1279].owner=0;check(mission_person_allocate(b,0x7cca80)==0x794680+1279*0xb4);
    state.people[0].owner=0;check(mission_person_allocate(b,0)==0x794680&&state.people[0].owner==0);check(mission_person_allocate(b,0x7cca80)==0x794680&&state.people[0].owner==0x7cca80);
    state.people[0].owner=0;callback_fault=true;bool failed=false;try{mission_person_allocate(b,0x7cca80);}catch(const MissionActorCreateError&){failed=true;}check(failed&&state.people[0].owner==0xfeedbeef);
    callback_fault=false;b.write={};failed=false;try{state.people[0].owner=0;mission_person_allocate(b,0x7cca80);}catch(const MissionActorCreateError&){failed=true;}check(failed&&state.people[0].owner==0xfeedbeef);
    std::cout<<"Canonical person allocation, clear order and fault prefixes passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
