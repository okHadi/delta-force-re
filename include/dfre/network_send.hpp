#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dfre {

// Outgoing datagram target, raw sockaddr_in fields. The wire bytes do not
// depend on the target; it is carried explicitly because both original send
// paths pass it to sendto.
struct SendTarget {
    std::uint32_t ip = 0;    // sin_addr, network byte order (original param_3)
    std::uint16_t port = 0;  // host-order port, htons'd into sin_port by the original
};

// Return codes of the original send paths. Socket errors (WSAGetLastError)
// are out of scope for this data-only API.
enum class SendStatus : std::uint32_t {
    Ok = 0,
    PayloadTooLarge = 0x2738,       // buffer_capacity - 4 < payload length
    CompressionUnavailable = 0x6a,  // selector only: DAT_10019860 == 0
};

// DAT_1001985c value used by every original emulation run so far. The runtime
// value comes from DLL initialization; the API takes it as a parameter.
inline constexpr std::size_t kSendBufferCapacity = 8192;

// Port of the extended wire encoder FUN_10005120 (0x10005120):
//   bytes 0..3  zero marker
//   bytes 4..7  packed low bits of payload[0..31], or CRC32 when < 32 bytes
//   byte 8      compressed flag (bit 0)
//   byte 9      header size, always 10
//   bytes 10..  payload; for payload >= 32 bytes the low bit of payload[i]
//               (i < 32) carries CRC bit i
struct ExtendedEncodeResult {
    SendStatus status = SendStatus::Ok;
    SendTarget target{};
    std::vector<std::uint8_t> datagram;  // empty when nothing is sent
    bool compressed_flag = false;
};

ExtendedEncodeResult encode_extended_datagram(const std::vector<std::uint8_t>& payload,
                                              SendTarget target = {},
                                              bool compressed_flag = false,
                                              std::size_t buffer_capacity = kSendBufferCapacity);

// Which original wire path ran.
enum class SendWireFormat : std::uint8_t { Simple, Extended };

// Port of the send selector FUN_10005860 (0x10005860): compress the payload,
// then send the simple 4-byte layout unless the extended length is smaller.
// `compression_available` mirrors DAT_10019860 (Cmprssh0.dll loaded).
struct SendSelectionResult {
    SendStatus status = SendStatus::Ok;
    SendWireFormat format = SendWireFormat::Simple;
    SendTarget target{};
    std::vector<std::uint8_t> datagram;  // empty when nothing is sent
    std::size_t compressed_bytes = 0;    // ceil(codec bits / 8), the compared length
    bool compressed_flag = false;        // extended header byte 8 bit 0
};

SendSelectionResult select_and_encode_send(const std::vector<std::uint8_t>& payload,
                                           SendTarget target = {},
                                           bool compression_available = true,
                                           std::size_t buffer_capacity = kSendBufferCapacity);

} // namespace dfre
