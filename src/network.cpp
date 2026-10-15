#include "dfre/network.hpp"

#include <array>

namespace dfre {
namespace {

constexpr std::array<std::uint32_t, 256> make_crc_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t index = 0; index < 256; ++index) {
        std::uint32_t value = index << 24;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value << 1) ^ ((value & 0x80000000u) ? 0x04c11db7u : 0u);
        }
        table[index] = value;
    }
    return table;
}

// Same table as the original at 0x10007234.
constexpr std::array<std::uint32_t, 256> kCrcTable = make_crc_table();

std::uint32_t read_le32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void write_le32(std::uint8_t* bytes, std::uint32_t value) {
    bytes[0] = static_cast<std::uint8_t>(value);
    bytes[1] = static_cast<std::uint8_t>(value >> 8);
    bytes[2] = static_cast<std::uint8_t>(value >> 16);
    bytes[3] = static_cast<std::uint8_t>(value >> 24);
}

std::uint32_t byte_swap32(std::uint32_t value) {
    return (value >> 24) | ((value >> 8) & 0x0000ff00u) | ((value << 8) & 0x00ff0000u) |
           (value << 24);
}

std::uint16_t byte_swap16(std::uint16_t value) {
    return static_cast<std::uint16_t>((value >> 8) | (value << 8));
}

std::uint32_t sweep_crc(std::uint32_t crc, std::uint8_t byte) {
    return (crc << 8) ^ kCrcTable[(crc >> 24) ^ byte];
}

// One pass of the original bit unscramble: take the low bit of each of the next
// 32 payload bytes, then toggle those low bits with the seed bits.
std::uint32_t unscramble_bits(std::uint8_t* bytes, std::uint32_t seed) {
    std::uint32_t extracted = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        std::uint8_t& byte = bytes[bit];
        extracted |= static_cast<std::uint32_t>(byte & 1u) << bit;
        byte = static_cast<std::uint8_t>(((byte ^ static_cast<std::uint8_t>(seed)) & 1u) ^ byte);
        seed >>= 1;
    }
    return extracted;
}

} // namespace

std::uint32_t crc32_mpeg2(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t index = 0; index < size; ++index) {
        crc = sweep_crc(crc, data[index]);
    }
    return crc;
}

std::vector<std::uint8_t> encode_direct_datagram(const std::vector<std::uint8_t>& payload) {
    if (payload.empty()) {
        return {};
    }
    const std::uint32_t crc = crc32_mpeg2(payload.data(), payload.size());
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
    write_le32(result.data(), prefix);
    return result;
}

bool source_address_blocked(std::uint32_t ip_network_order, const std::vector<IpFilterRange>& ranges) {
    if (ranges.empty()) {
        return false;
    }
    const std::uint32_t ip = byte_swap32(ip_network_order);  // ntohl
    int low = 0;
    int high = static_cast<int>(ranges.size());
    for (;;) {
        const int upper = high - 1;  // fixed for the inner search, like the original
        int mid = 0;
        for (;;) {
            if (upper < low) {
                return false;
            }
            mid = (upper + low) >> 1;
            const IpFilterRange& range = ranges[static_cast<std::size_t>(mid)];
            if (ip <= static_cast<std::uint32_t>(range.start + range.span)) {
                break;
            }
            low = mid + 1;
        }
        if (ip >= ranges[static_cast<std::size_t>(mid)].start) {
            return true;
        }
        high = mid;
    }
}

