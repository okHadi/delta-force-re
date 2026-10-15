#include "dfre/network_send.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "dfre/compression.hpp"
#include "dfre/network.hpp"

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
        std::cerr << "Bad hex length.\n";
        return {};
    }
    for (std::size_t at = 0; at < text.size(); at += 2) {
        const int high = hex_digit(text[at]);
        const int low = hex_digit(text[at + 1]);
        if (high < 0 || low < 0) {
            std::cerr << "Bad hex.\n";
            return {};
        }
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
}

dfre::SendTarget test_target() {
    dfre::SendTarget target;
    target.ip = 0x0100007f;
    target.port = 0x3039;
    return target;
}

// Frozen from the original FUN_10005120 through tools/check_network_send_original.py.
struct ExtendedVector {
    const char* name;
    const char* payload;
    const char* datagram;
    bool flag;
};

const ExtendedVector kExtendedVectors[] = {
    {"extended-hello.0", "48656c6c6f", "000000006d9babe5000a48656c6c6f", false},
    {"extended-hello.1", "48656c6c6f", "000000006d9babe5010a48656c6c6f", true},
    {"extended-40.0",
     "f407292d84c9ce4652098e501a762af2958375ba0133aaeece102977b494b903c4452f61cfaf211d",
     "000000002e0237cc000af406292d84c8cf4753088e501b762bf3958274ba0132aaeece102976b595b803c4452f61cfaf211d",
     false},
    {"extended-40.1",
     "fa59879ac3d2d116f12a5158782bbc59f3615ab929aadecd35d3a663b8fdff439d16fabd741322fa",
     "0000000056a59beb010afa59879bc2d2d016f12a5158782abd58f2605bb828abdecc35d3a763b8fcff439d16fabd741322fa",
     true},
};

// Frozen from the original FUN_10005860. Payloads longer than 256 bytes are
// rebuilt by their note; their exact bytes were compared by the checker.
struct SelectVector {
    const char* name;
    const char* payload;  // empty + length for generated payloads
    std::size_t length;
    std::uint32_t status;
    const char* format;  // "simple", "extended" or "none"
    std::size_t wire;
    bool flag;
    const char* datagram;
};

const SelectVector kSelectVectors[] = {
    {"select-random.100",
     "898971071f4a2469e32cb9ae704d1a8d212b909f0be62b891bd594e53a60c639df193eaab3e1426416015d9feb0a8b4f62d2906017f34fc0f6c969e4a8703e60f2a073068ff0fe4693236bced70ee3130e029ff22d6a9fcdc7a99b48d8da6c8801ba9525",
     0, 0, "simple", 104, false,
     "9fa5db8b898870071e4a2469e22cb8af714c1b8c212a909e0ae72a881ad494e43b61c638df193eaab3e1426416015d9feb0a8b4f62d2906017f34fc0f6c969e4a8703e60f2a073068ff0fe4693236bced70ee3130e029ff22d6a9fcdc7a99b48d8da6c8801ba9525"},
    {"select-zeros.4000", "", 4000, 0, "extended", 1512, true, ""},
    {"select-zeros.20000", "", 20000, 0, "extended", 7512, true, ""},
    {"select-random.9000", "", 9000, 10040, "none", 0, false, ""},
};

std::vector<std::uint8_t> make_payload(const SelectVector& vector) {
    if (vector.payload[0] != '\0') {
        return from_hex(vector.payload);
    }
    if (vector.name == std::string("select-random.9000")) {
        std::vector<std::uint8_t> payload(vector.length);
        std::uint32_t state = 0x12345678u;
        for (std::size_t at = 0; at < payload.size(); ++at) {
            state = state * 1664525u + 1013904223u;
            payload[at] = static_cast<std::uint8_t>(state >> 24);
        }
        return payload;
    }
    return std::vector<std::uint8_t>(vector.length, 0);
}

const char* format_name(dfre::SendWireFormat format) {
    return format == dfre::SendWireFormat::Extended ? "extended" : "simple";
}

bool check_extended_vectors() {
    for (const ExtendedVector& vector : kExtendedVectors) {
        const std::vector<std::uint8_t> payload = from_hex(vector.payload);
        const dfre::ExtendedEncodeResult result =
            dfre::encode_extended_datagram(payload, test_target(), vector.flag);
        if (!require(result.status == dfre::SendStatus::Ok, vector.name)) return false;
        if (!require(result.datagram == from_hex(vector.datagram), vector.name)) return false;
        if (!require(result.compressed_flag == vector.flag, vector.name)) return false;
        if (!require(result.target.ip == 0x0100007f && result.target.port == 0x3039, vector.name)) return false;
        if (!require(result.datagram[0] == 0 && result.datagram[1] == 0 && result.datagram[2] == 0 &&
                         result.datagram[3] == 0,
                     vector.name)) return false;
        if (!require(result.datagram[9] == 10, vector.name)) return false;
        if (!require((result.datagram[8] & 1u) == (vector.flag ? 1u : 0u), vector.name)) return false;
    }
    return true;
}

