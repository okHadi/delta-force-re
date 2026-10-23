#pragma once
#include "dfre/mission_line_file.hpp"
#include <filesystem>
#include <memory>
#include <optional>
namespace dfre {
//49A550 for a current FILE image. Supported count is2..INT_MAX.
std::uint32_t mission_line_file_read(const MissionLineFileBindings&,
    std::uint32_t target,std::uint32_t count,std::uint32_t file);
struct MissionLineFileRuntimeBindings {
    // Bind the host's real recursive FILE lock for reached reads.
    std::function<void(std::uint32_t descriptor)> lock;
    std::function<void(std::uint32_t descriptor)> unlock;
};
class MissionLineFileRuntime {
public:
    MissionLineFileRuntime(const std::filesystem::path& game_folder,
        const MissionSoundAssetBindings&,std::uint32_t scratch,std::uint32_t bytes,
        MissionLineFileRuntimeBindings={});
    MissionLineFileRuntime(const std::filesystem::path&,const MissionSoundAssetBindings&&,
        std::uint32_t,std::uint32_t,MissionLineFileRuntimeBindings={})=delete;
    ~MissionLineFileRuntime();
    MissionLineFileRuntime(const MissionLineFileRuntime&)=delete;
    MissionLineFileRuntime& operator=(const MissionLineFileRuntime&)=delete;
    std::optional<std::uint32_t> service(std::uint32_t site,const std::vector<std::uint32_t>&);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
