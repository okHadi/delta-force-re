#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace dfre {

// Encode payload bytes using the direct-datagram transform. Empty input returns empty.
// Port of FUN_10005020 (Netsocknl.dll 0x10005020).
std::vector<std::uint8_t> encode_direct_datagram(const std::vector<std::uint8_t>& payload);

// CRC-32 over payload bytes: poly 0x04c11db7, MSB first, init 0xffffffff, no final xor.
// Matches the receive table at 0x10007234 and the send encoder.
std::uint32_t crc32_mpeg2(const std::uint8_t* data, std::size_t size);

// Return codes of the original receive routine FUN_100052e0 (0x100052e0).
enum class ReceiveStatus : std::uint32_t {
    Ok = 0,
    SourceBlocked = 0x66,           // source IP matched a block range (FUN_10005280)
    CompressionUnavailable = 0x6a,  // compressed packet but no expander is loaded
    CrcFailed = 0x271e,             // CRC mismatch, empty payload or malformed packet
    BufferTooSmall = 0x2738,        // destination capacity smaller than the payload
};

// recvfrom error the original treats as an empty poll (WSAEWOULDBLOCK).
inline constexpr std::uint32_t kWsaWouldBlock = 0x2733;

// One blocked source range in host byte order. Hit: start <= ip <= start + span.
struct IpFilterRange {
    std::uint32_t start = 0;
    std::uint32_t span = 0;
};

// Port of FUN_10005280 (0x10005280): true means the packet is dropped.
// Empty ranges mean the filter is off (DAT_100096b8 == 0).
bool source_address_blocked(std::uint32_t ip_network_order, const std::vector<IpFilterRange>& ranges);

// Source address of one received UDP datagram: raw sockaddr_in fields.
struct DatagramSource {
    std::uint32_t ip = 0;    // sin_addr, network byte order
    std::uint16_t port = 0;  // sin_port, network byte order
};

// Decoded result of one datagram. `payload` holds the bytes the original writes
// into the destination buffer (it may be partial when status is not Ok).
// `reported_size` mirrors the original length output, zero unless status is Ok.
struct ReceiveDecodeResult {
    ReceiveStatus status = ReceiveStatus::Ok;
    DatagramSource source{};             // zeroed on failure
    std::uint16_t source_port_host = 0;  // htons(sin_port), zeroed on failure
    std::vector<std::uint8_t> payload;
    std::uint32_t reported_size = 0;
    bool extended_header = false;  // packet used the size-at-byte-9 header
    bool compressed_flag = false;  // extended header byte 8 bit 0
    bool decompressed = false;     // expander ran
};

// Bounded adapter for the original two-argument CMPRSS??.DLL expander:
// receives the decoded compressed bytes and returns the expanded bytes.
// Returning nullopt is treated like a missing expander (port extension).
using Expander = std::function<std::optional<std::vector<std::uint8_t>>(
    const std::uint8_t* compressed, std::size_t size, std::size_t capacity)>;

// Decode one received UDP datagram the way FUN_100052e0 does after recvfrom.
// `capacity` is the destination size the original checks before copying.
// `blocked_ranges` empty means the source filter is off.
// `expander` empty means compression is not loaded (DAT_10019860 == 0).
ReceiveDecodeResult decode_received_datagram(const std::vector<std::uint8_t>& datagram,
                                             std::size_t capacity,
                                             DatagramSource source,
                                             const std::vector<IpFilterRange>& blocked_ranges = {},
                                             const Expander& expander = {});

} // namespace dfre
