// Line driven probe for the Cmprssh0 codec port.
// Modes:
//   c <hex>  compress -> "<bits> <hex bytes>"
//   e <hex>  expand   -> "<len> <hex bytes>" or "err"
//   t        dump the 257 code table entries -> "<symbol> <code> <bits>"
// Expand uses a 0x10000 byte output buffer, the size the game passes.

#include "dfre/compression.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kExpandBuffer = 0x10000;

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool parse_hex(const std::string& text, std::vector<std::uint8_t>& out) {
    if (text.size() % 2) {
        return false;
    }
    for (std::size_t at = 0; at < text.size(); at += 2) {
        const int high = hex_digit(text[at]);
        const int low = hex_digit(text[at + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

std::string to_hex(const std::uint8_t* bytes, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        text.push_back(digits[bytes[i] >> 4]);
        text.push_back(digits[bytes[i] & 15]);
    }
    return text;
}

int run_compress(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out(dfre::compress_bound(payload.size()));
    const int bits = dfre::compress_buffer(payload.data(), static_cast<int>(payload.size()), out.data());
    if (bits < 0) {
        std::cout << "err\n";
        return 0;
    }
    const std::size_t bytes = (static_cast<std::size_t>(bits) + 7) / 8;
    std::cout << bits << ' ' << to_hex(out.data(), bytes) << '\n';
    return 0;
}

int run_expand(const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out(kExpandBuffer);
    const int size = dfre::expand_buffer(payload.data(), payload.size(), out.data(), out.size());
    if (size < 0) {
        std::cout << "err\n";
        return 0;
    }
    std::cout << size << ' ' << (size == 0 ? "-" : to_hex(out.data(), static_cast<std::size_t>(size))) << '\n';
    return 0;
}

int run_table() {
    for (unsigned symbol = 0; symbol < 257; ++symbol) {
        const dfre::HuffmanCode code = dfre::symbol_code(symbol);
        std::cout << symbol << ' ' << std::hex << code.value << std::dec << ' '
                  << static_cast<unsigned>(code.bits) << '\n';
    }
    return 0;
}

} // namespace

int main() {
    dfre::init_compression();
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream fields(line);
        std::string mode;
        if (!(fields >> mode)) {
            continue;
        }
        if (mode == "t") {
            run_table();
            continue;
        }
        std::string payload_text;
        fields >> payload_text;
        std::vector<std::uint8_t> payload;
        if (!parse_hex(payload_text, payload)) {
            std::cerr << "Expected an even number of hex digits.\n";
            return 1;
        }
        if (mode == "c") {
            run_compress(payload);
        } else if (mode == "e") {
            run_expand(payload);
        } else {
            std::cerr << "Unknown mode: " << mode << '\n';
            return 1;
        }
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}
