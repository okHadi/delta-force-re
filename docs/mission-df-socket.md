# DF socket startup

Use `MissionDfSocketBindings` with current shared reads, writes, and required OS calls. The module runs these Df.exe bodies:

- `454BB0`: start Winsock, open the optional receive socket, set current mode and ports, then prepare buffers.
- `454AB0`: open the send socket, set and read buffer sizes, allocate the work buffer, and clear its count.
- `454A10`: bind a send socket. A zero input port starts at `4A42`; other inputs start at the next low-word port.
- `454930`: open a nonblocking receive socket. Mode 1 makes one bind attempt. Other modes add `1000` hex to the low-word port after an address-in-use error.
- `454890` and `454900`: set send and receive buffer sizes.
- `454850` and `4548C0`: return the current buffer size written by `getsockopt`.
- `454DF0`: resolve a numeric address or read the first current host address from the returned list.

Preserve low-word port wrap and signed buffer-size comparisons. Read shared socket fields again at the source read points. Preserve completed writes when a later backend call fails.

The `frame` provider maps current caller memory for each reached stack frame. It receives source argument values for parameter storage. Keep all bytes available until `release`. Supply the current eight-byte sockaddr tail and output storage. `release` completes without an error, including during body errors. The module initializes the exact source fields and passes all 16 sockaddr bytes to the bind boundary.

Bind actual WSOCK32 import calls at `4C55B0`, `4C55B6`, `4C55BC`, `4C55C2`, `4C55C8`, `4C55CE`, `4C55D4`, `4C55E0`, `4C55EC`, and `4C55F8`. Bind `49DC80` to the work-buffer allocator. Use source address tokens for mapped buffers and socket handles. Missing reached providers raise an error.

Evidence: `.local/reports/mission-server-init-0x454bb0-source.json` and the retained reports for each nested body. Reports identify the supplied `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.

Run `check_mission_df_socket_original.py GAME PROBE --report REPORT`. The check compares actual instructions, return values, ordered global reads and writes, call arguments, complete OS buffers, caller stack bytes, current socket changes, port aliases, retries, signed size limits, DNS branches, and backend errors. OS and allocator services use explicit fixtures. Native checks cover port wrap, current socket rereads, allocation failure, frame cleanup, and failed-call prefixes. Tests produce no device audio.
