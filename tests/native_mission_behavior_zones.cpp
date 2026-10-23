#include "dfre/mission_behavior_zones.hpp"
#include <map>
#include <vector>
#include <iostream>
namespace {
void check(bool value){if(!value)throw std::runtime_error("Behavior zone check failed");}
}
int main(){try{
    using U=std::uint32_t;std::map<U,U> memory;std::vector<U> writes;
    dfre::MissionBehaviorZoneBindings b;
    b.read=[&](U at,U width){check(width==4);return memory[at];};
    b.write=[&](U at,U width,U n){check(width==4);memory[at]=n;writes.push_back(at);};
    check(!dfre::mission_behavior_zone_add(b,1,2,3,4,5,6)&&writes.empty());
    memory[0x67f528]=1;memory[0x4f8d44]=199;
    check(dfre::mission_behavior_zone_add(b,1,2,3,4,5,6));
    const U row=0x8e48b0+199*32;
    check(memory[row]==6&&memory[row+4]==5&&memory[row+8]==0xfffffffdu&&memory[row+28]==7&&memory[0x4f8d44]==200);
    check(writes[7]==0x4f8d44&&writes[8]==row+28);
    const auto old=writes.size();check(!dfre::mission_behavior_zone_add(b,0,0,0,0,0,0)&&writes.size()==old);
    dfre::mission_behavior_zones_reset(b);check(memory[0x4f8d44]==0&&memory[row]==6);
    memory[0x4f8d44]=0xffffffff;check(dfre::mission_behavior_zone_add(b,0,0,0,1,2,3));
    check(memory[0x8e4890]==3&&memory[0x4f8d44]==0);
    memory[0x4f8d44]=0;b.write=[&](U at,U,U n){memory[at]=n;if(at==0x4f8d44)throw std::runtime_error("fixture store fault");};
    bool failed=false;try{dfre::mission_behavior_zone_add(b,1,2,3,4,5,6);}catch(const std::runtime_error&){failed=true;}
    check(failed&&memory[0x4f8d44]==1&&memory[0x8e48c8]==0xffffffffu);
    dfre::MissionBehaviorZones owner;U host=1;auto bound=owner.bindings(host);
    check(dfre::mission_behavior_zone_add(bound,10,20,30,5,7,2));
    const auto view=owner.active_records();check(view.size()==1&&view[0].flags==2&&view[0].type==7&&view[0].x_min==5&&view[0].z_max==35);
    std::vector<dfre::AiActorState> actors(1);std::vector<dfre::AiPersonState> people(1);
    actors[0].person_slot=0;actors[0].position[0]=10;actors[0].position[1]=20;actors[0].position[2]=30;actors[0].field_72=7;
    dfre::ActorQueryTable table{&actors,&people,nullptr,nullptr};dfre::ActorQueryData data;
    data.zones=owner.active_records();dfre::BehaviorState behavior;
    dfre::query_zone_45fd90(table,0,data,behavior);
    check(people[0].field_9c==2&&behavior.actions.size()==1);
    host=0;check(!dfre::mission_behavior_zone_add(bound,0,0,0,0,0,0)&&owner.count==1);
    dfre::mission_behavior_zones_reset(bound);check(owner.active_records().empty()&&owner.records[0][7]==35);
    people[0].field_9c=0;data.zones=owner.active_records();dfre::query_zone_45fd90(table,0,data,behavior);
    check(people[0].field_9c==0&&behavior.actions.size()==1);
    owner.count=201;failed=false;try{owner.active_records();}catch(const std::runtime_error&){failed=true;}check(failed);
    std::cout<<"behavior zone checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
