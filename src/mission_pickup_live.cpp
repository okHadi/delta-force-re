#include "dfre/mission_pickup_live.hpp"
#include <exception>
#include <map>

namespace dfre {
namespace {
template<class T> const T& need(const T& fn,const char* message) {if(!fn)throw MissionTickError(message);return fn;}
struct RawActor {AiActorRef ref;AiActorImage image;};
class PickupLive {
public:
    PickupLive(MissionTickState& live,ActorPickupState& shared,const MissionPickupLiveBindings& foreign)
        :live_(live),shared_(shared),foreign_(foreign) {
        bindings_.definition=[&](std::uint32_t index){
            if(index>=live_.definitions.entries().size())throw MissionTickError("Pickup definition exceeds its table");
            const auto& definition=live_.definitions.entries()[index];return ActorPickupDefinition{definition.id,definition.field_94};
        };
        bindings_.client_message=[&](auto client,auto message,auto a,auto b){around([&]{need(foreign_.client_message,"Pickup client message is not bound")(live_,shared_,client,message,a,b);});};
        bindings_.resupply=[&](auto client){around([&]{need(foreign_.resupply,"Pickup resupply is not bound")(live_,shared_,client);});};
        bindings_.sound=[&](auto sound){around([&]{need(foreign_.sound,"Pickup sound is not bound")(live_,shared_,sound);});};
        bindings_.local_flag=[&]{around([&]{need(foreign_.local_flag,"Pickup local flag effect is not bound")(live_,shared_);});};
        bindings_.remove=[&](ActorPickupActor value){const auto ref=decode(value.identity);around([&]{need(foreign_.remove,"Pickup removal is not bound")(live_,shared_,ref);});};
        bindings_.message=[&](auto transport,auto id,const auto& payload){around([&]{need(foreign_.message,"Pickup transport is not bound")(live_,shared_,transport,id,payload);});};
        bindings_.objective=[&](ActorPickupActor value,ActorPickupPose pose,std::uint32_t mode){
            const auto actor=decode(value.identity),target=pose_ref(pose.identity);
            around([&]{need(foreign_.objective,"Pickup objective logic is not bound")(live_,shared_,actor,target,mode);});
        };
        bindings_.allocate=[&](std::uint32_t owner,Fixed3 position)->std::optional<ActorPickupActor>{
            if(owner)decode(owner);
            std::optional<AiActorRef> ref;
            around([&]{ref=need(foreign_.allocate,"Pickup allocation is not bound")(live_,shared_,owner,position);});
            if(!ref)return std::nullopt;
            if(ref->group!=0)throw MissionTickError("Pickup allocation needs a group-zero actor");
            return record(*ref);
        };
    }
    std::uint32_t run(const ActorCollisionCall& request) {
        sync_cells();
        try {
            std::uint32_t result=0;
            const auto primary=[&]{const auto ref=decode(request.arguments[0]);if(ref.group!=3)throw MissionTickError("Pickup source needs a group-three actor");return record(ref);};
            switch(request.site) {
            case ActorCollisionSite::refill:count(request,1);actor_pickup_refill(primary(),shared_,bindings_);break;
            case ActorCollisionSite::heal:count(request,1);actor_pickup_heal(primary(),shared_,bindings_);break;
            case ActorCollisionSite::can_use:count(request,1);result=actor_pickup_restore_home(record(decode(request.arguments[0])));break;
            case ActorCollisionSite::pickup:count(request,2);actor_pickup_take(primary(),record(decode(request.arguments[1])),shared_,bindings_);break;
            case ActorCollisionSite::deposit:count(request,1);actor_pickup_deposit(primary(),shared_,bindings_);break;
            case ActorCollisionSite::deliver_side_one:case ActorCollisionSite::deliver_side_two:
                count(request,2);actor_pickup_deliver(primary(),record(decode(request.arguments[1])),request.site==ActorCollisionSite::deliver_side_two,shared_,bindings_);break;
            case ActorCollisionSite::objective: {
                count(request,3);const auto ref=pose_ref(request.arguments[1]);const auto pose=record(ref);
                actor_pickup_objective(primary(),{pose.image->data()+0x24,request.arguments[1]},request.arguments[2],shared_,bindings_);break;
            }
            case ActorCollisionSite::objective_effect:
                count(request,4);actor_pickup_objective_spawn(request.arguments[0],{signed_bits(request.arguments[1]),signed_bits(request.arguments[2]),signed_bits(request.arguments[3])},bindings_);break;
            case ActorCollisionSite::remove: {
                count(request,1);const auto ref=decode(request.arguments[0]);record(ref);
                around([&]{need(foreign_.remove,"Pickup removal is not bound")(live_,shared_,ref);});break;
            }
            case ActorCollisionSite::sound:
                count(request,1);around([&]{result=need(foreign_.sound,"Pickup sound is not bound")(live_,shared_,request.arguments[0]);});break;
            }
            flush();return result;
        } catch(...) {const auto error=std::current_exception();flush();std::rethrow_exception(error);}
    }
private:
    static std::int32_t signed_bits(std::uint32_t n) {return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
    static void count(const ActorCollisionCall& request,std::uint32_t n) {if(request.count!=n)throw MissionTickError("Pickup call argument count differs");}
    AiActorRef decode(std::uint32_t identity) const {
        if(identity<kActorHandleBase)throw MissionTickError("Pickup actor handle is invalid");
        const auto key=identity-kActorHandleBase,group=key/0x10000u,slot=key%0x10000u;
        if(group>=4||slot>=live_.tables.groups[group].size())throw MissionTickError("Pickup actor exceeds its table");
        return {std::uint8_t(group),slot};
    }
    AiActorRef pose_ref(std::uint32_t identity) const {
        if(identity<0x24)throw MissionTickError("Pickup objective pose handle is invalid");
        return decode(identity-0x24);
    }
    ActorPickupActor record(const AiActorRef& ref) {
        if(ref.group>=4||!live_.actor_at(ref))throw MissionTickError("Pickup actor exceeds its table");
        const auto identity=actor_handle(ref.group,ref.index);auto found=records_.find(identity);
        if(found==records_.end())found=records_.emplace(identity,RawActor{ref,snapshot_mission_actor(live_,ref)}).first;
        AiPersonState* person=nullptr;
        if(const auto slot=live_.actor_at(ref)->person_slot) {
            if(*slot>=live_.people.size())throw MissionTickError("Pickup person exceeds its pool");person=&live_.people[*slot];
        }
        return {&found->second.image,identity,mission_actor_source_address(ref.group,ref.index),person};
    }
    void flush() {
        std::exception_ptr error;
        for(const auto& entry:records_)try{apply_mission_actor_image(live_,entry.second.ref,entry.second.image);}catch(...){if(!error)error=std::current_exception();}
        if(error)std::rethrow_exception(error);
    }
    void refresh() {for(auto& entry:records_)entry.second.image=snapshot_mission_actor(live_,entry.second.ref);}
    void sync_cells() {
        shared_.host=live_.globals.mission_67f528;shared_.local_player=0;
        if(live_.config.local_actor) {
            const AiActorRef ref{3,*live_.config.local_actor};if(!live_.actor_at(ref))throw MissionTickError("Pickup local actor exceeds its table");
            shared_.local_player=actor_handle(3,ref.index);
        }
    }
    void around(const std::function<void()>& call) {
        flush();sync_cells();
        try{call();}catch(...){const auto error=std::current_exception();refresh();sync_cells();std::rethrow_exception(error);}
        refresh();sync_cells();
    }
    MissionTickState& live_;
    ActorPickupState& shared_;
    const MissionPickupLiveBindings& foreign_;
    std::map<std::uint32_t,RawActor> records_;
    ActorPickupBindings bindings_;
};
bool supported(ActorCollisionSite site) {
    switch(site) {
    case ActorCollisionSite::refill:case ActorCollisionSite::heal:case ActorCollisionSite::can_use:
    case ActorCollisionSite::pickup:case ActorCollisionSite::deposit:case ActorCollisionSite::deliver_side_one:
    case ActorCollisionSite::deliver_side_two:case ActorCollisionSite::objective:case ActorCollisionSite::objective_effect:
    case ActorCollisionSite::remove:case ActorCollisionSite::sound:return true;
    }return false;
}
}
MissionCollisionEffect mission_pickup_live_effect(ActorPickupState& shared,MissionPickupLiveBindings bindings) {
    return [&shared,bindings=std::move(bindings)](MissionTickState& live,const ActorCollisionCall& call)->std::optional<std::uint32_t>{
        if(!supported(call.site))return std::nullopt;return PickupLive(live,shared,bindings).run(call);
    };
}
} // namespace dfre
