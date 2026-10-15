#include "dfre/network.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {
int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}
} // namespace

int main() {
    constexpr char digits[] = "0123456789abcdef";
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.size() % 2) {
            std::cerr << "Expected an even number of hex digits.\n";
            return 1;
        }
        std::vector<std::uint8_t> payload;
        payload.reserve(line.size() / 2);
        for (std::size_t at = 0; at < line.size(); at += 2) {
            const int high = hex_digit(line[at]);
            const int low = hex_digit(line[at + 1]);
            if (high < 0 || low < 0) {
                std::cerr << "Expected hex digits.\n";
                return 1;
            }
            payload.push_back(static_cast<std::uint8_t>((high << 4) | low));
        }
        for (const auto byte : dfre::encode_direct_datagram(payload)) {
            std::cout << digits[byte >> 4] << digits[byte & 15];
        }
        std::cout << '\n';
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}
