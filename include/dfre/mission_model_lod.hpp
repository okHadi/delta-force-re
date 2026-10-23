#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionModelLodBindings {
    std::function<std::uint32_t(std::uint32_t, unsigned)> read;
    std::function<void(std::uint32_t, unsigned, std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t, const std::vector<std::uint32_t>&)> call;
    // Owns source 4860BE..4860D6: current signed word, writable constants,
    // x87 control/status and result store. The result address is frame+24h.
    std::function<void(std::uint32_t, std::uint32_t)> material_ticks;
};
// args: current start/end/bank DWORDs. frame: aligned 40-byte source scratch.
// All raw storage and reached services remain caller-owned.
std::uint32_t mission_model_lod_load(const MissionModelLodBindings&,
                                    std::uint32_t args, std::uint32_t frame);
}
