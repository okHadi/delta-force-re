#include "dfre/mission_actor_lifecycle.hpp"
#include <cstring>
#include <utility>
namespace dfre {
namespace {
template<class F> const F& need(const F& f,const char* message){if(!f)throw MissionTickError(message);return f;}
}
MissionActorLifecycle::MissionActorLifecycle(MissionTickState& state,MissionAnimations& animations,
    std::array<MissionAudioRegistration,1024>& registrations,MissionActorLifecycleBindings bindings)
    :state_(state),animations_(animations),registrations_(registrations),bindings_(std::move(bindings)){}
std::uint32_t MissionActorLifecycle::read(const AiActorRef& ref,std::size_t at) const {
    const auto image=snapshot_mission_actor(state_,ref);std::uint32_t value;std::memcpy(&value,image.data()+at,4);return value;
}
void MissionActorLifecycle::write(const AiActorRef& ref,std::size_t at,std::uint32_t value,std::uint8_t width){
    auto image=snapshot_mission_actor(state_,ref);
    for(unsigned i=0;i<width;++i)image[at+i]=std::uint8_t(value>>(8*i));
    apply_mission_actor_image(state_,ref,image);
}
AiActorRef MissionActorLifecycle::linked_actor(std::uint32_t identity) const {
    constexpr std::array<std::uint32_t,4> capacities{1024,1024,512,256};
    for(std::uint8_t group=0;group<4;++group){
        const auto base=mission_actor_source_address(group,0);if(identity<base)continue;
        const auto delta=identity-base;
        if(delta%0x114)continue;
        const auto slot=delta/0x114;if(slot<capacities[group]&&slot<state_.tables.groups[group].size())return {group,slot};
    }
    throw MissionTickError("Actor link has no current source record");
}
void MissionActorLifecycle::drop_link(const AiActorRef& ref){
    const auto identity=read(ref,0x90);write(ref,0x90,0);
    if(!identity)return;
    const auto linked=linked_actor(identity);
    write(linked,0x6c,read(linked,0x6c)&0xfefdu,2);
    write(linked,0x6d,(read(linked,0x6c)>>8)|1u,1);
    if(!(read(ref,0x6c)&4u)){
        std::int32_t sine=0,cosine=0;
        need(bindings_.sincos_4776c0,"Actor drop sine/cosine body is not bound")(read(ref,0x30),sine,cosine);
        write(linked,0x24,read(ref,0x24)+std::uint32_t(cosine>>5));
        const auto y=read(ref,0x28)+std::uint32_t(sine>>5);write(linked,0x28,y);
        const auto z=need(bindings_.terrain_47df40,"Actor drop terrain body is not bound")(read(linked,0x24),y);
        write(linked,0x2c,z);
    }else{
        const auto y=read(linked,0x28),x=read(linked,0x24);
        const auto z=need(bindings_.terrain_47df40,"Actor drop terrain body is not bound")(x,y);
        write(linked,0x2c,z);
    }
}
void MissionActorLifecycle::release_character(std::uint32_t instance){animations_.release(instance);}
void MissionActorLifecycle::unregister_audio(const AiActorRef& ref){
    if(!state_.actor_at(ref)||ref.group>=4)throw MissionTickError("Audio removal actor exceeds its table");
    const auto identity=mission_actor_source_address(ref.group,ref.index);
    for(auto& registration:registrations_)if(registration.actor==identity)registration={};
}
void MissionActorLifecycle::remove(std::optional<AiActorRef> actor){
    if(!actor)return;const auto ref=*actor;const auto definition=read(ref,4);
    if(definition){
        if(definition>=state_.definitions.entries().size())throw MissionTickError("Removal definition exceeds its table");
        if(state_.definitions.entries()[definition].field_28==3){drop_link(ref);release_character(read(ref,0x18));}
    }
    const auto person=state_.actor_at(ref)->person_slot;
    if(person){
        if(*person>=state_.people.size()||*person>=state_.person_extras.size())throw MissionTickError("Removal person exceeds its pool");
        state_.people[*person]={};state_.person_extras[*person]={};
    }
    *state_.actor_at(ref)=AiActorState{};
    state_.extra_for(ref)=BehaviorActorExtra{};
    state_.stored_actor_images.at(ref.group).at(ref.index).fill(0);
    auto& runtime=state_.runtime_for(ref);
    runtime.driver.reset();runtime.animation=nullptr;
    runtime.byte_6e=0;runtime.roll_38=0;runtime.vertical_44=0;runtime.ground_100=0;runtime.link_90=0;
    unregister_audio(ref);
}
} // namespace dfre
