#include "dfre/lobby.hpp"

#include <cstring>
#include <iostream>
#include <string>

namespace {
int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool parse_hex(const std::string& text, std::uint8_t* bytes, std::size_t size) {
    if (text.size() != size * 2) return false;
    for (std::size_t at = 0; at < size; ++at) {
        const int high = hex_digit(text[at * 2]);
        const int low = hex_digit(text[at * 2 + 1]);
        if (high < 0 || low < 0) return false;
        bytes[at] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

void print_record(const dfre::MultiplayerSettings& settings) {
    constexpr char digits[] = "0123456789abcdef";
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&settings);
    for (std::size_t at = 0; at < sizeof(settings); ++at) {
        std::cout << digits[bytes[at] >> 4] << digits[bytes[at] & 15];
    }
    std::cout << '\n';
}

void print_lines(const dfre::MultiplayerSettings& settings) {
    std::cout << dfre::multiplayer_config_lines(settings);
    std::cout << ".\n";
}
} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "defaults") {
            print_record(dfre::multiplayer_defaults());
        } else if (line == "defaults-render") {
            print_lines(dfre::multiplayer_defaults());
        } else if (line.rfind("render-hex ", 0) == 0) {
            dfre::MultiplayerSettings settings;
            if (!parse_hex(line.substr(11), reinterpret_cast<std::uint8_t*>(&settings),
                           sizeof(settings))) {
                std::cerr << "Expected " << sizeof(settings) * 2 << " hex digits for a record.\n";
                return 1;
            }
            print_lines(settings);
        } else {
            std::cerr << "Unknown command.\n";
            return 1;
        }
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}
