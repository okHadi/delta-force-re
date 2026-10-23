# DF socket OS backend

`MissionDfSocketRuntime` supplies the OS calls used by the source-backed DF socket bodies. Keep its canonical memory provider alive. Use callbacks on the thread that made the runtime.

Route shared reads and writes through `claims`, `read`, and `write`. The runtime delegates other addresses to the canonical provider. This exposes each current pointer to startup and later callers.

| Source address | Owner |
| --- | --- |
| `8E1390` | 400-byte source WSADATA image |
| `23000000` | Caller scratch frames, 256-byte spacing |
| `23100000` | Current 32-bit host lookup result |
| `24000000` | Guarded work allocations |
| `8E352C..8E3550` | Caller-owned shared socket cells |

Scratch bytes persist when a frame is reused. `new_memory_byte` defines the initial scratch and work-buffer bytes. These bytes are a backend rule. Set the total work-buffer limit with `allocation_limit` (default 64 MiB). Allocation `49DC80` retains the source size guard and eight-byte rounding, with a 16-byte minimum.

The backend supports IPv4 UDP creation, bind, nonblocking mode, send/receive buffer options, address conversion, and host lookup. It maps source socket values to owned OS sockets. Destruction closes these sockets and releases each successful Winsock startup reference. Keep the runtime alive while any callback or source pointer is in use. Unsupported calls, layouts, widths, and unmapped owned bytes raise `MissionClientStartupError`.

Windows uses Winsock and copies native WSADATA fields into the source layout. Link `ws2_32`. POSIX uses native socket calls and maps supported errors to Winsock codes. An unknown error raises an error. POSIX WSADATA contains defined version 1.1 metadata (`POSIX`, `Running`); native buffer sizes remain OS-dependent.

Numeric IPv4 lookup uses local address parsing. Named lookup requires `allow_external_dns=true`. The default disables named DNS. The backend has no packet send or receive operation. Source bind calls preserve their requested address, including ANY.

A `gethostbyname` call before this runtime's first successful Winsock startup returns zero and sets error 10093 (`WSANOTINITIALISED`). This error path performs no DNS lookup. Source `43FF30` reaches address resolution before `454BB0` starts Winsock. See the [Windows return contract](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-gethostbyname).

The native test joins real OS calls to `454BB0`, receive/send retry bodies, buffer helpers, address resolution, and guarded allocation. It checks source memory visibility, frame reuse, failure prefixes, thread checks, address-in-use errors, and socket release. It uses local binds and sends no packets. Run the test where local socket creation is permitted. The frozen pure-body checker supplies the instruction comparison; this test checks the OS boundary.
