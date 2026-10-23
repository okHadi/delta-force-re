#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionEffectEmitError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct MissionEffectEmitBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
//42A440: three current argument words and 114h local bytes share the bindings.
void mission_effect_emit(const MissionEffectEmitBindings&,std::uint32_t arguments,
    std::uint32_t frame,std::size_t bytes);
void mission_effect_definitions_initialize(const MissionEffectEmitBindings&); //45DD40
}
