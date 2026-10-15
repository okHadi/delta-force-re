#pragma once

#include <cstdint>
#include <functional>

namespace dfre {

struct GameState {
    std::function<void()> enter;
    std::function<int()> update;
    std::function<void()> render;
    std::function<void()> leave;
};

struct StateClock {
    std::function<std::uint32_t()> now;
    std::function<void()> yield;
};

class StateLoop {
public:
    // States and their callbacks must remain valid until run() returns.
    void run(GameState* initial, const StateClock& clock);
    void request(GameState* next) noexcept { next_ = next; }
    GameState* current() const noexcept { return current_; }
    std::uint32_t elapsed() const noexcept { return elapsed_; }

private:
    GameState* current_ = nullptr;
    GameState* next_ = nullptr;
    std::uint32_t elapsed_ = 0;
};

} // namespace dfre
