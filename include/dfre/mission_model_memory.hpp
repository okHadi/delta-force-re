#pragma once
#include "dfre/assets.hpp"
#include "dfre/models.hpp"
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
namespace dfre {
class MissionModelMemoryError:public std::runtime_error {public:using std::runtime_error::runtime_error;};
struct MissionModelCallbackSeed {
    enum class Phase { pre_producer, post_initialized };
    Phase phase;
    std::array<std::uint32_t,5> words;
    static MissionModelCallbackSeed pre_producer();
    static MissionModelCallbackSeed post_initialized(std::array<std::uint32_t,5>);
};
struct MissionModelMemoryBinding {
    std::uint32_t handle=0;
    std::string name;
    std::optional<MissionModelCallbackSeed> callbacks;
};
// Keep the cache and both providers alive. Source files validate the current
// cached data for both phases; only pre_producer uses their callback seed bytes.
class MissionModelMemory {
public:
    using ModelProvider=std::function<const Model&(std::string_view)>;
    using FileProvider=std::function<AssetBytes(std::string_view)>;
    MissionModelMemory(ModelProvider,FileProvider,std::vector<MissionModelMemoryBinding>);
    ~MissionModelMemory();
    MissionModelMemory(const MissionModelMemory&)=delete;
    MissionModelMemory& operator=(const MissionModelMemory&)=delete;
    std::uint32_t source(std::uint32_t handle) const;
    std::uint32_t handle(std::uint32_t source) const;
    const Model& model(std::uint32_t source) const;
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::optional<std::uint32_t> record_base(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
