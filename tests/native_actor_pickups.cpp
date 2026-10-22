#include "dfre/actor_pickups.hpp"
#include <stdexcept>

using namespace dfre;
namespace {
void check(bool value){if(!value)throw std::runtime_error("Actor pickup test failed");}
void put(AiActorImage& a,unsigned at,std::uint32_t n){for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(8*i));}
std::uint32_t get(const AiActorImage& a,unsigned at){return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void refill_heal_home() {
    AiActorImage a{};a[0x6c]=1;put(a,4,1);std::array<ActorPickupClient,17> clients{};
    clients[0]={1,10,0};clients[5]={1,10,0};ActorPickupState state;state.clients=&clients;
    ActorPickupActor actor{&a,10,0x7cca80,nullptr};ActorPickupBindings b;std::vector<unsigned> calls;
    b.client_message=[&](auto client,auto message,auto p1,auto p2){check(message==0x1d&&p1==0&&p2==0);calls.push_back(unsigned(client));if(client==0)clients[1]={1,10,0};};
    b.resupply=[&](auto client){calls.push_back(unsigned(client)+100);};
    actor_pickup_refill(actor,state,b);check(calls==std::vector<unsigned>({0,100,1,101,5,105}));
    state.rules_67f549=1;b.definition=[](auto){return ActorPickupDefinition{0,0xffff};};
    b.sound=[&](auto sound){check(sound==0x165&&(get(a,0x74)&0xffff)==0xfffc);a[0x75]=0x12;};
    actor_pickup_heal(actor,state,b);check((get(a,0x74)&0xffff)==0x12fc);
    AiPersonState person;person.position[0]=123;person.position[1]=456;person.position[2]=789;person.angle_yaw=0x12345678;
    actor.person=&person;check(actor_pickup_restore_home(actor)==1&&get(a,0x24)==123&&get(a,0x30)==0x12345678);
    check((get(a,0x6c)&0x200100)==0x200100&&actor_pickup_restore_home(actor)==0&&actor_pickup_restore_home({})==0);
}
void pickup_delivery() {
    AiActorImage a{},t{};put(t,4,1);ActorPickupActor actor{&a,10,0x7cca80+257*0x114,nullptr},target{&t,20,0x7cca80+5*0x114,nullptr};
    ActorPickupState state;state.host=1;std::array<ActorPickupClient,17> clients{};state.clients=&clients;
    ActorPickupBindings b;b.definition=[](auto){return ActorPickupDefinition{0xfc3,0};};unsigned local=0;
    b.message=[&](auto transport,auto id,const auto& payload){check(transport==ActorPickupTransport::broadcast&&id==0x20&&payload==std::vector<std::uint8_t>({0x14,1,0}));state.local_player=10;};
    b.local_flag=[&]{++local;};actor_pickup_take(actor,target,state,b);
    check(get(a,0x90)==20&&(get(t,0x6c)&0x200102)==0x200102&&local==1);
    std::vector<unsigned> order;state.team_one_6e815c=0xffffffff;
    b.message=[&](auto transport,auto id,const auto& payload){
        if(id==0x20){check(state.team_one_6e815c==0&&transport==ActorPickupTransport::broadcast&&payload[0]==0x13);order.push_back(1);}
        else{check(transport==ActorPickupTransport::reliable&&id==0x10&&payload==std::vector<std::uint8_t>({5,0}));order.push_back(3);}
    };
    b.remove=[&](auto value){check(value.identity==20);order.push_back(2);clients[3]={1,10,0};*value.image={};};
    actor_pickup_deliver(actor,target,false,state,b);check(order==std::vector<unsigned>({1,2,3})&&clients[3].deliveries_1bc==1);
    state.host=1;b.message=[&](auto,auto,const auto&){state.host=0;};b.remove=[](auto){throw std::runtime_error("Host-off removal reached");};
    actor_pickup_deliver(actor,target,true,state,b);check(state.team_two_6e81c4==1&&clients[3].deliveries_1bc==2);
    state.host=0;put(t,4,1);put(a,0x90,0);ActorPickupBindings definitions_only;definitions_only.definition=b.definition;
    actor_pickup_take(actor,target,state,definitions_only);check(get(a,0x90)==20);
}
void objective() {
    AiActorImage source{},spawn{};put(source,0x24,1);put(source,0x28,2);put(source,0x2c,3);put(source,0x30,0x12345678);
    ActorPickupState state;state.host=1;ActorPickupBindings b;unsigned logic=0;
    const ActorPickupActor actor{nullptr,10,0x7cca80+2*0x114,nullptr};
    b.message=[&](auto transport,auto id,const auto& payload){
        check(transport==ActorPickupTransport::reliable&&id==0x27&&payload.size()==18&&payload[0]==2&&payload[2]==1&&payload[14]==0x34&&payload[15]==0x12&&payload[16]==0&&payload[17]==0xef);
        put(source,0x24,99);
    };
    b.objective=[&](auto,const auto& pose,auto mode){check(pose.bytes==source.data()+0x24&&get(source,0x24)==99&&mode==0xabcdef);++logic;};
    actor_pickup_objective(actor,{source.data()+0x24,0},0xabcdef,state,b);check(logic==1);
    state.host=0;actor_pickup_objective({}, {},0,state,{});
    put(spawn,4,1);put(spawn,0x6c,0x12345678);spawn[0x70]=2;unsigned allocations=0,messages=0;
    b.definition=[](auto){return ActorPickupDefinition{0x56781234,0};};
    b.allocate=[&](auto owner,auto position)->std::optional<ActorPickupActor>{check(owner==10&&position.x==1);++allocations;return ActorPickupActor{&spawn,20,0x7dde80+3*0x114,nullptr};};
    b.message=[&](auto transport,auto id,const auto& payload){check(transport==ActorPickupTransport::reliable&&id==0x2b&&payload.size()==43&&payload[0]==3&&payload[2]==0x34&&payload[3]==0x12&&payload[40]==3&&payload[41]==1&&payload[42]==2);++messages;};
    actor_pickup_objective_spawn(10,{1,2,3},b);check(allocations==1&&messages==1);
    b.allocate=[](auto,auto)->std::optional<ActorPickupActor>{return std::nullopt;};actor_pickup_objective_spawn(0,{},b);check(messages==1);
}
}
int main(){try{refill_heal_home();pickup_delivery();objective();}catch(...){return 1;}return 0;}
