#pragma once
#include "dfre/mission_live.hpp"
namespace dfre {
struct MissionRoundCheckBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<void(std::uint32_t slot,std::uint32_t type,std::uint8_t* payload,std::uint32_t length)> enqueue_441750;
    // 447930 passes {type,current byte,length} to4423F0. Other arguments are source words.
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>& args)> call;
};
void mission_round_broadcast(const MissionRoundCheckBindings&,std::uint32_t type,std::uint8_t* payload,std::uint32_t length); //4423F0
void mission_round_check_end(const MissionRoundCheckBindings&); //444630
void mission_round_check_clients(const MissionRoundCheckBindings&); //448590
void mission_round_publish_status(const MissionRoundCheckBindings&); //447930
// Share canonical globals, local actor, group2 count and current actor records.
// Host owns clients, squads, other cells, definition pointers and reached calls.
MissionRoundCheckBindings mission_round_check_bindings(MissionTickState&,MissionRoundCheckBindings host);
}
