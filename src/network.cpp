#include "dfre/network.hpp"

namespace dfre {

std::vector<std::uint8_t> encode_direct_datagram(const std::vector<std::uint8_t>& payload) {
    if (payload.empty()) {
        return {};
    }
    std::uint32_t crc = 0xffffffffu;
    for (const auto byte : payload) {
        crc ^= static_cast<std::uint32_t>(byte) << 24;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc << 1) ^ ((crc & 0x80000000u) ? 0x04c11db7u : 0u);
        }
    }
    std::vector<std::uint8_t> result(4, 0);
    result.insert(result.end(), payload.begin(), payload.end());
    std::uint32_t prefix = crc;
    if (payload.size() >= 32) {
        prefix = 0;
        for (unsigned bit = 0; bit < 32; ++bit) {
            prefix |= static_cast<std::uint32_t>(payload[bit] & 1u) << bit;
            result[4 + bit] = static_cast<std::uint8_t>((payload[bit] & 0xfeu) | ((crc >> bit) & 1u));
        }
    }
    for (unsigned byte = 0; byte < 4; ++byte) {
        result[byte] = static_cast<std::uint8_t>(prefix >> (8 * byte));
    }
    return result;
}

} // namespace dfre
