#include "dfre/network_send.hpp"

#include <algorithm>
#include <limits>

#include "dfre/compression.hpp"
#include "dfre/network.hpp"

namespace dfre {
namespace {

void write_le32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[at + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

bool exceeds_limit(std::size_t size, std::size_t buffer_capacity) {
    return buffer_capacity < 4 || size > buffer_capacity - 4;
}

} // namespace

ExtendedEncodeResult encode_extended_datagram(const std::vector<std::uint8_t>& payload,
                                              SendTarget target,
                                              bool compressed_flag,
                                              std::size_t buffer_capacity) {
    ExtendedEncodeResult result;
    result.target = target;
    if (payload.empty()) {
        return result;  // original returns 0 without sending
    }
    if (exceeds_limit(payload.size(), buffer_capacity)) {
        result.status = SendStatus::PayloadTooLarge;
        return result;
    }
    result.compressed_flag = compressed_flag;
    result.datagram.assign(10 + payload.size(), 0);
    const std::uint32_t crc = crc32_mpeg2(payload.data(), payload.size());
    std::copy(payload.begin(), payload.end(), result.datagram.begin() + 10);
    std::uint32_t packed = crc;
    if (payload.size() >= 32) {
        packed = 0;
        for (unsigned bit = 0; bit < 32; ++bit) {
            packed |= static_cast<std::uint32_t>(payload[bit] & 1u) << bit;
            result.datagram[10 + bit] =
                static_cast<std::uint8_t>((payload[bit] & 0xfeu) | ((crc >> bit) & 1u));
        }
    }
    write_le32(result.datagram, 4, packed);
    result.datagram[8] = compressed_flag ? 1 : 0;
    result.datagram[9] = 10;
    return result;
}

SendSelectionResult select_and_encode_send(const std::vector<std::uint8_t>& payload,
                                           SendTarget target,
                                           bool compression_available,
                                           std::size_t buffer_capacity) {
    SendSelectionResult result;
    result.target = target;
    if (!compression_available) {
        result.status = SendStatus::CompressionUnavailable;
        return result;
    }
    if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        // Port guard: the original takes an int length.
        result.status = SendStatus::PayloadTooLarge;
        return result;
    }
    std::vector<std::uint8_t> compressed(compress_bound(payload.size()));
    const int bits = compress_buffer(payload.data(), static_cast<int>(payload.size()), compressed.data());
    const std::uint32_t raw_bits = static_cast<std::uint32_t>(bits);
    std::size_t wire = raw_bits >> 3;
    if ((raw_bits & 7u) != 0) {
        ++wire;
    }
    result.compressed_bytes = wire;
    if (payload.size() + 4u <= wire + 10u) {
        result.format = SendWireFormat::Simple;
        if (exceeds_limit(payload.size(), buffer_capacity)) {
            result.status = SendStatus::PayloadTooLarge;
            return result;
        }
        result.datagram = encode_direct_datagram(payload);
        return result;
    }
    result.format = SendWireFormat::Extended;
    const std::size_t available = std::min(wire, compressed.size());
    const std::vector<std::uint8_t> compressed_payload(compressed.begin(),
                                                       compressed.begin() + static_cast<std::ptrdiff_t>(available));
    const ExtendedEncodeResult extended =
        encode_extended_datagram(compressed_payload, target, true, buffer_capacity);
    result.status = extended.status;
    result.datagram = extended.datagram;
    result.compressed_flag = extended.compressed_flag;
    return result;
}

} // namespace dfre
