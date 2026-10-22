#include "dfre/mission_projectile_stage.hpp"
#include <stdexcept>

using namespace dfre;
namespace {
void check(bool good) {if(!good)throw std::runtime_error("Mission projectile stage test failed");}
void put(AiActorImage& a,unsigned at,std::uint32_t n) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(8*i));}
std::uint32_t get(const AiActorImage& a,unsigned at) {return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void listener() {
    MissionProjectileStageState s;MissionProjectileStageBindings b;
    mission_listener_stage(s,b);
    AiActorImage local{};put(local,0x24,123);put(local,0x30,456);s.local_player={&local,10};unsigned calls=0;
    b.listener=[&](const auto& pose){check(pose.position.x==123&&pose.yaw==456);++calls;};
    mission_listener_stage(s,b);check(calls==1);
    s.alternate_listener=1;s.alternate_pose={{123,0,0},456,0};s.local_player={};
    mission_listener_stage(s,b);check(calls==2);
}
void helpers() {
    std::vector<AiActorImage> actors(1);auto& a=actors[0];put(a,0,1);put(a,4,2);put(a,0x110,3);put(a,0x74,100);
    std::vector<MissionPendingSpawn> pending{{{65536,0,0},3,77}};
    MissionProjectileStageState s;s.pending=&pending;s.local_player_identity=10;BehaviorState random;s.random=&random;
    MissionProjectileStageBindings b;b.actor=[&](auto){return MissionAreaActor{&actors[0],10};};
    b.definition=[](auto){return MissionAreaDefinition{3,0};};unsigned queries=0,updates=0,hits=0;
    b.segment=[&](auto,const auto& start,const auto& end){check(start.z==0&&end.z==0x8000);++queries;random.rng=0x12345678;return 1;};
    b.update=[&](auto,auto callback,auto message){check(callback==3&&message==2&&get(actors[0],0xf8)==77&&actors[0][0x74]==98);++updates;pending.reserve(128);};
    b.local_hit=[&](){check(updates==1);++hits;};
    random.rng=0x12345678;const auto expected=behavior_rng_next(random);random.rng=0;
    mission_area_apply(MissionAreaSite::blast,{3,0},0,0x40000,s,b);
    check(queries==1&&updates==1&&hits==1&&random.rng!=0);
    check(get(actors[0],0x84)==(expected<0x4000?0xffu:0xfau));
    put(actors[0],0x74,100);b.update=[&](auto,auto,auto message){check(message==3&&get(actors[0],0x6c)==0x10400&&actors[0][0x74]==92);++updates;};
    mission_area_apply(MissionAreaSite::shock,{3,0},0,0,s,b);check(updates==2);
}
void queue() {
    std::array<std::vector<AiActorImage>,4> actors;actors[3].resize(1);put(actors[3][0],0,1);put(actors[3][0],4,2);put(actors[3][0],0x110,3);put(actors[3][0],0x74,100);
    std::vector<MissionPendingSpawn> pending{{{0,0,0},2,0}};
    MissionProjectileStageState s;s.pending=&pending;s.host=1;
    MissionProjectileStageBindings b;b.count=[&](auto group){return actors[group].size();};
    b.actor=[&](auto ref){return MissionAreaActor{&actors[ref.group][ref.index],10};};
    b.definition=[](auto){return MissionAreaDefinition{3,0};};unsigned calls=0;
    b.update=[&](auto,auto,auto message){++calls;if(calls==1){check(message==3);pending.push_back({{0,0,0},4,0});}else check(message==5);};
    mission_pending_area_stage(s,b);check(pending.empty()&&calls==2&&actors[3][0][0x74]==91);
    pending.push_back({{0,0,0},-1,0});s.incoming_dispatch=MissionAreaDispatch{0,MissionAreaSite::blast};
    mission_pending_area_stage(s,{});check(pending.empty());
    pending.push_back({{0,0,0},1,0});s.host=0;mission_pending_area_stage(s,{});check(pending.size()==1);
}
}
int main(){try{listener();helpers();queue();}catch(...){return 1;}return 0;}
