#include "dfre/compression.hpp"

#include <cstdint>
#include <iostream>
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
    std::vector<std::uint8_t> out;
    for (std::size_t at = 0; at + 1 < text.size(); at += 2) {
        out.push_back(static_cast<std::uint8_t>((hex_digit(text[at]) << 4) | hex_digit(text[at + 1])));
    }
    return out;
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 0x100000001b3ull;
    }
    return hash;
}

// Same generator as the Python replay; the golden measurements were produced
// from the original DLL with these exact sequences.
struct XorShift64 {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

std::vector<std::uint8_t> random_bytes(std::uint64_t seed, std::size_t size) {
    XorShift64 generator{seed};
    std::vector<std::uint8_t> out;
    out.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(static_cast<std::uint8_t>(generator.next() >> 56));
    }
    return out;
}

std::vector<std::uint8_t> low_entropy_bytes(std::uint64_t seed, std::size_t size) {
    XorShift64 generator{seed};
    std::vector<std::uint8_t> out;
    out.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(static_cast<std::uint8_t>((generator.next() >> 56) % 4));
    }
    return out;
}

std::vector<std::uint8_t> protocol_like_bytes() {
    std::vector<std::uint8_t> out;
    for (int i = 0; i < 20; ++i) {
        const std::string body = "player" + std::to_string(i) + " joined the game";
        out.push_back(0x02);
        out.push_back(0x00);
        out.push_back(static_cast<std::uint8_t>((body.size() + 8) & 0xff));
        out.push_back(0x00);
        out.insert(out.end(), body.begin(), body.end());
        out.insert(out.end(), 4, 0x00);
    }
    return out;
}

std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& payload, int* bits) {
    std::vector<std::uint8_t> out(dfre::compress_bound(payload.size()));
    *bits = dfre::compress_buffer(payload.data(), static_cast<int>(payload.size()), out.data());
    out.resize(*bits >= 0 ? (static_cast<std::size_t>(*bits) + 7) / 8 : 0);
    return out;
}

bool roundtrip(const std::vector<std::uint8_t>& payload, const char* message) {
    int bits = 0;
    const std::vector<std::uint8_t> packed = compress(payload, &bits);
    if (!require(bits > 0, message)) return false;
    std::vector<std::uint8_t> back(payload.size() + 16);
    const int size = dfre::expand_buffer(packed.data(), packed.size(), back.data(), back.size());
    if (!require(size == static_cast<int>(payload.size()), message)) return false;
    back.resize(static_cast<std::size_t>(size));
    return require(back == payload, message);
}

} // namespace

