# Network compression

`compression.cpp` ports the fixed Huffman codec in `Cmprssh0.dll`.

| Original export | Address | Result |
| --- | --- | --- |
| `init_compression` | `10001150` | Builds the 257-symbol table |
| `compress_buffer` | `100012E0` | Code bit count |
| `expand_buffer` | `10001400` | Output byte count |

- Allocate `compress_bound(size)` bytes for compression. This includes the
  reset byte that the original writer clears at a byte boundary.
- Send `(bits + 7) / 8` bytes. The reset byte is outside the wire data when
  the final code ends at a byte boundary.
- Supply compressed input length and output capacity to `expand_buffer`.
  It returns -1 for a truncated stream or a small output buffer.
- Use `expand_compressed` with the receive parser.
- Tables initialize on first use. Equal valid input produces the original
  code bits and output bytes.

Verification: `native_compression` and `check_compression_original.py`.
Original replay: all 257 codes, 468 compression cases, 468 expansion cases,
including three game files. All match. The check also covers 42 invalid
streams. Address and undefined behavior checks pass.
