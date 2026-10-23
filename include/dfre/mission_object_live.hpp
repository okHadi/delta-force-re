#pragma once
#include "dfre/mission_live.hpp"
#include "dfre/object_live_callbacks.hpp"
namespace dfre {
struct MissionObjectCallResult {
    std::array<std::uint32_t,6> values{};
    std::uint8_t output_mask=0x3e;
    MissionObjectCallResult()=default;
    MissionObjectCallResult(std::array<std::uint32_t,6> output):values(output){}
};
struct MissionObjectLiveBindings {
    std::function<std::uint32_t(MissionTickState&,std::uint32_t index,std::uint32_t offset)> definition;
    std::function<std::uint32_t(MissionTickState&,std::uint32_t slot,std::uint32_t offset)> type_slot;
    std::function<std::uint32_t(MissionTickState&,std::uint32_t address)> global;
    std::function<MissionObjectCallResult(MissionTickState&,const AiActorRef&,
        const ObjectBehaviorAction&)> call;
};
// 429BC0 only. Keep current actor/person ownership and the shared behavior RNG.
void mission_object_veh0(MissionTickState&,const AiActorRef&,const MissionObjectLiveBindings&);
void mission_object_veh0(MissionTickState&,const AiActorRef&,const MissionObjectLiveBindings&,std::uint32_t message);
// Register only the proven veh0 AI entry. Existing ownership is an error.
void bind_mission_object_veh0(MissionTickState&,MissionObjectLiveBindings);
// 429EA0 only. Keep definition reads and foreign sine/terrain calls current.
void mission_object_veh0_move(MissionTickState&,const AiActorRef&,const MissionObjectLiveBindings&);
void bind_mission_object_veh0_move(MissionTickState&,MissionObjectLiveBindings);
// 451CB0 with its complete451850 sweep body.
void mission_object_slgt_move(MissionTickState&,const AiActorRef&,const MissionObjectLiveBindings&);
void bind_mission_object_slgt_move(MissionTickState&,MissionObjectLiveBindings);
void mission_object_slgt_ai(MissionTickState&,const AiActorRef&,const MissionObjectLiveBindings&);
void mission_object_slgt_ai(MissionTickState&,const AiActorRef&,const MissionObjectLiveBindings&,std::uint32_t message);
void bind_mission_object_slgt_ai(MissionTickState&,MissionObjectLiveBindings);
}