ReceiveDecodeResult decode_received_datagram(const std::vector<std::uint8_t>& datagram,
                                             std::size_t capacity,
                                             DatagramSource source,
                                             const std::vector<IpFilterRange>& blocked_ranges,
                                             const Expander& expander) {
    ReceiveDecodeResult result;
    if (source_address_blocked(source.ip, blocked_ranges)) {
        result.status = ReceiveStatus::SourceBlocked;
        return result;
    }
    constexpr std::size_t kSimpleHeader = 4;
    const std::size_t received = datagram.size();
    // Malformed short packets read stale socket-buffer bytes in the original.
    // The port fails closed instead; see docs/network-runtime.md.
    if (received < kSimpleHeader) {
        result.status = ReceiveStatus::CrcFailed;
        return result;
    }
    std::vector<std::uint8_t> buffer(datagram);
    std::size_t header = kSimpleHeader;
    if (read_le32(buffer.data()) == 0) {
        result.extended_header = true;
        if (received < 10) {
            result.status = ReceiveStatus::CrcFailed;
            return result;
        }
        header = buffer[9];
    }
    if (received <= header) {
        result.status = ReceiveStatus::CrcFailed;
        return result;
    }
    std::size_t decoded_size = received - header;
    if (capacity < decoded_size) {
        result.status = ReceiveStatus::BufferTooSmall;
        return result;
    }
    if (decoded_size >= 32) {
        const std::size_t seed_at = (header == kSimpleHeader) ? 0 : 4;
        const std::uint32_t extracted = unscramble_bits(buffer.data() + header, read_le32(buffer.data() + seed_at));
        write_le32(buffer.data() + seed_at, extracted);
    }
    std::vector<std::uint8_t> output;
    output.reserve(decoded_size);
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t index = 0; index < decoded_size; ++index) {
        const std::uint8_t byte = buffer[header + index];
        output.push_back(byte);
        crc = sweep_crc(crc, byte);
    }
    const std::size_t expected_at = (header == kSimpleHeader) ? 0 : 4;
    bool verified = crc == read_le32(buffer.data() + expected_at);
    if (!verified && header > kSimpleHeader && decoded_size >= 32) {
        // Second attempt of the original for extended headers (0x100054a1).
        const std::uint32_t extracted = unscramble_bits(buffer.data() + header, read_le32(buffer.data() + 4));
        write_le32(buffer.data() + 4, extracted);
        std::uint32_t packed = 0;
        for (unsigned bit = 0; bit < 32; ++bit) {
            std::uint8_t& byte = buffer[4 + bit];
            packed |= static_cast<std::uint32_t>(byte & 1u) << bit;
            byte = static_cast<std::uint8_t>(byte & 0xfeu);
        }
        write_le32(buffer.data(), packed);
        output.assign(buffer.begin() + 4, buffer.end());
        crc = 0xffffffffu;
        for (const std::uint8_t byte : output) {
            crc = sweep_crc(crc, byte);
        }
        verified = crc == read_le32(buffer.data());
        if (verified) {
            header = kSimpleHeader;
            decoded_size = received - kSimpleHeader;
        }
    }
    result.payload = std::move(output);
    if (!verified) {
        result.status = ReceiveStatus::CrcFailed;
        return result;
    }
    result.reported_size = static_cast<std::uint32_t>(decoded_size);
    const bool compressed = header > kSimpleHeader && (buffer[8] & 1u) != 0;
    result.compressed_flag = compressed;
    if (compressed) {
        if (!expander) {
            result.status = ReceiveStatus::CompressionUnavailable;
            result.reported_size = 0;
            return result;
        }
        const std::optional<std::vector<std::uint8_t>> expanded =
            expander(result.payload.data(), result.payload.size(), 0x10000u);
        if (!expanded) {
            // Port extension: the original never checks the expander result.
            result.status = ReceiveStatus::CompressionUnavailable;
            result.payload.clear();
            result.reported_size = 0;
            return result;
        }
        if (expanded->size() > capacity) {
            // Port safety guard: the original copies the expanded bytes unchecked.
            result.status = ReceiveStatus::BufferTooSmall;
            result.payload.clear();
            result.reported_size = 0;
            return result;
        }
        // The original copies the expanded bytes over the decoded payload, so any
        // bytes beyond the expanded size keep the earlier copy.
        std::vector<std::uint8_t> expanded_payload = *expanded;
        if (expanded_payload.size() < result.payload.size()) {
            expanded_payload.insert(expanded_payload.end(),
                                    result.payload.begin() +
                                        static_cast<std::ptrdiff_t>(expanded_payload.size()),
                                    result.payload.end());
        }
        result.payload = std::move(expanded_payload);
        result.reported_size = static_cast<std::uint32_t>(expanded->size());
        result.decompressed = true;
    }
    result.status = ReceiveStatus::Ok;
    result.source = source;
    result.source_port_host = byte_swap16(source.port);
    return result;
}

} // namespace dfre
