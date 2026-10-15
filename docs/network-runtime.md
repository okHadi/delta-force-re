# Network receive

`decode_received_datagram` ports `Netsocknl.dll` `100052E0`.
`source_address_blocked` ports `10005280`.

- Supply the received bytes, output capacity, and raw socket source fields.
- Supply sorted host-order `{start, span}` block ranges. Ends are inclusive.
- Simple packets use a four-byte header. Extended packets use a zero marker,
  a CRC or bit seed, flags at byte 8, and header length at byte 9.
- The parser restores payload low bits and checks CRC-32 (`04C11DB7`).
- Extended packets with a failed CRC use the original second decode attempt.
- Use `expand_compressed` for compressed packets. The callback takes input
  length and output capacity so it can check both bounds.
- Short headers, empty payloads, and retries that require bytes outside the
  packet return `CrcFailed`. Expanded output must fit the supplied capacity.
- Return codes are 0, 102 (blocked), 106 (codec unavailable), 10014 (bad
  packet), and 10040 (small output buffer).

The API parses bytes. Socket I/O and multiplayer sessions require separate
runtime code.

Verification: `native_network` and `check_network_receive_original.py`.
Original replay: 382 packet cases and 265 filter cases, zero differences.
For 56 malformed cases, the check compares status only because original code
reads bytes outside the received packet. Encoder replay: 468 cases.
