#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionModelTextureBindings {
    std::function<std::uint32_t(std::uint32_t,unsigned)> read;
    std::function<void(std::uint32_t,unsigned,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// Current bank DWORD at args. Source body485BA0; storage remains host-owned.
void mission_model_texture_load(const MissionModelTextureBindings&,std::uint32_t args);
// Source485B10: clear current texture count and all2000 descriptors.
void mission_model_texture_initialize(const MissionModelTextureBindings&);
}
