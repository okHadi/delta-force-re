#include "dfre/mission_area_live.hpp"
#include <exception>
#include <map>

namespace dfre {
namespace {
template<class T> const T& need(const T& fn,const char* message) {if(!fn)throw MissionTickError(message);return fn;}
struct RawActor { AiActorRef ref; AiActorImage image; };
class LiveArea {
public:
    LiveArea(MissionTickState& state,const MissionGeometry& geometry,const MissionAreaLiveBindings& bindings)
        :live_(state),geometry_(geometry),foreign_(bindings) {
        stage_.pending=&live_.pending_spawns;stage_.random=&live_.behavior;
        bindings_.count=[&](std::uint32_t group){return live_.tables.groups.at(group).size();};
        bindings_.actor=[&](MissionAreaActorRef ref){return record({std::uint8_t(ref.group),ref.index});};
        bindings_.owner=[&](std::uint32_t identity){return record(decode(identity));};
        bindings_.definition=[&](std::uint32_t index){
            if(index>=live_.definitions.entries().size())throw MissionTickError("Area definition exceeds its table");
            const auto& definition=live_.definitions.entries()[index];
            return MissionAreaDefinition{definition.field_28,definition.id};
        };
        bindings_.bounds=[&](std::uint32_t handle){return MissionAreaBounds{geometry_.bounds(handle).radius_64,geometry_.height_68(handle)};};
        bindings_.segment=[&](MissionAreaActorRef ref,const Fixed3& start,const Fixed3& end){
            std::int32_t result=0;
            around([&]{result=need(foreign_.segment,"Area segment query is not bound")(live_,convert(ref),start,end);});
            return result;
        };
        bindings_.update=[&](MissionAreaActorRef ref,std::uint32_t callback,std::uint32_t message){
            around([&]{need(foreign_.update,"Area actor update is not bound")(live_,convert(ref),callback,message);});
        };
        bindings_.local_hit=[&]{around([&]{need(foreign_.local_hit,"Area local-hit effect is not bound")(live_);});};
        bindings_.listener=[&](const MissionListenerPose& pose){
            around([&]{need(foreign_.listener,"Area listener effect is not bound")(live_,pose);});
        };
    }
    void run(std::uint32_t address) {
        try {
            sync_globals();
            if(address==0x45e050)mission_listener_stage(stage_,bindings_);
            else mission_pending_area_stage(stage_,bindings_);
        } catch(...) {
            const auto error=std::current_exception();flush();std::rethrow_exception(error);
        }
        flush();
    }
private:
    static AiActorRef convert(MissionAreaActorRef ref) {
        if(ref.group>=4)throw MissionTickError("Area actor group is invalid");
        return {std::uint8_t(ref.group),ref.index};
    }
    AiActorRef decode(std::uint32_t identity) const {
        if(identity<kActorHandleBase)throw MissionTickError("Area owner handle is invalid");
        const auto key=identity-kActorHandleBase;const auto group=key/0x10000u,slot=key%0x10000u;
        if(group>=4||slot>=live_.tables.groups[group].size())throw MissionTickError("Area owner exceeds its table");
        return {std::uint8_t(group),slot};
    }
    MissionAreaActor record(const AiActorRef& ref) {
        if(ref.group>=4||!live_.actor_at(ref))throw MissionTickError("Area actor exceeds its table");
        const auto identity=actor_handle(ref.group,ref.index);
        auto found=records_.find(identity);
        if(found==records_.end())found=records_.emplace(identity,RawActor{ref,snapshot_mission_actor(live_,ref)}).first;
        return {&found->second.image,identity};
    }
    void flush() {
        std::exception_ptr error;
        for(const auto& entry:records_) {
            try {apply_mission_actor_image(live_,entry.second.ref,entry.second.image);}
            catch(...) {if(!error)error=std::current_exception();}
        }
        if(error)std::rethrow_exception(error);
    }
    void refresh() {for(auto& entry:records_)entry.second.image=snapshot_mission_actor(live_,entry.second.ref);}
    void sync_globals() {
        const auto values=need(foreign_.globals,"Area external globals are not bound")(live_);
        stage_.host=live_.globals.mission_67f528;stage_.rules_67f549=values.rules_67f549;
        stage_.water=live_.world.water_level;stage_.alternate_listener=values.alternate_listener_6ba3ec;
        stage_.alternate_pose=values.alternate_pose;stage_.incoming_dispatch=values.incoming_dispatch;
        stage_.local_player={};stage_.local_player_identity=0;
        if(live_.config.local_actor) {
            stage_.local_player=record({3,*live_.config.local_actor});
            stage_.local_player_identity=stage_.local_player.identity;
        }
    }
    void around(const std::function<void()>& call) {
        flush();
        try {call();}
        catch(...) {
            const auto error=std::current_exception();refresh();sync_globals();std::rethrow_exception(error);
        }
        refresh();sync_globals();
    }
    MissionTickState& live_;
    const MissionGeometry& geometry_;
    const MissionAreaLiveBindings& foreign_;
    std::map<std::uint32_t,RawActor> records_;
    MissionProjectileStageState stage_;
    MissionProjectileStageBindings bindings_;
};
}

std::optional<std::uint32_t> mission_area_live_stage(MissionTickState& state,const MissionGeometry& geometry,
    const MissionStageCall& call,const MissionAreaLiveBindings& bindings) {
    if(call.address!=0x45e050&&call.address!=0x465bd0)return std::nullopt;
    if(call.address==0x465bd0&&(!state.globals.mission_67f528||state.pending_spawns.empty()))return 0;
    LiveArea(state,geometry,bindings).run(call.address);return 0;
}
} // namespace dfre
