# Network send

`network_send.cpp` ports these `Netsocknl.dll` functions:

| API | Original address |
| --- | --- |
| `encode_extended_datagram` | `10005120` |
| `select_and_encode_send` | `10005860` |

- Supply payload bytes, target fields, and buffer capacity.
- The selector compresses the payload and compares total packet lengths.
  It selects compressed output when `compressed_bytes + 10 < size + 4`.
- Simple packets have a four-byte header. Extended packets have ten bytes:
  zero marker, CRC or bit seed, compressed flag, and header length.
- Both paths store CRC bits in the first 32 payload low bits for payloads
  of at least 32 bytes. The receiver restores those bits.
- The selected payload must fit `buffer_capacity - 4`. Empty payloads send
  no bytes. A missing codec returns 106. Excess length returns 10040.
- Capacity defaults to 8192, the original replay fixture value. Supply the
  runtime capacity when it is known.

The API returns packet bytes. Socket errors require the socket runtime.

Verification: `native_network_send` checks frozen packets, bounds, and
round trips through the receive parser with the real codec port.
`check_network_send_original.py` runs the original selector, encoder, and
codec: 107 selection cases, 24 missing-codec cases, and 30 extended cases.
All match, including every packet byte. Address and undefined behavior
checks pass.
