#pragma once
#include "dfre/mission_tick.hpp"

namespace dfre {
struct MissionProfilerCells {
    std::uint32_t current_4f8718,previous_4f871c;
    std::vector<std::uint32_t> totals_8e3f40;
};
struct MissionCameraCells {
    Fixed3& position_6ba3d0;
    std::array<std::uint32_t,3> delta_6ba3e0_e4_dc;
    std::uint32_t yaw_6ba3e8,actor_6ba3f0;
};
struct MissionScriptQueueEntry {
    std::uint32_t resource,index,delay,armed;
};
struct MissionScriptQueueCells {
    std::uint32_t scale_8e3d64,enabled_8e3d68;
    std::vector<MissionScriptQueueEntry> entries_8e3d74;
};
struct MissionFrameServiceCells {
    std::uint32_t& requested_8ee1d4;
    MissionProfilerCells& profiler;
    std::array<std::uint32_t,3>& timers_bd4814;
    std::uint32_t& stat_bcfc24;
    MissionCameraCells& camera;
    MissionScriptQueueCells& scripts;
};
struct MissionFrameServiceBindings {
    std::function<std::uint64_t()> rdtsc;
    // Read the four actor words cached at 42AB79 before camera writes.
    std::function<std::uint32_t(std::uint32_t actor,std::uint32_t offset)> actor_word;
    std::function<std::uint32_t(std::uint32_t resource)> script_count;
    std::function<std::uint8_t(std::uint32_t resource,std::uint32_t index)> script_delay;
    // 456110(resource,index), 447C60(resource+4,index), 456000(resource+4), 455B40().
    std::function<void(std::uint32_t site,std::uint32_t resource,std::uint32_t index)> script_call;
    // 4702A0 writes its output before resetting the shared statistic.
    std::function<void(std::uint32_t target,std::uint32_t value)> store_word;
};
class MissionFrameServices {
public:
    MissionFrameServices(MissionFrameServiceCells,MissionFrameServiceBindings);
    std::optional<std::uint32_t> stage(const MissionStageCall&);
    void profile(std::uint32_t next);
    void camera();
    void scripts(std::uint32_t pulse);
private:
    MissionFrameServiceCells cells_;
    MissionFrameServiceBindings bindings_;
};
} // namespace dfre
