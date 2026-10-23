#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionItemResourceError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct MissionItemResourceBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
//45D6F0: flag is a caller-owned aligned word in the same current memory owner.
void mission_item_resource_load(const MissionItemResourceBindings&,std::uint32_t index,
    std::uint32_t flag);
}
