#pragma once
#include "dfre/world_runtime.hpp"
namespace dfre {
// Keep runtime and cache alive. Reserve the complete base/stride/capacity span
// against other virtual owners. Only each row's+10..+13 weight bytes are exposed.
class MissionAnimationTableLive {
public:
    MissionAnimationTableLive(const MissionRuntimeState&,const MissionAssetCache&,
        std::uint32_t base,std::uint32_t stride,std::size_t capacity);
    MissionAnimationTableLive(const MissionAnimationTableLive&)=delete;
    MissionAnimationTableLive& operator=(const MissionAnimationTableLive&)=delete;
    std::uint32_t table_address(std::uint32_t animation_handle); // Null handle returns0.
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
private:
    std::string_view name(std::uint32_t handle) const;
    const MissionRuntimeState& runtime_;
    const MissionAssetCache& cache_;
    std::uint32_t base_,stride_;
    std::size_t capacity_;
    std::uint64_t end_;
    std::vector<std::uint32_t> handles_;
};
}
