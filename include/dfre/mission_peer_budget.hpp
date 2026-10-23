#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace dfre {
class MissionPeerBudgetError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct MissionPeerBudgetBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
};
// 444E30. Client base6E9A90,stride9B84,count17. Preserve shared actor storage.
void mission_peer_budget_update(const MissionPeerBudgetBindings&);
} // namespace dfre
