#include "dfre/mission_object_smoke.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <utility>
namespace dfre {
namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
struct Access {
    const MissionObjectSmokeBindings& b;
    U r(U at,U width=4)const{if(!b.read)throw MissionTickError("Smoke reader is not bound");return b.read(at,width);}
    void w(U at,U value,U width=4)const{if(!b.write)throw MissionTickError("Smoke writer is not bound");b.write(at,width,width==1?value&255:value);}
    void call(U site,std::initializer_list<U> args)const{if(!b.call)throw MissionTickError("Smoke body is not bound");b.call(site,args);}
};
U source(const MissionTickState& state,const AiActorRef& ref){
    static constexpr std::size_t caps[]{1024,1024,512,256,1024};
    if(ref.group>4||ref.index>=caps[ref.group]||!state.actor_at(ref))throw MissionTickError("Smoke actor has no physical owner");
    return ref.group==4?0x822e80u+U(ref.index)*0x114:mission_actor_source_address(ref.group,ref.index);
}
}
void mission_object_smoke_generator(const MissionObjectSmokeBindings& b,U actor){
    const Access m{b};const U owner=m.r(actor+0xf8);
    if(owner&&!(m.r(owner+0x6c,1)&4)){m.call(0x45da30,{actor});return;}
    const U from=owner?owner:actor,z=m.r(from+0x2c),y=m.r(from+0x28),x=m.r(from+0x24);m.call(0x42a440,{x,y,z});
    const U count=m.r(actor+0x7c)+1;m.w(actor+0x7c,count);
    if(si(count)<100){
        if(!b.call)throw MissionTickError("Smoke random body is not bound");const U random=b.call(0x477860,{});m.w(actor+0xfc,U(si(random)>>13)+16);
    }else m.call(0x45da30,{actor});
}
void mission_object_smoke_move(const MissionObjectSmokeBindings& b,U actor){
    const Access m{b};const U velocity=m.r(actor+0x40),x=m.r(actor+0x24),y=m.r(actor+0x28);
    const U velocity_again=m.r(actor+0x40),z=m.r(actor+0x2c),growth=m.r(actor+0x7c),vertical=m.r(actor+0x44);
    m.w(actor+0x24,x+velocity);const U growth_again=m.r(actor+0x7c);m.w(actor+0x28,y+U(si(velocity_again)>>2));m.w(actor+0x2c,z+vertical);
    const U counter=m.r(actor+0x3b,1);m.w(actor+0x7c,growth+(growth_again>>7));const U current_growth=m.r(actor+0x7c);m.w(actor+0x3b,counter+1,1);
    if(current_growth>0x20000)m.w(actor+0x7c,0x20000);
    const U vertical_now=m.r(actor+0x44),velocity_now=m.r(actor+0x40);m.w(actor+0x44,vertical_now+0x20);m.w(actor+0x40,velocity_now+8);
}
void bind_mission_object_smoke(MissionTickState& state,MissionObjectSmokeBindings bindings){
    if(state.update_callbacks.count(0x42a520)||state.secondary_callbacks.count(0x42a590))throw MissionTickError("Smoke callbacks already have an owner");
    state.update_callbacks.emplace(0x42a520,[bindings](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=0x42a520)throw MissionTickError("Smoke generator received another callback");mission_object_smoke_generator(bindings,source(current,ref));
    });
    state.secondary_callbacks.emplace(0x42a590,[bindings=std::move(bindings)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::secondary||action.target!=0x42a590)throw MissionTickError("Smoke move received another callback");mission_object_smoke_move(bindings,source(current,ref));
    });
}
}
