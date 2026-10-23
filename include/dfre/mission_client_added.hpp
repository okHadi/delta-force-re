#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class MissionClientAddedError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct MissionClientAddedBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>& arguments)> call;
};
// 43F830. The current client pool and scratch memory share the supplied memory owner.
// Scratch covers the 90-byte-hex source local frame; all reached accesses need backing.
void mission_client_added(const MissionClientAddedBindings&,std::uint32_t scratch,std::uint32_t bytes);
} // namespace dfre
