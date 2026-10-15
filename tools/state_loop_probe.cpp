#include "dfre/state_loop.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    std::string ticks_line, limits_line;
    while (std::getline(std::cin, ticks_line) && std::getline(std::cin, limits_line)) {
        try {
            std::istringstream tick_input(ticks_line), limit_input(limits_line);
            std::vector<std::uint32_t> ticks, limits;
            std::uint32_t value;
            while (tick_input >> value) ticks.push_back(value);
            while (limit_input >> value) limits.push_back(value);
            std::vector<dfre::GameState> states(limits.size());
            std::vector<std::uint32_t> calls(limits.size());
            dfre::StateLoop loop;
            std::ostringstream trace;
            std::size_t at = 0;
            for (std::size_t i = 0; i < states.size(); ++i) {
                states[i].enter = [&, i] { trace << 'e' << i << ','; };
                if (limits[i]) states[i].update = [&, i] {
                    trace << 'u' << i << ',';
                    return ++calls[i] == limits[i] ? 1 : 0;
                };
                states[i].render = [&, i] { trace << 'r' << i << ','; };
                states[i].leave = [&, i] {
                    trace << 'l' << i << ',';
                    loop.request(i + 1 < states.size() ? &states[i + 1] : nullptr);
                };
            }
            loop.run(states.empty() ? nullptr : &states[0], {[&] {
                if (at >= ticks.size()) throw std::runtime_error("Clock exhausted");
                return ticks[at++];
            }, [&] { trace << "y,"; }});
            std::cout << trace.str() << ' ' << loop.elapsed() << ' ' << at << '\n';
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }
}
