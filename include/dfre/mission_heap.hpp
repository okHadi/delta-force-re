#pragma once
#include "dfre/mission_periodic_services.hpp"
namespace dfre {
// Current contexts, arenas, callback rows and caller frames share one raw owner.
std::uint32_t mission_heap_initialize(const MissionPeriodicBindings&); //497E20
// 30h bytes: locals0/4/8; current size20h/alignment24h/unused28h/label2Ch.
std::uint32_t mission_heap_allocate(const MissionPeriodicBindings&,
    std::uint32_t frame,std::uint32_t incoming_ebx); //497F60
// Ch-byte caller frame; current payload argument at+8 is read at source sites.
std::uint32_t mission_heap_release(const MissionPeriodicBindings&,std::uint32_t frame); //4980D0
std::uint32_t mission_heap_find_previous(const MissionPeriodicBindings&,std::uint32_t header); //498550
void mission_heap_notify_free(const MissionPeriodicBindings&,std::uint32_t payload); //498850
// 498A00 plus498960/498990/4989C0. The found cell is a current DWORD.
std::uint32_t mission_heap_find_hole(const MissionPeriodicBindings&,
    std::uint32_t start,std::uint32_t candidate,std::uint32_t total_size,
    std::uint32_t found_cell);
}
