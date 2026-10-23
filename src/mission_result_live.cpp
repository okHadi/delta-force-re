#include "dfre/mission_result_live.hpp"
#include <array>
#include <cstring>
#include <memory>
namespace dfre {namespace {
using U=std::uint32_t;
constexpr std::array<std::size_t,4> capacities{1024,1024,512,256};
template<class F>const F& need(const F& f){if(!f)throw MissionTickError("Mission result live reached a missing provider");return f;}
U address(const AiActorRef& ref){if(ref.group>=4||ref.index>=capacities[ref.group])throw MissionTickError("Mission result actor exceeds its physical pool");return mission_actor_source_address(ref.group,ref.index);}
AiActorRef locate(U pointer){for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(pointer>=base&&std::uint64_t(pointer-base)<capacities[group]*0x114ull){if((pointer-base)%0x114)throw MissionTickError("Mission result actor pointer is not aligned");return {group,(pointer-base)/0x114};}}throw MissionTickError("Mission result actor pointer has no physical owner");}
struct Adapter {
    MissionTickState& state;MissionResultLiveHost host;
    AiActorImage image(const AiActorRef& ref){address(ref);if(state.tables.groups[ref.group].size()>capacities[ref.group])throw MissionTickError("Mission result actor count exceeds its physical pool");if(ref.index>=state.tables.groups[ref.group].size())return need(host.physical_tail)(ref);
        auto result=snapshot_mission_actor(state,ref);const auto person=state.tables.groups[ref.group][ref.index].person_slot;
        if(person&&*person>=0x500)throw MissionTickError("Mission result person exceeds its physical pool");
        const U pointer=person?0x794680+U(*person)*0xb4:0;std::memcpy(result.data()+0xc,&pointer,4);return result;
    }
    std::string name(U pointer){const auto bytes=image(locate(pointer));U person;std::memcpy(&person,bytes.data()+0xc,4);if(!person)return need(host.fallback_name)(pointer);
        if(person<0x794680||(person-0x794680)%0xb4)throw MissionTickError("Mission result person pointer is invalid");const auto slot=(person-0x794680)/0xb4;
        if(slot>=0x500||slot>=state.people.size())throw MissionTickError("Mission result person has no current owner");return state.people[slot].name;
    }
};
}
MissionLogicResultBindings mission_result_live_bindings(MissionTickState& state,MissionResultLiveHost host){
    auto owner=std::make_shared<Adapter>(Adapter{state,std::move(host)});MissionLogicResultBindings out;out.state=owner->host.state;out.store_name=owner->host.store_name;out.publish=owner->host.publish;
    out.actor_identity=[](U group,std::size_t slot){if(group>3)throw MissionTickError("Mission result actor group is invalid");return address({std::uint8_t(group),slot});};
    out.actor_source_address=[](U pointer){return pointer;};
    out.local_actor=[owner]()->std::optional<U>{if(!owner->state.config.local_actor)return {};const AiActorRef ref{3,*owner->state.config.local_actor};if(ref.index>=owner->state.tables.groups[3].size())throw MissionTickError("Mission result local actor has no current owner");return address(ref);};
    out.actor_word=[owner](U pointer,U offset,U width){if((width!=1&&width!=2&&width!=4)||std::uint64_t(offset)+width>0x114)throw MissionTickError("Mission result actor read crosses its source record");const auto bytes=owner->image(locate(pointer));U result=0;for(U i=0;i<width;++i)result|=U(bytes[offset+i])<<(8*i);return result;};
    out.actor_name=[owner](U pointer){return owner->name(pointer);};return out;
}
} // namespace dfre
