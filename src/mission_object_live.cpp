#include "dfre/mission_object_live.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <exception>
#include <map>
#include <utility>
namespace dfre {
namespace {
template<class Image>std::uint32_t word(const Image& image,std::size_t at){std::uint32_t n;std::memcpy(&n,image.data()+at,4);return n;}
class Veh0 {
public:
    Veh0(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& b)
        :state_(state),ref_(ref),bindings_(b){
        if(!state.actor_at(ref)||ref.group>=4)throw MissionTickError("veh0 needs a current actor");
        self_.token=actor_handle(ref.group,ref.index);reload();
        scratch_.rng=state_.behavior.rng;scratch_.single_player=state_.globals.mission_67f528==0;
        world_.person_image=[&](std::uint32_t raw)->std::uint8_t*{
            if(raw<kPersonHandleBase)throw MissionTickError("Object callback reached a null or unknown person");
            if((raw-kPersonHandleBase)%kAiPersonRecordBytes)
                throw MissionTickError("veh0 person handle is not aligned");
            const auto slot=std::size_t((raw-kPersonHandleBase)/kAiPersonRecordBytes);
            if(slot>=state_.people.size())throw MissionTickError("veh0 person exceeds its current pool");
            auto found=people_.find(slot);
            if(found==people_.end())found=people_.emplace(slot,person(slot)).first;
            return found->second.data();
        };
        world_.actor_image=[&](std::uint32_t raw)->const std::uint8_t*{
            for(std::uint8_t group=0;group<4;++group)for(std::size_t slot=0;slot<state_.tables.groups[group].size();++slot){
                const auto identity=actor_handle(group,slot);
                if(raw!=identity && raw!=mission_actor_source_address(group,slot))continue;
                if(group==ref_.group&&slot==ref_.index)return self_.image.data();
                auto& image=others_[identity];image=snapshot_mission_actor(state_,{group,slot});return image.data();
            }
            throw MissionTickError("Object target has no current actor owner");
        };
        world_.global_dword=[&](auto at){
            if(!bindings_.global)throw MissionTickError("veh0 needs its current global owner");
            std::uint32_t n=0;boundary([&]{n=bindings_.global(state_,at);});return n;
        };
        world_.definition_dword=[&](auto index,auto at){
            if(!bindings_.definition)throw MissionTickError("veh0 needs its current definition owner");
            std::uint32_t n=0;boundary([&]{n=bindings_.definition(state_,index,at);});return n;
        };
        world_.type_slot_dword=[&](auto slot,auto at){
            if(!bindings_.type_slot)throw MissionTickError("veh0 needs its current type slot owner");
            std::uint32_t n=0;boundary([&]{n=bindings_.type_slot(state_,slot,at);});return n;
        };
        scratch_.call=[&](auto&,auto&,const ObjectBehaviorAction& action){
            MissionObjectCallResult result;
            boundary([&]{
                if(!bindings_.call)throw MissionTickError("veh0 foreign body is not bound "+std::to_string(action.site));
                result=bindings_.call(state_,ref_,action);
            });
            ObjectBehaviorInput input;input.site=action.site;input.values=result.values;input.output_mask=result.output_mask;return input;
        };
    }
    void run(std::uint32_t address=0x429bc0,std::uint32_t message=0){
        try{if(address==0x4519a0)object_behavior::slgt_ai(self_,scratch_,world_,message);else if(address==0x451cb0)object_behavior::slgt_move(self_,scratch_,world_);else if(address==0x429ea0)object_behavior::veh0_move(self_,scratch_,world_);else object_behavior::veh0_ai(self_,scratch_,world_,message);}
        catch(...){const auto error=std::current_exception();flush();std::rethrow_exception(error);}
        flush();
    }
private:
    AiPersonImage person(std::size_t slot){
        auto* actor=state_.actor_at(ref_);if(!actor)throw MissionTickError("veh0 actor owner was removed");
        BehaviorActor view{{ref_.group,ref_.index},self_.token,*actor,state_.people.at(slot),state_.extra_for(ref_),state_.person_extra(slot)};
        return behavior_person_image(view);
    }
    void flush(){
        const auto* actor=state_.actor_at(ref_);if(!actor)throw MissionTickError("Object callback actor was removed");
        const auto expected=actor->person_slot?person_handle(*actor->person_slot):0u;
        if(word(self_.image,0xc)!=expected)throw MissionTickError("Object callback changed its person binding without an owner");
        auto canonical=self_.image;const auto current=snapshot_mission_actor(state_,ref_);
        const auto raw_person=word(current,0xc);std::memcpy(canonical.data()+0xc,&raw_person,4);
        apply_mission_actor_image(state_,ref_,canonical);
        for(const auto& row:people_){
            auto& p=state_.people.at(row.first);auto& extra=state_.person_extra(row.first);
            p.flags24=row.second[0x24];extra.field_38=word(row.second,0x38);extra.field_44=word(row.second,0x44);
            extra.field_48=word(row.second,0x48);extra.field_4c=word(row.second,0x4c);extra.field_50=word(row.second,0x50);
            extra.field_a4=word(row.second,0xa4);p.tail_flag=extra.field_a4!=0;
            p.field_ac=word(row.second,0xac);p.field_b0=word(row.second,0xb0);
        }
        state_.behavior.rng=scratch_.rng;
    }
    void reload(){
        self_.image=snapshot_mission_actor(state_,ref_);
        const auto* actor=state_.actor_at(ref_);
        if(actor->person_slot && *actor->person_slot>=state_.people.size())throw MissionTickError("Object callback person exceeds its pool");
        const auto handle=actor->person_slot?person_handle(*actor->person_slot):0u;
        std::memcpy(self_.image.data()+0xc,&handle,4);
        for(auto& row:people_)row.second=person(row.first);
        scratch_.rng=state_.behavior.rng;
    }
    void boundary(const std::function<void()>& call){
        flush();
        try{call();}catch(...){const auto error=std::current_exception();reload();std::rethrow_exception(error);}
        reload();
    }
    MissionTickState& state_;AiActorRef ref_;const MissionObjectLiveBindings& bindings_;
    ObjectBehaviorActor self_;ObjectBehaviorState scratch_;ObjectBehaviorWorld world_;
    std::map<std::size_t,AiPersonImage> people_;
    std::map<std::uint32_t,AiActorImage> others_;
};
}
void mission_object_veh0(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& bindings){mission_object_veh0(state,ref,bindings,0);}
void mission_object_veh0(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& bindings,std::uint32_t message){Veh0(state,ref,bindings).run(0x429bc0,message);}
void mission_object_veh0_move(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& bindings){Veh0(state,ref,bindings).run(0x429ea0);}
void mission_object_slgt_move(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& bindings){Veh0(state,ref,bindings).run(0x451cb0);}
void mission_object_slgt_ai(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& bindings){mission_object_slgt_ai(state,ref,bindings,0);}
void mission_object_slgt_ai(MissionTickState& state,const AiActorRef& ref,const MissionObjectLiveBindings& bindings,std::uint32_t message){Veh0(state,ref,bindings).run(0x4519a0,message);}
void bind_mission_object_veh0(MissionTickState& state,MissionObjectLiveBindings bindings){
    if(state.update_callbacks.count(0x429bc0))throw MissionTickError("veh0 callback already has an owner");
    state.update_callbacks.emplace(0x429bc0,[bindings=std::move(bindings)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=0x429bc0)throw MissionTickError("veh0 callback received a different body");
        mission_object_veh0(current,ref,bindings);
    });
}
void bind_mission_object_veh0_move(MissionTickState& state,MissionObjectLiveBindings bindings){
    if(state.secondary_callbacks.count(0x429ea0))throw MissionTickError("veh0 move callback already has an owner");
    state.secondary_callbacks.emplace(0x429ea0,[bindings=std::move(bindings)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::secondary||action.target!=0x429ea0)throw MissionTickError("veh0 move callback received a different body");
        mission_object_veh0_move(current,ref,bindings);
    });
}
void bind_mission_object_slgt_move(MissionTickState& state,MissionObjectLiveBindings bindings){
    if(state.secondary_callbacks.count(0x451cb0))throw MissionTickError("Searchlight move callback already has an owner");
    state.secondary_callbacks.emplace(0x451cb0,[bindings=std::move(bindings)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::secondary||action.target!=0x451cb0)throw MissionTickError("Searchlight move callback received a different body");
        mission_object_slgt_move(current,ref,bindings);
    });
}
void bind_mission_object_slgt_ai(MissionTickState& state,MissionObjectLiveBindings bindings){
    if(state.update_callbacks.count(0x4519a0))throw MissionTickError("Searchlight AI callback already has an owner");
    state.update_callbacks.emplace(0x4519a0,[bindings=std::move(bindings)](auto& current,const auto& ref,const auto& action){
        if(action.kind!=AiActorActionKind::update||action.target!=0x4519a0)throw MissionTickError("Searchlight AI callback received a different body");
        mission_object_slgt_ai(current,ref,bindings);
    });
}
}
