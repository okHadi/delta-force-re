#pragma once
#include "dfre/mission_df_socket.hpp"
#include <memory>
namespace dfre {
struct MissionDfSocketRuntimeMemory {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
};
struct MissionDfSocketRuntimeOptions {
    bool allow_external_dns=false;
    std::uint32_t allocation_limit=64u*1024u*1024u;
    std::uint8_t new_memory_byte=0;
};
// Keep the canonical memory provider alive. Call bindings on the construction thread.
class MissionDfSocketRuntime {
public:
    explicit MissionDfSocketRuntime(MissionDfSocketRuntimeMemory&,
        MissionDfSocketRuntimeOptions={});
    ~MissionDfSocketRuntime();
    MissionDfSocketRuntime(const MissionDfSocketRuntime&)=delete;
    MissionDfSocketRuntime& operator=(const MissionDfSocketRuntime&)=delete;
    MissionDfSocketBindings bindings();
    // Route every caller through these providers for owned source address ranges.
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
    std::uint16_t local_port(std::uint32_t source_socket) const;
    std::size_t socket_count() const;
    std::size_t frame_depth() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
