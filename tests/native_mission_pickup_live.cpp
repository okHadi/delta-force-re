#include "dfre/mission_pickup_live.hpp"
#include <stdexcept>
#include <string>

using namespace dfre;
namespace {
void check(bool value){if(!value)throw std::runtime_error("Mission pickup live test failed");}
void init(MissionTickState& state,std::size_t targets=1) {
    AiActorTables tables;tables.groups[3].resize(1);tables.groups[1].resize(targets);tables.groups[0].resize(targets);
    for(auto group:{3u,1u,0u})for(auto& a:tables.groups[group]){a.active=true;a.definition_index=group==3?1:2;a.field_6c=group==3?1:0;}
    tables.groups[3][0].person_slot=0;
    std::vector<AiPersonState> people(1);people[0].owner=actor_handle(3,0);people[0].position[0]=123;people[0].angle_yaw=0x12345678;
    std::vector<AiItemDefinition> definitions(3);definitions[1].field_94=25;definitions[1].field_28=3;definitions[2].id=0xfc3;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),std::move(people),0);
    state.globals.mission_67f528=1;state.config.local_actor=0;state.extra_for({3,0}).field_74=100;
}
ActorCollisionCall request(ActorCollisionSite site,std::initializer_list<std::uint32_t> args) {
    ActorCollisionCall call{site};call.count=std::uint32_t(args.size());unsigned i=0;for(auto n:args)call.arguments[i++]=n;return call;
}
std::uint32_t word(const AiActorImage& a,unsigned at){return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void home_and_void() {
    MissionTickState state;init(state);ActorPickupState shared;
    auto effect=mission_pickup_live_effect(shared,{});const auto handle=actor_handle(3,0);
    check(word(snapshot_mission_actor(state,{3,0}),0xc)==0);
    check(effect(state,request(ActorCollisionSite::can_use,{handle}))==1);
    check(state.tables.groups[3][0].position[0]==123&&state.tables.groups[3][0].angle_yaw==0x12345678);
    check(effect(state,request(ActorCollisionSite::can_use,{handle}))==0);
    check(state.people[0].position[0]==123);
    bool rejected=false;
    try{effect(state,request(ActorCollisionSite::heal,{handle}));}catch(const MissionTickError&){rejected=true;}
    check(rejected&&state.extra_for({3,0}).field_74==25);
    MissionPickupLiveBindings bindings;unsigned sounds=0;
    bindings.sound=[&](auto& live,auto& same,auto id){check(&same==&shared&&id==0x165&&live.extra_for({3,0}).field_74==100);++sounds;live.extra_for({3,0}).field_74=88;return 0xabcdefu;};
    shared.rules_67f549=1;effect=mission_pickup_live_effect(shared,bindings);state.globals.mission_67f528=0;
    check(effect(state,request(ActorCollisionSite::heal,{handle})).has_value());
    check(sounds==1&&state.extra_for({3,0}).field_74==88&&shared.host==0);
    bindings.sound=[](auto&,auto&,auto){return 0xabcdefu;};effect=mission_pickup_live_effect(shared,bindings);
    check(effect(state,request(ActorCollisionSite::sound,{0x166}))==0xabcdefu);
    check(!effect(state,request(ActorCollisionSite(0xdeadbeef),{})));
}
void messages_and_errors() {
    MissionTickState state;init(state);ActorPickupState shared;std::array<ActorPickupClient,17> clients{};shared.clients=&clients;
    const auto actor=actor_handle(3,0),target=actor_handle(1,0);MissionPickupLiveBindings bindings;unsigned messages=0;
    bindings.message=[&](MissionTickState& live,ActorPickupState& same,auto transport,auto id,const auto& payload){
        check(&same==&shared&&transport==ActorPickupTransport::broadcast&&id==0x20&&payload==std::vector<std::uint8_t>({0x14,0,0}));
        check(live.runtime_for({3,0}).link_90==target&&(word(snapshot_mission_actor(live,{1,0}),0x6c)&0x200102)==0x200102);++messages;
        live.tables.groups[3][0].position[0]=333;live.extra_for({1,0}).field_74=77;clients[5]={1,actor,10};throw std::runtime_error("transport stopped");
    };
    auto effect=mission_pickup_live_effect(shared,bindings);bool rejected=false;
    try{effect(state,request(ActorCollisionSite::pickup,{actor,target}));}catch(const std::runtime_error& e){rejected=std::string(e.what())=="transport stopped";}
    check(rejected&&messages==1&&state.runtime_for({3,0}).link_90==target&&state.tables.groups[3][0].position[0]==333&&state.extra_for({1,0}).field_74==77&&clients[5].deliveries_1bc==10);
    unsigned removals=0;bindings.message=[&](auto& live,auto& same,auto,auto,const auto&){check(same.team_one_6e815c==1);live.globals.mission_67f528=0;clients[3]={1,actor,0};};
    bindings.remove=[&](auto&,auto&,const auto&){++removals;};effect=mission_pickup_live_effect(shared,bindings);
    effect(state,request(ActorCollisionSite::deliver_side_one,{actor,target}));
    check(removals==0&&clients[3].deliveries_1bc==1&&shared.host==0&&shared.team_one_6e815c==1);
    state.globals.mission_67f528=1;bindings.message=[&](auto& live,auto&,auto,auto,const auto&){live.config.local_actor.reset();};
    bindings.local_flag=[](auto&,auto&){throw std::runtime_error("Stale local flag callback");};effect=mission_pickup_live_effect(shared,bindings);
    effect(state,request(ActorCollisionSite::pickup,{actor,target}));check(shared.local_player==0);
}
void clients_and_pose() {
    MissionTickState state;init(state,40);ActorPickupState shared;std::array<ActorPickupClient,17> clients{};shared.clients=&clients;
    const auto actor=actor_handle(3,0),target=actor_handle(1,37);clients[0]={1,actor,0};std::vector<unsigned> order;
    MissionPickupLiveBindings bindings;
    bindings.client_message=[&](auto&,auto& same,auto client,auto id,auto a,auto b){check(&same==&shared&&id==0x1d&&a==0&&b==0);order.push_back(unsigned(client));if(client==0)clients[1]={1,actor,0};};
    bindings.resupply=[&](auto&,auto&,auto client){order.push_back(unsigned(client)+100);++clients[client].deliveries_1bc;};
    auto effect=mission_pickup_live_effect(shared,bindings);effect(state,request(ActorCollisionSite::refill,{actor}));
    check(order==std::vector<unsigned>({0,100,1,101})&&clients[0].deliveries_1bc==1&&clients[1].deliveries_1bc==1);
    auto& t=state.tables.groups[1][37];t.position[0]=1;t.position[1]=2;t.position[2]=3;t.angle_yaw=0x12345678;
    unsigned logic=0;
    bindings.message=[&](auto& live,auto&,auto transport,auto id,const auto& payload){
        check(transport==ActorPickupTransport::reliable&&id==0x27&&payload.size()==18&&payload[2]==1&&payload[14]==0x34&&payload[15]==0x12&&payload[17]==0xef);
        live.tables.groups[1][37].position[0]=99;live.tables.groups[1][37].angle_yaw=0xffff1234;
    };
    bindings.objective=[&](auto& live,auto&,const auto& source,const auto& pose,auto mode){
        check(source.group==3&&source.index==0&&pose.group==1&&pose.index==37&&mode==0xabcdef);
        const auto image=snapshot_mission_actor(live,pose);check(word(image,0x24)==99&&word(image,0x30)==0xffff1234);++logic;
    };
    effect=mission_pickup_live_effect(shared,bindings);effect(state,request(ActorCollisionSite::objective,{actor,target+0x24,0xabcdef}));
    check(logic==1&&state.tables.groups[1][37].position[0]==99&&state.tables.groups[1][37].angle_yaw==0xffff1234);
    unsigned packets=0;
    bindings.allocate=[&](auto&,auto&,auto owner,auto position)->std::optional<AiActorRef>{check(owner==target&&position.x==7);return AiActorRef{0,37};};
    bindings.message=[&](auto&,auto&,auto transport,auto id,const auto& payload){check(transport==ActorPickupTransport::reliable&&id==0x2b&&payload[0]==37&&payload[40]==37&&payload[41]==1);++packets;};
    effect=mission_pickup_live_effect(shared,bindings);effect(state,request(ActorCollisionSite::objective_effect,{target,7,8,9}));check(packets==1);
    bindings.allocate=[](auto&,auto&,auto,auto)->std::optional<AiActorRef>{return AiActorRef{3,0};};effect=mission_pickup_live_effect(shared,bindings);bool rejected=false;
    try{effect(state,request(ActorCollisionSite::objective_effect,{target,7,8,9}));}catch(const MissionTickError&){rejected=true;}check(rejected&&packets==1);
}
void removal_and_validation() {
    MissionTickState state;init(state);ActorPickupState shared;const auto actor=actor_handle(3,0);
    auto effect=mission_pickup_live_effect(shared,{});bool rejected=false;
    try{effect(state,request(ActorCollisionSite::remove,{actor}));}catch(const MissionTickError&){rejected=true;}
    check(rejected&&state.tables.groups[3][0].active&&state.people[0].owner==actor);
    MissionPickupLiveBindings bindings;unsigned cleaned=0;
    bindings.remove=[&](auto& live,auto&,const auto& ref){
        check(ref.group==3&&ref.index==0);live.people[0].owner=0;
        apply_mission_actor_image(live,ref,{});++cleaned;
    };
    effect=mission_pickup_live_effect(shared,bindings);effect(state,request(ActorCollisionSite::remove,{actor}));
    check(cleaned==1&&state.people[0].owner==0&&!state.tables.groups[3][0].active&&snapshot_mission_actor(state,{3,0})==AiActorImage{});
    for(const auto& call:{request(ActorCollisionSite::heal,{}),request(ActorCollisionSite::heal,{actor_handle(4,0)}),request(ActorCollisionSite::heal,{actor_handle(0,0)})}) {
        rejected=false;try{effect(state,call);}catch(const MissionTickError&){rejected=true;}check(rejected);
    }
}

}
int main(){try{home_and_void();messages_and_errors();clients_and_pose();removal_and_validation();}catch(...){return 1;}return 0;}