int main() {
    // Two calls: the port must be idempotent like repeated DLL init calls.
    dfre::init_compression();
    dfre::init_compression();

    // Code table entries dumped from the original after init_compression.
    struct CodeGolden {
        unsigned symbol;
        std::uint32_t value;
        unsigned bits;
    };
    const CodeGolden code_golden[] = {
        {0, 7, 3}, {1, 0xb, 4}, {2, 0x63, 7}, {3, 0xd7, 8}, {4, 0xa, 7},
        {5, 0x42, 7}, {6, 3, 5}, {7, 0x3d, 7}, {8, 0xc, 6}, {9, 0x25, 6},
        {10, 0x25, 7}, {11, 0x9c, 8}, {256, 0x94, 11},
    };
    for (const CodeGolden& golden : code_golden) {
        const dfre::HuffmanCode code = dfre::symbol_code(golden.symbol);
        if (!require(code.value == golden.value && code.bits == golden.bits,
                     "Code table entry differs from the original dump.")) return 1;
    }

    // Compressed bytes measured from the original DLL for these payloads.
    struct SmallVector {
        const char* payload_hex;
        int bits;
        const char* bytes_hex;
    };
    const SmallVector small_vectors[] = {
        {"", 11, "1280"},
        {"41", 20, "388940"},
        {"414243", 36, "38c4bf8940"},
        {"313233343536373839", 93, "386f5bb20d39e07bd15804a0"},
        {"00000000000000000000000000000000000000000000000000000000000000000000000000000000",
         131, "ffffffffffffffffffffffffffffff1280"},
        {"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
         176, "ce739ce739ce739ce739ce739ce739ce739ce739c894"},
    };
    for (const SmallVector& vector : small_vectors) {
        const std::vector<std::uint8_t> payload = from_hex(vector.payload_hex);
        int bits = 0;
        const std::vector<std::uint8_t> packed = compress(payload, &bits);
        if (!require(bits == vector.bits, "Compressed bit count differs from the original.")) return 1;
        if (!require(packed == from_hex(vector.bytes_hex), "Compressed bytes differ from the original.")) return 1;
        std::vector<std::uint8_t> back(payload.size() + 16);
        const int size = dfre::expand_buffer(packed.data(), packed.size(), back.data(), back.size());
        if (!require(size == static_cast<int>(payload.size()), "Golden stream did not expand.")) return 1;
        back.resize(static_cast<std::size_t>(size));
        if (!require(back == payload, "Golden stream expanded to other bytes.")) return 1;
    }

    // Larger cases: bit count, byte count and FNV-1a of the exact original
    // output (measured with the Unicorn replay of the DLL).
    struct HashVector {
        std::vector<std::uint8_t> payload;
        int bits;
        std::size_t bytes;
        std::uint64_t hash;
    };
    HashVector hash_vectors[5];
    const std::string text = "Delta Force network compression test payload";
    hash_vectors[0] = {{text.begin(), text.end()}, 404, 51, 0x7d296123364ee3ecull};
    std::vector<std::uint8_t> range;
    for (unsigned value = 0; value < 256; ++value) range.push_back(static_cast<std::uint8_t>(value));
    hash_vectors[1] = {range, 2310, 289, 0x96d40c271c6139d4ull};
    hash_vectors[2] = {random_bytes(0xdf1a01, 1024), 9174, 1147, 0x5d65ede1ac7629b5ull};
    hash_vectors[3] = {protocol_like_bytes(), 4953, 620, 0x54038d3688478e6full};
    hash_vectors[4] = {low_entropy_bytes(0x51e0, 512), 2761, 346, 0x6f43392f4a78f70full};
    for (const HashVector& vector : hash_vectors) {
        int bits = 0;
        const std::vector<std::uint8_t> packed = compress(vector.payload, &bits);
        if (!require(bits == vector.bits, "Large case bit count differs from the original.")) return 1;
        if (!require(packed.size() == vector.bytes, "Large case byte count differs from the original.")) return 1;
        if (!require(fnv1a(packed) == vector.hash, "Large case bytes differ from the original.")) return 1;
        if (!roundtrip(vector.payload, "Large case did not roundtrip.")) return 1;
    }

    // All single byte values and short cycling payloads must roundtrip.
    for (unsigned value = 0; value < 256; ++value) {
        const std::vector<std::uint8_t> payload{static_cast<std::uint8_t>(value)};
        if (!roundtrip(payload, "Single byte payload did not roundtrip.")) return 1;
    }
    for (std::size_t size = 0; size <= 64; ++size) {
        std::vector<std::uint8_t> payload;
        for (std::size_t i = 0; i < size; ++i) payload.push_back(static_cast<std::uint8_t>((i * 7) & 0xff));
        int bits = 0;
        const std::vector<std::uint8_t> packed = compress(payload, &bits);
        if (!require(packed.size() <= dfre::compress_bound(payload.size()),
                     "compress_bound is smaller than the compressed size.")) return 1;
        if (!roundtrip(payload, "Cycling payload did not roundtrip.")) return 1;
    }

    // Seven longest byte codes plus the end code finish on a byte boundary.
    // The allocation also holds the reset byte written after that boundary.
    {
        const std::vector<std::uint8_t> input(7,150);
        if (!require(dfre::symbol_code(150).bits==11,"Boundary fixture needs an 11-bit code."))return 1;
        std::vector<std::uint8_t> output(dfre::compress_bound(input.size()));
        const int bits=dfre::compress_buffer(input.data(),int(input.size()),output.data());
        if (!require(bits==88 && output.size()>=12 && output[11]==0,"Byte-boundary reset needs one extra output byte."))return 1;
    }

    // Capacity rules: exact capacity succeeds, one byte less fails.
    {
        int bits = 0;
        const std::vector<std::uint8_t> packed = compress(from_hex("313233343536373839"), &bits);
        std::vector<std::uint8_t> out(32);
        if (!require(dfre::expand_buffer(packed.data(), packed.size(), out.data(), 9) == 9,
                     "Exact capacity must expand.")) return 1;
        if (!require(dfre::expand_buffer(packed.data(), packed.size(), out.data(), 8) == -1,
                     "Short capacity must fail.")) return 1;
        // The original never checks the input length; a stream cut before the
        // end-of-stream code fails here instead of reading past the buffer.
        if (!require(dfre::expand_buffer(packed.data(), packed.size() - 1, out.data(), out.size()) == -1,
                     "Truncated stream must fail.")) return 1;
        if (!require(dfre::expand_buffer(nullptr, 0, out.data(), out.size()) == -1,
                     "Empty stream must fail.")) return 1;
    }

    // Invalid streams stay bounded by the destination capacity.
    {
        std::vector<std::uint8_t> zeros(64, 0x00);
        std::vector<std::uint8_t> ones(64, 0xff);
        std::vector<std::uint8_t> out(0x10000);
        if (!require(dfre::expand_buffer(zeros.data(), zeros.size(), out.data(), 16) == -1,
                     "Zero stream must not fill a short buffer.")) return 1;
        if (!require(dfre::expand_buffer(zeros.data(), zeros.size(), out.data(), out.size()) == -1,
                     "Zero stream has no end-of-stream code.")) return 1;
        if (!require(dfre::expand_buffer(ones.data(), ones.size(), out.data(), 16) == -1,
                     "0xff stream must not fill a short buffer.")) return 1;
    }

    // A negative length behaves like zero bytes, matching the original loop.
    {
        std::vector<std::uint8_t> out(16);
        const int bits = dfre::compress_buffer(nullptr, -5, out.data());
        if (!require(bits == 11 && out[0] == 0x12 && out[1] == 0x80,
                     "Negative length must encode only end of stream.")) return 1;
    }

    // Adapter used by the network receive hook.
    {
        int bits = 0;
        const std::vector<std::uint8_t> packed = compress(from_hex("414243"), &bits);
        const auto expanded = dfre::expand_compressed(packed.data(), packed.size(), 0x10000);
        if (!require(expanded.has_value() && *expanded == from_hex("414243"),
                     "expand_compressed must return the payload.")) return 1;
        if (!require(!dfre::expand_compressed(packed.data(), packed.size() - 1, 0x10000).has_value(),
                     "expand_compressed must reject truncated input.")) return 1;
        if (!require(!dfre::expand_compressed(packed.data(), packed.size(), 2).has_value(),
                     "expand_compressed must reject short capacity.")) return 1;
    }

    std::cout << "native_compression: all checks passed\n";
    return 0;
}
