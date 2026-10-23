#include "dfre/actor_pickups.hpp"
#include <stdexcept>

namespace dfre {
namespace {
std::uint32_t read(const std::uint8_t* p) {return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
std::uint32_t word(const AiActorImage& a,unsigned at) {return read(a.data()+at);}
void put(AiActorImage& a,unsigned at,std::uint32_t n) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(8*i));}
void store(std::vector<std::uint8_t>& bytes,unsigned at,std::uint32_t n,unsigned width=4) {for(unsigned i=0;i<width;++i)bytes[at+i]=std::uint8_t(n>>(8*i));}
AiActorImage& image(ActorPickupActor actor) {if(!actor.image)throw std::invalid_argument("Pickup needs an actor image");return *actor.image;}
template<class T> const T& need(const T& fn) {if(!fn)throw std::invalid_argument("Pickup needs a synchronous binding");return fn;}
std::array<ActorPickupClient,17>& clients(ActorPickupState& s) {if(!s.clients)throw std::invalid_argument("Pickup needs the shared client records");return *s.clients;}
std::uint32_t slot(ActorPickupActor actor,std::uint32_t base) {return (actor.original_address-base)/0x114u;}
}
void actor_pickup_refill(ActorPickupActor actor,ActorPickupState& s,const ActorPickupBindings& b) {
    if(!(image(actor)[0x6c]&1))return;
    for(std::size_t i=0;i<17;++i)if(clients(s)[i].active_18c&&clients(s)[i].actor_08==actor.identity) {
        need(b.client_message)(i,0x1d,0,0);need(b.resupply)(i);
    }
}
void actor_pickup_heal(ActorPickupActor actor,ActorPickupState& s,const ActorPickupBindings& b) {
    auto& a=image(actor);if(!(a[0x6c]&1))return;
    auto health=need(b.definition)(word(a,4)).health_94;
    if(s.rules_67f549&1)health=std::uint16_t(std::uint32_t(health)<<2);
    a[0x74]=std::uint8_t(health);a[0x75]=std::uint8_t(health>>8);need(b.sound)(0x165);
}
void actor_pickup_take(ActorPickupActor actor,ActorPickupActor target,ActorPickupState& s,const ActorPickupBindings& b) {
    auto& t=image(target);if(!word(t,4))return;
    if(!target.original_address)throw std::invalid_argument("Pickup target needs its source address");
    put(image(actor),0x90,target.original_address);put(t,0x6c,word(t,0x6c)|0x200102);
    auto id=need(b.definition)(word(t,4)).id;
    if(id==0xfc3||id==0xfc4)actor_pickup_flag_message(actor,s,b);
}
void actor_pickup_flag_message(ActorPickupActor actor,ActorPickupState& s,const ActorPickupBindings& b) {
    if(!s.host)return;
    need(b.message)(ActorPickupTransport::broadcast,0x20,{0x14,std::uint8_t(slot(actor,0x7cca80)),0});
    if(s.local_player==actor.identity)need(b.local_flag)();
}
void actor_pickup_return(ActorPickupActor actor,ActorPickupState& s,const ActorPickupBindings& b) {
    if(!s.host)return;
    std::vector<std::uint8_t> payload(2,0);store(payload,0,slot(actor,0x7cca80),2);
    need(b.remove)(actor);need(b.message)(ActorPickupTransport::reliable,0x10,payload);
}
std::uint32_t actor_pickup_restore_home(ActorPickupActor actor) {
    if(!actor.image||!actor.person)return 0;
    auto& a=image(actor);const auto& p=*actor.person;
    if(word(a,0x24)==p.position[0]&&word(a,0x28)==p.position[1]&&word(a,0x2c)==p.position[2]&&word(a,0x30)==p.angle_yaw)return 0;
    for(unsigned i=0;i<3;++i)put(a,0x24+i*4,p.position[i]);put(a,0x30,p.angle_yaw);
    put(a,0x6c,word(a,0x6c)|0x200100);return 1;
}
void actor_pickup_deposit(ActorPickupActor actor,ActorPickupState& s,const ActorPickupBindings& b) {
    if(!s.host)return;
    need(b.message)(ActorPickupTransport::broadcast,0x20,{0x15,std::uint8_t(slot(actor,0x7cca80)),0});
}
void actor_pickup_deliver(ActorPickupActor actor,ActorPickupActor carried,bool side_two,
    ActorPickupState& s,const ActorPickupBindings& b) {
    if(!s.host)return;
    if(side_two)++s.team_two_6e81c4;else ++s.team_one_6e815c;
    need(b.message)(ActorPickupTransport::broadcast,0x20,{0x13,std::uint8_t(slot(actor,0x7cca80)),0});
    actor_pickup_return(carried,s,b);
    for(std::size_t i=0;i<17;++i)if(clients(s)[i].active_18c&&clients(s)[i].actor_08==actor.identity)++clients(s)[i].deliveries_1bc;
}
void actor_pickup_objective(ActorPickupActor actor,ActorPickupPose pose,std::uint32_t mode,
    ActorPickupState& s,const ActorPickupBindings& b) {
    if(!s.host)return;
    if(!pose.bytes)throw std::invalid_argument("Pickup objective needs a live pose");
    std::vector<std::uint8_t> payload(18,0);store(payload,0,slot(actor,0x7cca80),2);
    for(unsigned i=0;i<12;++i)payload[2+i]=pose.bytes[i];store(payload,14,read(pose.bytes+12)>>16,2);payload[17]=std::uint8_t(mode);
    need(b.message)(ActorPickupTransport::reliable,0x27,payload);need(b.objective)(actor,pose,mode);
}
void actor_pickup_objective_spawn(std::uint32_t owner,Fixed3 position,const ActorPickupBindings& b) {
    const auto spawned=need(b.allocate)(owner,position);if(!spawned)return;
    const auto& a=image(*spawned);std::vector<std::uint8_t> payload(43,0);
    store(payload,0,slot(*spawned,0x7dde80),2);store(payload,2,need(b.definition)(word(a,4)).id,2);
    store(payload,20,word(a,0x6c));for(unsigned i=0;i<3;++i)store(payload,24+i*4,word(a,0x24+i*4));
    store(payload,36,word(a,0x30)>>16,2);store(payload,38,word(a,0x34)>>16,2);
    store(payload,40,owner?slot(*spawned,0x7cca80):0xffff,2);payload[42]=a[0x70];
    need(b.message)(ActorPickupTransport::reliable,0x2b,payload);
}
} // namespace dfre
