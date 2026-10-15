#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dfre {

// Fixed Huffman codec ported from Cmprssh0.dll
// (sha256 4dbc4de8458b92c4b07933fe6ac6132912f51e3490c511c65731ac12e13539a6).
// 257 symbols: byte values 0..255 plus one end-of-stream code. This is not a
// generic LZ format; it only matches this shipped DLL.
//
// Original exports (see docs/network-compression.md):
//   init_compression 0x10001150, compress_buffer 0x100012e0,
//   expand_buffer 0x10001400.

// One code of the fixed table. `value` holds the right-aligned code.
struct HuffmanCode {
    std::uint32_t value = 0;
    std::uint8_t bits = 0;
};

// Port of init_compression (0x10001150): builds the static code table from
// the frequency table shipped in the DLL. Safe to call more than once; the
// other functions build the table on first use, so calling it is optional.
void init_compression();

// Code of `symbol` (0..255 byte, 256 end of stream). The longest code in the
// original table is 11 bits.
HuffmanCode symbol_code(unsigned symbol);

// Output allocation size for `size` input bytes. Includes the end marker and
// the writer's trailing reset byte. Wire length is (returned bits + 7) / 8.
std::size_t compress_bound(std::size_t size);

// Port of compress_buffer (0x100012e0). Writes the zero-padded bitstream to
// `out` (compress_bound(src_len) bytes) and returns the number of code bits,
// the end-of-stream code included. src_len < 0 encodes only the
// end-of-stream code, like the original.
int compress_buffer(const std::uint8_t* src, int src_len, std::uint8_t* out);

// Port of expand_buffer (0x10001400). Decodes until the end-of-stream code
// and returns the number of output bytes. The original has no input or output
// bounds; this port stops safely with -1 when the stream is truncated,
// invalid, or would write more than `dst_capacity` bytes. Valid streams
// decode exactly like the original.
int expand_buffer(const std::uint8_t* src, std::size_t src_len, std::uint8_t* dst,
                  std::size_t dst_capacity);

// Adapter for the network receive hook (see `dfre::Expander` in network.hpp).
// Returns the expanded bytes, or std::nullopt for invalid or truncated input.
std::optional<std::vector<std::uint8_t>> expand_compressed(const std::uint8_t* compressed,
                                                           std::size_t size,
                                                           std::size_t capacity);

} // namespace dfre
