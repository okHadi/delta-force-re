#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionModelRegistryError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct MissionModelRegistryBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
//486530: arguments hold current name, bank and flag pointer words. The 32-byte
//scratch and all source accesses use the same required memory owner.
std::uint32_t mission_model_registry_load(const MissionModelRegistryBindings&,
    std::uint32_t arguments,std::uint32_t frame);
}
