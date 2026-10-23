#pragma once
#include "dfre/assets.hpp"
#include <functional>
#include <memory>

namespace dfre {
struct MissionNetsockPacketMemoryBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint8_t*(std::uint32_t address,std::size_t bytes)> resolve;
};
// Own only the fixed10000000..1001B000 DLL image. Keep the canonical foreign
// memory provider alive. All dynamic buffers and DF memory stay with that provider.
class MissionNetsockPacketMemory {
public:
    MissionNetsockPacketMemory(const AssetBytes& dll,MissionNetsockPacketMemoryBindings&);
    ~MissionNetsockPacketMemory();
    MissionNetsockPacketMemory(const MissionNetsockPacketMemory&)=delete;
    MissionNetsockPacketMemory& operator=(const MissionNetsockPacketMemory&)=delete;
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
    std::uint8_t* resolve(std::uint32_t address,std::size_t bytes);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
struct MissionNetsockPacketBindings : MissionNetsockPacketMemoryBindings {
    // Required imports and diagnostics. Indirect calls use the current raw target.
    std::function<std::uint32_t(std::uint32_t target,const std::vector<std::uint32_t>&)> call;
    // Map a source frame and its arguments. See the layouts below.
    std::function<std::uint32_t(std::uint32_t site,std::uint32_t bytes,
        const std::vector<std::uint32_t>& arguments)> frame;
    // Complete release without an error, including while another body fails.
    std::function<void(std::uint32_t site,std::uint32_t address,std::uint32_t bytes)> release;
    std::function<void(std::uint32_t target,const std::vector<std::uint32_t>&)> before_call;
};
// Raw entry frames include the return cell. Arguments start at+4.
std::uint32_t mission_netsock_get_message(const MissionNetsockPacketBindings&,
    std::uint32_t frame); //10001C80,10-byte hex frame
std::uint32_t mission_netsock_queue_pop(const MissionNetsockPacketBindings&,
    std::uint32_t frame); //10003C00,18-byte hex frame
std::uint32_t mission_netsock_player_name(const MissionNetsockPacketBindings&,
    std::uint32_t frame); //10001AA0,C-byte hex frame
// DF4B97A0 local frame:88-byte record, caller peer at+94; map98 bytes.
std::uint32_t mission_df_netsock_player_name(const MissionNetsockPacketBindings&,
    std::uint32_t frame);
// Nested frame layouts:10001BE0/10001C30 have local+0,args+8 (14 bytes);
//10003C00 args+4 (18 bytes);10001A20/10001AA0 args+4 (C bytes).
// Bind malloc/free and all dynamic storage through the one canonical host allocator.
} // namespace dfre