bool check_select_vectors() {
    for (const SelectVector& vector : kSelectVectors) {
        const std::vector<std::uint8_t> payload = make_payload(vector);
        const dfre::SendSelectionResult result =
            dfre::select_and_encode_send(payload, test_target());
        if (!require(static_cast<std::uint32_t>(result.status) == vector.status, vector.name)) return false;
        if (!require(result.datagram.size() == vector.wire, vector.name)) return false;
        if (vector.format[0] != 'n') {
            if (!require(format_name(result.format) == vector.format, vector.name)) return false;
        }
        if (!require(result.compressed_flag == vector.flag, vector.name)) return false;
        if (vector.datagram[0] != '\0' &&
            !require(result.datagram == from_hex(vector.datagram), vector.name)) return false;
    }
    return true;
}

bool decode_round_trip(const std::vector<std::uint8_t>& datagram,
                       const std::vector<std::uint8_t>& payload, std::size_t capacity,
                       const dfre::Expander& expander) {
    dfre::DatagramSource source;
    source.ip = 0x0100007f;
    source.port = 0x3039;
    const dfre::ReceiveDecodeResult decoded =
        dfre::decode_received_datagram(datagram, capacity, source, {}, expander);
    if (!require(decoded.status == dfre::ReceiveStatus::Ok, "Round trip status failed.")) return false;
    if (!require(decoded.payload == payload, "Round trip payload failed.")) return false;
    // The receive outputs htons(sin_port) and the raw sin_addr.
    if (!require(decoded.source_port_host == 0x3930 && decoded.source.ip == 0x0100007f,
                 "Round trip source failed.")) return false;
    return true;
}

bool check_round_trips() {
    const dfre::Expander expander = dfre::expand_compressed;
    // Simple path through the existing decoder.
    const std::vector<std::uint8_t> random_payload = from_hex(
        "898971071f4a2469e32cb9ae704d1a8d212b909f0be62b891bd594e53a60c639df193eaab3e1426416015d9feb0a8b4f62d2906017f34fc0f6c969e4a8703e60f2a073068ff0fe4693236bced70ee3130e029ff22d6a9fcdc7a99b48d8da6c8801ba9525");
    const dfre::SendSelectionResult simple =
        dfre::select_and_encode_send(random_payload, test_target());
    if (!require(simple.format == dfre::SendWireFormat::Simple, "Expected simple path.")) return false;
    if (!decode_round_trip(simple.datagram, random_payload, 8192, expander)) return false;

    // Compressed path through the existing decoder with expand_compressed.
    const std::vector<std::uint8_t> zeros(4000, 0);
    const dfre::SendSelectionResult compressed = dfre::select_and_encode_send(zeros, test_target());
    if (!require(compressed.format == dfre::SendWireFormat::Extended, "Expected extended path.")) return false;
    if (!require(compressed.compressed_flag, "Expected compressed flag.")) return false;
    if (!decode_round_trip(compressed.datagram, zeros, 65536, expander)) return false;

    // A payload past the simple limit still rides the compressed path.
    const std::vector<std::uint8_t> big_zeros(8189, 0);
    const dfre::SendSelectionResult big = dfre::select_and_encode_send(big_zeros, test_target());
    if (!require(big.status == dfre::SendStatus::Ok &&
                     big.format == dfre::SendWireFormat::Extended,
                 "Compressed path must allow payloads over the simple limit.")) return false;
    if (!decode_round_trip(big.datagram, big_zeros, 65536, expander)) return false;
    return true;
}

bool check_boundaries() {
    // Extended encoder limit: buffer_capacity - 4 bytes.
    const std::vector<std::uint8_t> at_limit(8188, 0x5a);
    const dfre::ExtendedEncodeResult ok =
        dfre::encode_extended_datagram(at_limit, test_target(), false);
    if (!require(ok.status == dfre::SendStatus::Ok && ok.datagram.size() == 8198,
                 "8188-byte extended payload must encode.")) return false;
    const std::vector<std::uint8_t> over_limit(8189, 0x5a);
    const dfre::ExtendedEncodeResult over =
        dfre::encode_extended_datagram(over_limit, test_target(), false);
    if (!require(over.status == dfre::SendStatus::PayloadTooLarge && over.datagram.empty(),
                 "8189-byte extended payload must be rejected.")) return false;

    // Empty payloads produce no datagram and report success.
    const dfre::ExtendedEncodeResult empty_extended =
        dfre::encode_extended_datagram({}, test_target(), true);
    if (!require(empty_extended.status == dfre::SendStatus::Ok && empty_extended.datagram.empty() &&
                     !empty_extended.compressed_flag,
                 "Empty extended encode failed.")) return false;
    const dfre::SendSelectionResult empty_select =
        dfre::select_and_encode_send({}, test_target());
    if (!require(empty_select.status == dfre::SendStatus::Ok && empty_select.datagram.empty(),
                 "Empty select failed.")) return false;
    return true;
}

bool check_compression_unavailable() {
    const dfre::SendSelectionResult result =
        dfre::select_and_encode_send(from_hex("48656c6c6f"), test_target(), false);
    if (!require(result.status == dfre::SendStatus::CompressionUnavailable,
                 "Missing codec must return 0x6a.")) return false;
    if (!require(result.datagram.empty() && !result.compressed_flag,
                 "Missing codec must not send.")) return false;
    return true;
}

} // namespace

int main() {
    if (!check_extended_vectors()) return 1;
    if (!check_select_vectors()) return 1;
    if (!check_round_trips()) return 1;
    if (!check_boundaries()) return 1;
    if (!check_compression_unavailable()) return 1;
    return 0;
}
