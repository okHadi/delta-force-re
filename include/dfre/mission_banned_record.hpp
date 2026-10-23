#pragma once
#include "dfre/mission_line_file.hpp"
namespace dfre {
//448690. Keep the eight-byte source local frame mapped through atoi calls.
std::uint32_t mission_banned_record(const MissionLineFileBindings&,
    std::uint32_t record,std::uint32_t frame);
}
