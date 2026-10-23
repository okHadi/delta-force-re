#pragma once
#include "dfre/profiles.hpp"
#include <functional>
#include <optional>
namespace dfre {
// Keep the ProfileStore and selection owner alive. This view owns no profile.
class MissionProfileMemory {
public:
    MissionProfileMemory(std::function<AssetBytes&(std::size_t)>,std::function<std::size_t()>);
    std::uint32_t source(std::size_t slot) const;
    std::optional<std::uint32_t> record_base(std::uint32_t address,std::uint32_t width) const;
    bool claims(std::uint32_t address,std::uint32_t width) const;
    std::uint32_t read(std::uint32_t address,std::uint32_t width) const;
    void write(std::uint32_t address,std::uint32_t width,std::uint32_t value);
private:
    AssetBytes& current(std::size_t) const;
    std::function<AssetBytes&(std::size_t)> profile_;
    std::function<std::size_t()> selected_;
};
}
