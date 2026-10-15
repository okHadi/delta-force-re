#include "dfre/state_loop.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}

struct FakeClock {
    std::vector<std::uint32_t> ticks;
    std::size_t at = 0;
    std::string& trace;
    dfre::StateClock clock() {
        return {[this] {
            if (at == ticks.size()) throw std::runtime_error("Unexpected clock read");
            return ticks[at++];
        }, [this] { trace += 'y'; }};
    }
};

void transition_and_budget() {
    dfre::StateLoop loop;
    std::string trace;
    dfre::GameState first, second, alternate;
    int updates = 0;
    first.enter = [&] { trace += 'a'; };
    first.update = [&] {
        trace += 'u';
        if (++updates == 3) { loop.request(&alternate); return 1; }
        return 0;
    };
    first.render = [&] { trace += 'r'; };
    first.leave = [&] { trace += 'l'; loop.request(&second); };
    second.enter = [&] { trace += 'b'; };
    second.update = [&] { trace += 'v'; return 1; };
    second.render = [&] { throw std::runtime_error("Render after exit"); };
    second.leave = [&] { trace += 'm'; };
    alternate.enter = [&] { throw std::runtime_error("Leave did not override next state"); };
    FakeClock clock{{100, 100, 101, 115, 116, 148, 500, 500}, 0, trace};
    loop.run(&first, clock.clock());
    check(trace == "auryyurulbvm", "Callback order or carried budget differs");
    check(loop.elapsed() == 33, "Elapsed total includes idle ticks");
    check(loop.current() == nullptr, "Current state remains set after return");
    check(clock.at == clock.ticks.size(), "Unexpected clock count");
}

void wrap_and_backlog() {
    dfre::StateLoop loop;
    std::string trace;
    dfre::GameState state;
    int updates = 0;
    state.update = [&] { trace += 'u'; return ++updates == 4 ? 1 : 0; };
    state.render = [&] { trace += 'r'; };
    FakeClock clock{{0xfffffff0u, 0x10, 0x20}, 0, trace};
    loop.run(&state, clock.clock());
    check(trace == "uuuru", "Tick wrap or backlog updates differ");
    check(loop.elapsed() == 48, "Tick wrap elapsed differs");
}

void no_update_and_empty() {
    dfre::StateLoop loop;
    std::string trace;
    dfre::GameState state;
    state.enter = [&] { trace += 'a'; };
    state.render = [&] { throw std::runtime_error("Render with no update"); };
    state.leave = [&] { trace += 'l'; };
    FakeClock clock{{1, 2}, 0, trace};
    loop.run(&state, clock.clock());
    loop.run(nullptr, clock.clock());
    check(trace == "al" && clock.at == 2 && loop.elapsed() == 0,
          "No-update state differs");
}
} // namespace

int main() {
    try {
        transition_and_budget();
        wrap_and_backlog();
        no_update_and_empty();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
