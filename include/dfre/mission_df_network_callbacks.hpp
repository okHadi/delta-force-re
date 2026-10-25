#pragma once
#include "dfre/mission_client_added.hpp"
namespace dfre {
//43FD60 uses E8 hex current local bytes.43FEF0/43FF20 need no local span.
//Keep all reached foreign bodies required and all current memory in one owner.
void mission_df_network_callback(const MissionClientAddedBindings&,
    std::uint32_t entry,std::uint32_t scratch=0,std::uint32_t bytes=0);
} // namespace dfre
