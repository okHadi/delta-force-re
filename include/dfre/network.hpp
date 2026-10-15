#pragma once

#include <cstdint>
#include <vector>

namespace dfre {

// Encode payload bytes using the direct-datagram transform. Empty input returns empty.
std::vector<std::uint8_t> encode_direct_datagram(const std::vector<std::uint8_t>& payload);

} // namespace dfre
