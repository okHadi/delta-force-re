#include "dfre/actor_collision.hpp"
#include "dfre/models.hpp"
#include <stdexcept>

using namespace dfre;
namespace {
void check(bool good) {if(!good)throw std::runtime_error("Actor collision test failed");}
void put(AiActorImage& a,unsigned at,std::uint32_t value) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(value>>(i*8));}
std::uint32_t get(const AiActorImage& a,unsigned at) {return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void collision() {
    AiActorImage a{},t{};put(t,0x10,10);put(t,4,20);
    ActorCollisionMesh mesh;mesh.matrix=model_identity_matrix();mesh.bounds={-0x20000,0x20000,-0x20000,0x20000,-0x20000,0x20000};
    CollisionVolume volume;volume.type=1;volume.bounds={-0x8000,0x8000,-0x8000,0x8000,-0x8000,0x8000};mesh.volumes.push_back(volume);
    std::vector<ActorCollisionMesh> meshes{mesh};ActorCollisionBindings b;unsigned calls=0;
    b.model=[](auto model){check(model==10);return ActorCollisionModel{0x20000,true};};
    b.state=[&](auto,auto target){check(target.image==&t);++calls;return &meshes;};
    b.definition=[](auto definition){check(definition==20);return ActorCollisionDefinition{0,0};};
    check(actor_target_collision({&a,1},{&t,2},b)==1&&get(a,0x24)==0x14000&&calls==1);
    put(a,0x24,0);meshes[0].matrix[3]=0x10000;
    check(actor_target_collision({&a,1},{&t,2},b)==1&&get(a,0x24)==0xffffc000);
    put(a,0x24,0);b.definition=[](auto){return ActorCollisionDefinition{0,0x10};};
    check(actor_target_collision({&a,1},{&t,2},b)==1&&get(a,0x24)==0);
    put(a,0x24,0x800000);check(actor_target_collision({&a,1},{&t,2},b)==0);
    check(actor_target_collision({},{}, {})==0);
    put(a,0x24,0);b.state={};
    try {actor_target_collision({&a,1},{&t,2},b);check(false);}catch(const std::invalid_argument&){}
}
void trigger() {
    AiActorImage a{},t{},carried{};put(t,4,20);put(a,0x70,1);a[0x6c]=1;
    ActorCollisionBindings b;check(actor_collision_trigger({}, {}, b,0xfedcba98)==0xfedcba98);
    b.host=true;std::uint32_t id=0xfaa;b.definition=[&](auto){return ActorCollisionDefinition{id,0x10};};
    std::vector<ActorCollisionCall> calls;
    b.call=[&](const auto& request){calls.push_back(request);return 123u;};
    check(actor_collision_trigger({&a,1},{&t,2},b)==123);
    check(calls.size()==2&&calls[0].site==ActorCollisionSite::refill&&calls[1].site==ActorCollisionSite::remove);
    calls.clear();id=0xfd6;put(t,0x74,100);put(t,0x76,99);t[0x6c]=1;
    b.call=[&](const auto& request){calls.push_back(request);if(request.site==ActorCollisionSite::objective)put(t,0x24,77);if(request.site==ActorCollisionSite::objective_effect)t[0x6c]|=0x20;return 0x12345678u;};
    check(actor_collision_trigger({&a,1},{&t,2},b)==0x12342578);
    check(get(t,0x74)==0&&t[0x6c]==0x25&&calls.size()==2&&calls[1].arguments[1]==77&&calls[1].arguments[3]==0x8000);
    calls.clear();id=0xfcb;put(a,0x90,3);put(carried,4,30);
    b.actor=[&](auto identity){check(identity==3);return ActorCollisionActor{&carried,3};};
    b.definition=[&](auto definition){return ActorCollisionDefinition{definition==30?0xfc3u:id,0x10};};
    b.call=[&](const auto& request){calls.push_back(request);put(a,0x90,99);return 12u;};
    check(actor_collision_trigger({&a,1},{&t,2},b)==12&&get(a,0x90)==0);
    check(calls.size()==1&&calls[0].site==ActorCollisionSite::deliver_side_one&&calls[0].arguments[1]==3);
}
}
int main() {try{collision();trigger();}catch(...){return 1;}return 0;}
