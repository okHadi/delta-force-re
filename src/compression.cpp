#include "dfre/compression.hpp"

#include <array>
#include <limits>

namespace dfre {
namespace {

// Symbol weights copied byte-for-byte from Cmprssh0.dll
// (sha256 4dbc4de8458b92c4b07933fe6ac6132912f51e3490c511c65731ac12e13539a6),
// section .data VA 0x10005030 = file offset 0x3830. Index = byte value.
// The end-of-stream symbol 256 has weight 1 in the original
// (init_compression writes it to node 256, VA 0x10008aa0).
constexpr std::array<std::uint32_t, 256> kSymbolWeights = {
    89110, 40061, 5017, 2640, 2844, 3905, 11520, 3811,
    5809, 8581, 3223, 2193, 3202, 2204, 3897, 22584,
    3239, 3540, 1392, 1921, 1656, 2290, 1596, 1864,
    1755, 2272, 5496, 2440, 4498, 1852, 1951, 1493,
    3226, 1499, 693, 1331, 4550, 1838, 2106, 2111,
    870, 823, 875, 961, 954, 1315, 839, 848,
    724, 734, 1386, 891, 1010, 649, 926, 757,
    639, 1664, 595, 587, 1369, 1085, 602, 662,
    923, 744, 1979, 1933, 3963, 1477, 980, 548,
    1586, 728, 799, 795, 790, 589, 590, 453,
    1114, 608, 650, 548, 455, 490, 450, 475,
    728, 528, 582, 871, 500, 496, 608, 622,
    809, 758, 774, 664, 499, 684, 818, 759,
    543, 586, 514, 667, 828, 624, 890, 5279,
    1294, 593, 684, 756, 643, 409, 563, 477,
    1115, 880, 630, 1806, 878, 763, 1801, 816,
    2857, 1654, 688, 820, 1987, 680, 437, 488,
    763, 1280, 363, 447, 830, 2202, 1398, 673,
    497, 420, 435, 389, 438, 367, 343, 555,
    752, 745, 725, 566, 619, 423, 496, 583,
    808, 1410, 441, 646, 501, 497, 502, 546,
    795, 818, 642, 694, 1088, 697, 484, 434,
    546, 501, 414, 493, 626, 522, 570, 455,
    883, 585, 476, 599, 482, 677, 958, 817,
    1990, 1786, 645, 862, 699, 491, 587, 603,
    1104, 578, 671, 447, 666, 571, 840, 612,
    799, 572, 537, 408, 638, 666, 616, 676,
    756, 480, 678, 2301, 515, 467, 876, 554,
    662, 881, 562, 803, 1055, 1606, 697, 678,
    1529, 925, 580, 845, 1072, 1092, 898, 980,
    876, 959, 967, 1016, 1054, 1112, 1451, 1260,
    1653, 1670, 1975, 2199, 8915, 13100, 23270, 20643,
};

constexpr unsigned kSymbolCount = 257;   // 256 bytes + end of stream
constexpr unsigned kRootNode = 512;      // original root for this weight table
constexpr unsigned kSentinelNode = 513;  // original builder sentinel slot
constexpr unsigned kEndOfStream = 256;
constexpr unsigned kMaxCodeBits = 11;    // longest code in the original table

struct Node {
    std::uint32_t weight = 0;
    std::uint32_t child0 = 0;
    std::uint32_t child1 = 0;
};

struct Tables {
    std::array<Node, kSentinelNode + 1> nodes{};
    std::array<HuffmanCode, kSymbolCount> codes{};
};

// Port of FUN_10001000 (0x10001000). Scans nodes 0..count-1 for the two
// smallest nonzero weights and merges them. Strict "<" keeps the first index
// on ties, like the original. Node 513 starts with weight 0x7fffffff and is
// never selected. Returns the root index (512 for this table).
unsigned build_tree(Tables& store) {
    std::array<Node, kSentinelNode + 1>& nodes = store.nodes;
    for (unsigned i = 0; i < 256; ++i) {
        nodes[i].weight = kSymbolWeights[i];
    }
    nodes[kEndOfStream].weight = 1;
    nodes[kSentinelNode].weight = 0x7fffffffu;

    unsigned count = kSymbolCount;
    for (;;) {
        unsigned best1 = kSentinelNode;  // weight 0x7fffffff
        unsigned best2 = kSentinelNode;
        for (unsigned i = 0; i < count; ++i) {
            const std::uint32_t weight = nodes[i].weight;
            if (weight == 0) {
                continue;
            }
            if (weight < nodes[best1].weight) {
                best2 = best1;
                best1 = i;
            } else if (weight < nodes[best2].weight) {
                best2 = i;
            }
        }
        if (best1 == kSentinelNode || best2 == kSentinelNode) {
            break;
        }
        nodes[count].weight = nodes[best1].weight + nodes[best2].weight;
        nodes[best1].weight = 0;
        nodes[best2].weight = 0;
        nodes[count].child0 = best1;
        nodes[count].child1 = best2;
        ++count;
    }
    return count - 1;
}

// Port of FUN_100010e0 (0x100010e0). The original recurses on left children
// and loops along right spines; this explicit stack assigns the same codes.
void assign_codes(const Tables& store, std::array<HuffmanCode, kSymbolCount>& codes) {
    struct Frame {
        unsigned node;
        std::uint32_t code;
        unsigned bits;
    };
    std::array<Frame, kSymbolCount> stack{};
    std::size_t top = 0;
    stack[top++] = {kRootNode, 0, 0};
    while (top > 0) {
        const Frame frame = stack[--top];
        if (frame.node <= kEndOfStream) {
            codes[frame.node] = {frame.code, static_cast<std::uint8_t>(frame.bits)};
            continue;
        }
        const Node& node = store.nodes[frame.node];
        stack[top++] = {node.child1, (frame.code << 1) | 1u, frame.bits + 1};
        stack[top++] = {node.child0, frame.code << 1, frame.bits + 1};
    }
}

const Tables& tables() {
    // Built once, on first use (the original builds in init_compression).
    static const Tables built = [] {
        Tables value;
        const unsigned root = build_tree(value);
        (void)root;  // 512 for the shipped weights; verified against the DLL
        assign_codes(value, value.codes);
        return value;
    }();
    return built;
}

// Port of the bit writer FUN_100011c0 (0x100011c0). `count` is at most 8 and
// `value` is a byte, like the original. The caller splits longer codes.
struct BitWriter {
    std::uint8_t* ptr;
    int pos;  // 7 down to 0: free bits left in *ptr
};

void write_bits(BitWriter& writer, std::uint8_t value, unsigned count) {
    if (count == 0) {
        writer.pos = 7;
        writer.ptr[0] = 0;
        return;
    }
    const std::uint8_t masked = static_cast<std::uint8_t>(((1u << count) - 1u) & value);
    if (count <= static_cast<unsigned>(writer.pos) + 1u) {
        writer.ptr[0] |= static_cast<std::uint8_t>(masked << (writer.pos + 1 - static_cast<int>(count)));
        writer.pos -= static_cast<int>(count);
        if (writer.pos < 0) {
            writer.pos = 7;
            ++writer.ptr;
            writer.ptr[0] = 0;
        }
        return;
    }
    const unsigned rest = count - static_cast<unsigned>(writer.pos) - 1u;  // 1..7 bits for the next byte
    writer.ptr[0] |= static_cast<std::uint8_t>(masked >> rest);
    ++writer.ptr;
    writer.ptr[0] = static_cast<std::uint8_t>((masked & ((1u << rest) - 1u)) << (8u - rest));
    writer.pos = 7 - static_cast<int>(rest);
}

void write_code(BitWriter& writer, HuffmanCode code) {
    unsigned bits = code.bits;
    if (bits > 8) {
        write_bits(writer, static_cast<std::uint8_t>(code.value >> 8), bits - 8);
        bits = 8;
    }
    write_bits(writer, static_cast<std::uint8_t>(code.value), bits);
}

} // namespace

void init_compression() {
    (void)tables();
}

HuffmanCode symbol_code(unsigned symbol) {
    const Tables& table = tables();
    if (symbol >= kSymbolCount) {
        return {};
    }
    return table.codes[symbol];
}

std::size_t compress_bound(std::size_t size) {
    // The original writer clears the next byte at an exact byte boundary.
    constexpr std::size_t kMaxCodeBitsPer = kMaxCodeBits;
    if (size > std::numeric_limits<std::size_t>::max() / kMaxCodeBitsPer - 1) {
        return std::numeric_limits<std::size_t>::max();
    }
    return (kMaxCodeBitsPer * (size + 1)) / 8 + 1;
}

int compress_buffer(const std::uint8_t* src, int src_len, std::uint8_t* out) {
    const Tables& table = tables();
    BitWriter writer{out, 7};
    out[0] = 0;  // original reset call FUN_100011c0(out, 0, 0)
    int bits = 0;
    for (int i = 0; i < src_len; ++i) {
        const HuffmanCode code = table.codes[src[i]];
        bits += code.bits;
        write_code(writer, code);
    }
    const HuffmanCode end = table.codes[kEndOfStream];
    bits += end.bits;
    write_code(writer, end);
    return bits;
}

int expand_buffer(const std::uint8_t* src, std::size_t src_len, std::uint8_t* dst,
                  std::size_t dst_capacity) {
    const Tables& table = tables();
    const std::size_t total_bits = src_len * 8;
    std::size_t bit = 0;
    std::size_t out = 0;
    unsigned node = kRootNode;
    for (;;) {
        if (bit >= total_bits) {
            return -1;  // truncated: the original reads past the input here
        }
        const unsigned next = (src[bit >> 3] >> (7u - static_cast<unsigned>(bit & 7u))) & 1u;
        ++bit;
        node = next == 0 ? table.nodes[node].child0 : table.nodes[node].child1;
        if (node <= 0xffu) {
            if (out >= dst_capacity) {
                return -1;
            }
            dst[out++] = static_cast<std::uint8_t>(node);
            node = kRootNode;
        } else if (node == kEndOfStream) {
            if (out > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                return -1;
            }
            return static_cast<int>(out);
        }
    }
}

std::optional<std::vector<std::uint8_t>> expand_compressed(const std::uint8_t* compressed,
                                                           std::size_t size,
                                                           std::size_t capacity) {
    std::vector<std::uint8_t> out(capacity);
    const int written = expand_buffer(compressed, size, out.data(), capacity);
    if (written < 0) {
        return std::nullopt;
    }
    out.resize(static_cast<std::size_t>(written));
    return out;
}

} // namespace dfre
