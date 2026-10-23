# Netsock client worker

Use one current packet memory, allocator, queue owner and frame provider. Call `mission_netsock_client_worker_call` with the source entry and its mapped frame.

| Entries | Body |
| --- | --- |
| `10003300`, `10003470`, `10001FE0` | Blocking client worker, client start, ping arm |
| `10003290`, `10002E40`, `10002E70` | Outgoing drain, incoming queue, payload delivery |
| `100031A0`, `100031C0`, `10003210` | Join, action and close timers |
| `100030A0`, `10003130`, `10003170` | Join, action and close packets |
| `10003260`, `10002F20` | Heartbeat and disconnect packets |
| `10002F50`, `10002E00`, `10002EC0`, `10002F00` | Current opcode table and reply handlers |
| `10002EA0`, `10002EE0` | Action and close completion |

Map all frame sizes and offsets in hex:

- `3300`: `C` bytes; port WORD at `2`, IP at `4`, length at `8`. Keep incoming bytes.
- `3290`: `C` bytes; port at `0`, IP at `4`, length at `8`.
- `1FE0`: `C` bytes; IP at `4`, port WORD at `8`.
- `2E40`: `14` bytes; four arguments at `4..10`.
- `2E70`: `C` bytes; packet and IP at `4/8`.
- Other one-argument entries: `8` bytes, argument at `4`.
- Frameless entries use frame `0`.

Keep current OS imports, clock, packet handlers and thread services bound. `2180` remains a required ping-reply handler. Client start uses the existing queue initializer and drop-file producer. It calls `_beginthread(10003300,0,0)` and ignores its return value.

The worker reads current time, random and Sleep call pointers at each source reach. Ping arm caches its initial clock value before its state gates. Packet builders preserve source WORD stores and current buffer reads.

Run the worker on a host-owned thread. Keep current memory and frames valid until that thread ends. Protect shared backend access. Use the current `19B40` stop cell. Keep OS join outside the source body. The production `_endthread` provider ends its OS thread; a controlled test provider can return.

Close uses the existing `10001830` wait and the actual `10003210` timer. Completion `10002EE0` clears `19C10`, writes the result to `19C1C`, then sets `19C14`. The worker reaches completion after a reply, a disconnected state, or more than 75 close-timer calls. Keep absent services as errors at their reached call.

The native test runs actual client start, queue allocation, drop seed, worker, close timer, packet encoder and close wait. It captures three `1040` packets and joins the worker after source stop. Its declared inputs are an empty receive source, an absent `LOSEPCKT.NFO`, socket identifier `55`, and packet capture in place of outside output. These inputs test the source join; the application must supply its actual socket and file services.

Use the supplied `Netsocknl.dll` SHA-256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`. Exact instructions are retained in `.local/reports/mission-netsock-client-worker-scope.json`. The original checker covers the twenty bodies, read/store/call failures, current-pointer changes, and finite worker loops with actual poll, receiver-error, queue-pop, ping-gate and queue-destroy bodies. Required external calls stop before side effects. Keep the SHA-checked original output tape for sanitizer replay.
