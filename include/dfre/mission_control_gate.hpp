#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
namespace dfre {
class MissionControlGateError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct MissionControlGateBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
};
// 4718D0. Read current mode/host flags before the signed delay calculation.
std::int32_t mission_control_gate(const MissionControlGateBindings&);
}
