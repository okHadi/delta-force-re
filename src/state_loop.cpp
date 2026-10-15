#include "dfre/state_loop.hpp"

#include <stdexcept>

namespace dfre {

void StateLoop::run(GameState* initial, const StateClock& clock) {
    if (!clock.now || !clock.yield) throw std::invalid_argument("State clock is incomplete");
    std::uint32_t budget = 16;
    auto* state = initial;
    for (;;) {
        current_ = state;
        next_ = nullptr;
        if (!current_) return;
        if (current_->enter) current_->enter();
        auto previous = clock.now();
        for (;;) {
            const auto now = clock.now();
            const std::uint32_t delta = now - previous;
            budget += delta;
            if (!current_->update) break;
            if (budget < 16) {
                clock.yield();
                previous = now;
                continue;
            }
            int result = 0;
            while (budget >= 16) {
                result = current_->update();
                if (result != 0) break;
                budget -= 16;
            }
            if (!result && current_->render) current_->render();
            elapsed_ += delta;
            previous = now;
            if (result != 0) break;
        }
        if (current_->leave) current_->leave();
        state = next_;
    }
}

} // namespace dfre
