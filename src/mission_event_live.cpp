#include "dfre/mission_event_live.hpp"
#include <utility>
namespace dfre {
MissionEventPersonBindings mission_event_live_bindings(MissionTickState& state,
 std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read,
 std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write,
 std::function<std::uint32_t(std::size_t)> definition,
 std::function<std::uint16_t()> control){
 return mission_event_person_current_bindings(state,std::move(read),std::move(write),std::move(definition),std::move(control));
}
}
