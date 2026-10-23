#pragma once
#include <cstdint>
#include <functional>
namespace dfre {
inline constexpr std::uint32_t crt_random_initial_seed=1; //4C19FC: current thread block+0C
struct CrtRandomBindings {
    std::function<std::uint32_t*()> current_seed; //49AF60; resolve on each call
};
std::uint32_t crt_random_next(const CrtRandomBindings&); //49AF6C
void crt_random_seed(const CrtRandomBindings&,std::uint32_t); //49AF90
} // namespace dfre
