#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
namespace dfre {
struct MissionLocalizationBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<std::uint32_t(std::uint32_t table,std::uint32_t group,std::uint32_t key)> lookup_49a092;
};
//404CF0. Return the current source text pointer or its source sentinel.
std::uint32_t mission_localized_text(const MissionLocalizationBindings&,
    std::uint32_t group,std::uint32_t key);
}
