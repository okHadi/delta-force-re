#include "dfre/network.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool require(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

std::uint32_t little_word(const std::vector<std::uint8_t>& bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint32_t embedded_crc(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t value = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        value |= static_cast<std::uint32_t>(bytes[4 + bit] & 1u) << bit;
    }
    return value;
}
} // namespace

int main() {
    if (!require(dfre::encode_direct_datagram({}).empty(), "Empty payload must produce no datagram.")) return 1;
    if (!require(dfre::encode_direct_datagram({0xff}) ==
                 std::vector<std::uint8_t>({0x00, 0xff, 0xff, 0xff, 0xff}),
                 "One-byte CRC prefix order failed.")) return 1;
    const std::string text = "123456789";
    const std::vector<std::uint8_t> check(text.begin(), text.end());
    auto encoded = dfre::encode_direct_datagram(check);
    if (!require(little_word(encoded) == 0x0376e6e7u, "CRC/MPEG-2 check vector failed.")) return 1;
    if (!require(std::vector<std::uint8_t>(encoded.begin() + 4, encoded.end()) == check,
                 "Short payload bytes must stay unchanged.")) return 1;

    // Expected CRC values come from separate polynomial long division over GF(2).
    const std::uint32_t zero_crc[] = {0xf2ca6f69u, 0x4a55af67u, 0x4ea3c1a1u};
    for (std::size_t size = 31; size <= 33; ++size) {
        const std::vector<std::uint8_t> payload(size, 0);
        encoded = dfre::encode_direct_datagram(payload);
        if (!require(encoded.size() == size + 4, "Datagram size failed.")) return 1;
        if (size == 31) {
            if (!require(little_word(encoded) == zero_crc[0], "31-byte CRC prefix failed.")) return 1;
            if (!require(std::vector<std::uint8_t>(encoded.begin() + 4, encoded.end()) == payload,
                         "31-byte payload changed.")) return 1;
        } else {
            if (!require(little_word(encoded) == 0, "Zero low-bit prefix failed.")) return 1;
            if (!require(embedded_crc(encoded) == zero_crc[size - 31], "Embedded CRC failed.")) return 1;
            if (size == 33 && !require(encoded.back() == 0, "33rd payload byte changed.")) return 1;
        }
    }
    std::vector<std::uint8_t> payload;
    for (unsigned value = 0; value < 33; ++value) payload.push_back(static_cast<std::uint8_t>(value));
    const auto original = payload;
    encoded = dfre::encode_direct_datagram(payload);
    if (!require(little_word(encoded) == 0xaaaaaaaau, "Saved alternating low bits failed.")) return 1;
    if (!require(embedded_crc(encoded) == 0x48f9bbb3u, "33-byte check vector failed.")) return 1;
    for (std::size_t byte = 0; byte < 32; ++byte) {
        if (!require((encoded[4 + byte] & 0xfeu) == (payload[byte] & 0xfeu), "Upper payload bits changed.")) return 1;
    }
    if (!require(encoded.back() == 32, "Payload tail changed.")) return 1;
    if (!require(payload == original, "Input payload changed.")) return 1;
    payload.assign(32, 0xa4);
    for (unsigned bit = 0; bit < 32; ++bit) payload[bit] |= (0x12345678u >> bit) & 1u;
    encoded = dfre::encode_direct_datagram(payload);
    if (!require(std::vector<std::uint8_t>(encoded.begin(), encoded.begin() + 4) ==
                 std::vector<std::uint8_t>({0x78, 0x56, 0x34, 0x12}),
                 "Long-payload prefix byte order failed.")) return 1;
    return 0;
}
