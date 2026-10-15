#include "dfre/network.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

bool require(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::vector<std::uint8_t> from_hex(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    if (text.size() % 2) {
        std::cerr << "Bad hex length: " << text << '\n';
        return {};
    }
    for (std::size_t at = 0; at < text.size(); at += 2) {
        const int high = hex_digit(text[at]);
        const int low = hex_digit(text[at + 1]);
        if (high < 0 || low < 0) {
            std::cerr << "Bad hex: " << text << '\n';
            return {};
        }
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
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

// Vectors frozen from the original Netsocknl.dll receive routine FUN_100052e0
// through tools/check_network_receive_original.py (Unicorn, socket stubs).
// Every field below is the original output for source 127.0.0.1:0x1234.
struct ReceiveVector {
    const char* name;
    const char* packet;
    std::size_t capacity;
    const char* expander;  // empty = no expander loaded
    std::uint32_t status;
    std::uint16_t host_port;
    std::uint32_t size;
    const char* payload;  // bytes the original writes into the destination
    bool extended;
    bool compressed;
    bool decompressed;
};

const ReceiveVector kReceiveVectors[] = {
    {"simple-40",
     "52500ea1696ca9bf838f888a2e84305ea55402df668d85ce1ac13675b66ae6e0e5d92ccc4195a67aac632d6d",
     8192, "", 0, 0x3412, 40,
     "686da8be838e898a2e84305ea55403de668d85cf1ac03674b76ae6e0e4d92ccd4195a67aac632d6d",
     false, false, false},
    {"simple-40-small-capacity",
     "52500ea1696ca9bf838f888a2e84305ea55402df668d85ce1ac13675b66ae6e0e5d92ccc4195a67aac632d6d",
     10, "", 10040, 0, 0, "", false, false, false},
    {"extended-40",
     "000000006eb15c81000a5d1cfdfe7f14d38dc54645f7bc9165158e9dbcc6ec05c8181880e5b5b811b1bbffa983a3bd1e46b2",
     8192, "", 0, 0x3412, 40,
     "5c1dfdff7e15d38cc54644f6bd9164158e9cbdc7ed04c9181980e4b4b810b0bbffa983a3bd1e46b2",
     true, false, false},
    {"extended-15",
     "00000000f2e38901000a9a1ba5179dbc46717fafca3293bf4d",
     8192, "", 0, 0x3412, 15,
     "9a1ba5179dbc46717fafca3293bf4d", true, false, false},
    {"extended-32",
     "00000000835ab398000a826f0d0fc8f77ed58b95c8422222548924c533ef534cb963556274c6b6004eed",
     8192, "", 0, 0x3412, 32,
     "836f0c0ec8f67ed58a95c8432322558825c532ee534db863546274c7b7004eed", true, false, false},
    {"extended-31",
     "00000000d51bb8ae000a836f0c0ec8f67ed58a95c8432322558825c532ee534db863546274c7b7004e",
     8192, "", 0, 0x3412, 31,
     "836f0c0ec8f67ed58a95c8432322558825c532ee534db863546274c7b7004e", true, false, false},
    {"retry-40",
     "0000000001000100000a345c505a3284d922c3872f1fd4ee920196ab5d7e621e57b8e3cb3e30087c82f28a50",
     8192, "", 0, 0x3412, 40,
     "00000000000a345c505a3284d822c2862e1ed4ee920096aa5c7e621e56b8e2ca3e30087c82f28a50",
     true, false, false},
    {"compressed-40",
     "0000000052500ea1010a696ca9bf838f888a2e84305ea55402df668d85ce1ac13675b66ae6e0e5d92ccc4195a67aac632d6d",
     8192, "abcd00ff1234", 0, 0x3412, 6,
     "abcd00ff1234898a2e84305ea55403de668d85cf1ac03674b76ae6e0e4d92ccd4195a67aac632d6d",
     true, true, true},
    {"compressed-40-no-expander",
     "0000000052500ea1010a696ca9bf838f888a2e84305ea55402df668d85ce1ac13675b66ae6e0e5d92ccc4195a67aac632d6d",
     8192, "", 106, 0, 0,
     "686da8be838e898a2e84305ea55403de668d85cf1ac03674b76ae6e0e4d92ccd4195a67aac632d6d",
     true, true, false},
    {"corrupt-40",
     "52500ea1696ca9bf838f888a2e84305ea55402df668d85ce1ac13675b66ae6e0e5d92ccc4195a67aac632d92",
     8192, "", 10014, 0, 0,
     "686da8be838e898a2e84305ea55403de668d85cf1ac03674b76ae6e0e4d92ccd4195a67aac632d92",
     false, false, false},
    {"malformed-extended-9",
     "000000000000000000",
     8192, "", 10014, 0, 0, "", true, false, false},
};

const ReceiveVector* find_vector(const std::string& name) {
    for (const ReceiveVector& vector : kReceiveVectors) {
        if (name == vector.name) return &vector;
    }
    return nullptr;
}

dfre::Expander fixed_expander(const std::vector<std::uint8_t>& bytes) {
    return [bytes](const std::uint8_t*, std::size_t, std::size_t capacity)
        -> std::optional<std::vector<std::uint8_t>> {
        if (bytes.size() > capacity) return std::nullopt;
        return bytes;
    };
}

bool decode_matches(const ReceiveVector& vector) {
    dfre::DatagramSource source;
    source.ip = 0x0100007f;
    source.port = 0x1234;
    dfre::Expander expander;
    if (vector.expander[0] != '\0') {
        expander = fixed_expander(from_hex(vector.expander));
    }
    const dfre::ReceiveDecodeResult result =
        dfre::decode_received_datagram(from_hex(vector.packet), vector.capacity, source, {}, expander);
    if (!require(static_cast<std::uint32_t>(result.status) == vector.status, vector.name)) return false;
    if (!require(result.source_port_host == vector.host_port, vector.name)) return false;
    if (!require(result.reported_size == vector.size, vector.name)) return false;
    if (!require(result.payload == from_hex(vector.payload), vector.name)) return false;
    if (!require(result.extended_header == vector.extended, vector.name)) return false;
    if (!require(result.compressed_flag == vector.compressed, vector.name)) return false;
    if (!require(result.decompressed == vector.decompressed, vector.name)) return false;
    if (vector.status == 0) {
        if (!require(result.source.ip == 0x0100007f, vector.name)) return false;
        if (!require(result.source.port == 0x1234, vector.name)) return false;
    } else {
        if (!require(result.source.ip == 0 && result.source.port == 0, vector.name)) return false;
    }
    return true;
}

bool check_receive_vectors() {
    for (const ReceiveVector& vector : kReceiveVectors) {
        if (!decode_matches(vector)) return false;
    }
    return true;
}

bool check_crc() {
    const std::string check = "123456789";
    return require(dfre::crc32_mpeg2(reinterpret_cast<const std::uint8_t*>(check.data()), check.size()) ==
                       0x0376e6e7u,
                   "MPEG-2 CRC check vector failed.");
}

bool check_round_trip() {
    const std::size_t sizes[] = {1, 2, 31, 32, 33, 255, 1024};
    for (const std::size_t size : sizes) {
        std::vector<std::uint8_t> payload(size);
        for (std::size_t at = 0; at < size; ++at) {
            payload[at] = static_cast<std::uint8_t>(at * 7 + 3);
        }
        dfre::DatagramSource source;
        source.ip = 0x09080706;
        source.port = 0x0a0b;
        const dfre::ReceiveDecodeResult result =
            dfre::decode_received_datagram(dfre::encode_direct_datagram(payload), 8192, source);
        if (!require(result.status == dfre::ReceiveStatus::Ok, "Round trip status failed.")) return false;
        if (!require(result.payload == payload, "Round trip payload failed.")) return false;
        if (!require(result.reported_size == size, "Round trip size failed.")) return false;
        if (!require(result.source.ip == 0x09080706 && result.source_port_host == 0x0b0a,
                     "Round trip source failed.")) return false;
    }
    return true;
}

bool check_filter() {
    const std::vector<dfre::IpFilterRange> ranges = {{0x0a000000, 0x00ffffff}, {0x7f000000, 0x0000ffff}};
    if (!require(dfre::source_address_blocked(0x0302010a, ranges), "10.1.2.3 must be blocked.")) return false;
    if (!require(dfre::source_address_blocked(0x0100007f, ranges), "127.0.0.1 must be blocked.")) return false;
    if (!require(!dfre::source_address_blocked(0x0403020b, ranges), "11.2.3.4 must pass.")) return false;
    if (!require(dfre::source_address_blocked(0x0000000a, ranges), "10.0.0.0 boundary must be blocked.")) return false;
    if (!require(dfre::source_address_blocked(0xffffff0a, ranges), "10.255.255.255 boundary must be blocked.")) return false;
    if (!require(!dfre::source_address_blocked(0x0000000b, ranges), "11.0.0.0 must pass.")) return false;
    if (!require(!dfre::source_address_blocked(0x0100007f, {}), "Empty filter must pass.")) return false;
    const std::vector<dfre::IpFilterRange> single = {{0x0a000000, 0x00000000}};
    if (!require(dfre::source_address_blocked(0x0000000a, single), "Single address must be blocked.")) return false;
    if (!require(!dfre::source_address_blocked(0x0100000a, single), "Next address must pass.")) return false;

    dfre::DatagramSource source;
    source.port = 0x1234;
    source.ip = 0x0000000a;  // 10.0.0.0
    const dfre::ReceiveDecodeResult blocked = dfre::decode_received_datagram(
        dfre::encode_direct_datagram({1, 2, 3}), 8192, source, ranges);
    if (!require(blocked.status == dfre::ReceiveStatus::SourceBlocked, "Blocked source status failed.")) return false;
    if (!require(blocked.payload.empty() && blocked.reported_size == 0, "Blocked source wrote output.")) return false;
    return true;
}

bool check_compression_dispatch() {
    const ReceiveVector* vector = find_vector("compressed-40");
    if (!require(vector != nullptr, "Missing compressed-40 vector.")) return false;
    const std::vector<std::uint8_t> packet = from_hex(vector->packet);
    dfre::DatagramSource source;
    source.ip = 0x0100007f;
    source.port = 0x1234;
    const dfre::Expander failing = [](const std::uint8_t*, std::size_t, std::size_t)
        -> std::optional<std::vector<std::uint8_t>> { return std::nullopt; };
    const dfre::ReceiveDecodeResult failed =
        dfre::decode_received_datagram(packet, 8192, source, {}, failing);
    if (!require(failed.status == dfre::ReceiveStatus::CompressionUnavailable,
                 "Failed expander status failed.")) return false;
    if (!require(failed.payload.empty() && failed.reported_size == 0, "Failed expander wrote output.")) return false;

    // Port extension: the original copies the expanded bytes unchecked; a size over
    // the destination capacity is reported instead of overflowing.
    const dfre::Expander oversized = [](const std::uint8_t*, std::size_t, std::size_t)
        -> std::optional<std::vector<std::uint8_t>> {
        return std::vector<std::uint8_t>(48, 0x5a);
    };
    const dfre::ReceiveDecodeResult over =
        dfre::decode_received_datagram(packet, 40, source, {}, oversized);
    if (!require(over.status == dfre::ReceiveStatus::BufferTooSmall,
                 "Oversized expansion status failed.")) return false;
    return true;
}

bool check_fail_closed_cases() {
    dfre::DatagramSource source;
    source.port = 0x1234;
    const std::vector<std::uint8_t> empty;
    if (!require(dfre::decode_received_datagram(empty, 8192, source).status ==
                     dfre::ReceiveStatus::CrcFailed,
                 "Empty datagram must be rejected.")) return false;
    const std::vector<std::uint8_t> three = {1, 2, 3};
    if (!require(dfre::decode_received_datagram(three, 8192, source).status ==
                     dfre::ReceiveStatus::CrcFailed,
                 "Three byte datagram must be rejected.")) return false;
    // The original reads stale socket-buffer bytes here; the port fails closed.
    const std::vector<std::uint8_t> short_extended(9, 0);
    const dfre::ReceiveDecodeResult result = dfre::decode_received_datagram(short_extended, 8192, source);
    if (!require(result.status == dfre::ReceiveStatus::CrcFailed,
                 "Short extended datagram must be rejected.")) return false;
    if (!require(result.payload.empty(), "Short extended datagram wrote output.")) return false;
    const std::vector<std::uint8_t> overrun = {0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 1, 2};
    if (!require(dfre::decode_received_datagram(overrun, 8192, source).status ==
                     dfre::ReceiveStatus::CrcFailed,
                 "Header overrun must be rejected.")) return false;
    return true;
}

bool check_encoder() {
    if (!require(dfre::encode_direct_datagram({}).empty(), "Empty payload must produce no datagram.")) return false;
    if (!require(dfre::encode_direct_datagram({0xff}) ==
                     std::vector<std::uint8_t>({0x00, 0xff, 0xff, 0xff, 0xff}),
                 "One-byte CRC prefix order failed.")) return false;
    const std::string text = "123456789";
    const std::vector<std::uint8_t> check(text.begin(), text.end());
    auto encoded = dfre::encode_direct_datagram(check);
    if (!require(little_word(encoded) == 0x0376e6e7u, "CRC/MPEG-2 check vector failed.")) return false;
    if (!require(std::vector<std::uint8_t>(encoded.begin() + 4, encoded.end()) == check,
                 "Short payload bytes must stay unchanged.")) return false;

    // Expected CRC values come from separate polynomial long division over GF(2).
    const std::uint32_t zero_crc[] = {0xf2ca6f69u, 0x4a55af67u, 0x4ea3c1a1u};
    for (std::size_t size = 31; size <= 33; ++size) {
        const std::vector<std::uint8_t> payload(size, 0);
        encoded = dfre::encode_direct_datagram(payload);
        if (!require(encoded.size() == size + 4, "Datagram size failed.")) return false;
        if (size == 31) {
            if (!require(little_word(encoded) == zero_crc[0], "31-byte CRC prefix failed.")) return false;
            if (!require(std::vector<std::uint8_t>(encoded.begin() + 4, encoded.end()) == payload,
                         "31-byte payload changed.")) return false;
        } else {
            if (!require(little_word(encoded) == 0, "Zero low-bit prefix failed.")) return false;
            if (!require(embedded_crc(encoded) == zero_crc[size - 31], "Embedded CRC failed.")) return false;
            if (size == 33 && !require(encoded.back() == 0, "33rd payload byte changed.")) return false;
        }
    }
    std::vector<std::uint8_t> payload;
    for (unsigned value = 0; value < 33; ++value) payload.push_back(static_cast<std::uint8_t>(value));
    const auto original = payload;
    encoded = dfre::encode_direct_datagram(payload);
    if (!require(little_word(encoded) == 0xaaaaaaaau, "Saved alternating low bits failed.")) return false;
    if (!require(embedded_crc(encoded) == 0x48f9bbb3u, "33-byte check vector failed.")) return false;
    for (std::size_t byte = 0; byte < 32; ++byte) {
        if (!require((encoded[4 + byte] & 0xfeu) == (payload[byte] & 0xfeu), "Upper payload bits changed.")) return false;
    }
    if (!require(encoded.back() == 32, "Payload tail changed.")) return false;
    if (!require(payload == original, "Input payload changed.")) return false;
    payload.assign(32, 0xa4);
    for (unsigned bit = 0; bit < 32; ++bit) payload[bit] |= (0x12345678u >> bit) & 1u;
    encoded = dfre::encode_direct_datagram(payload);
    if (!require(std::vector<std::uint8_t>(encoded.begin(), encoded.begin() + 4) ==
                     std::vector<std::uint8_t>({0x78, 0x56, 0x34, 0x12}),
                 "Long-payload prefix byte order failed.")) return false;
    return true;
}

} // namespace

int main() {
    if (!check_receive_vectors()) return 1;
    if (!check_crc()) return 1;
    if (!check_round_trip()) return 1;
    if (!check_filter()) return 1;
    if (!check_compression_dispatch()) return 1;
    if (!check_fail_closed_cases()) return 1;
    if (!check_encoder()) return 1;
    return 0;
}
