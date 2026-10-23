#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionModelLoadError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct MissionModelLoadBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
//486370: arguments contain destination, filename and bank. The four-byte
//texture-count frame and full7C load record use the same required memory owner.
std::uint32_t mission_model_load(const MissionModelLoadBindings&,
    std::uint32_t arguments,std::uint32_t texture_count_frame);
}
