# Netsock host setup

Use one current `MissionNetsockPacketMemory`, canonical allocator, and mapped frame provider for the host, queues, worker, and packet code. Keep each allocation and OS resource with that owner. Supply thread-safe current reads and stores when the worker runs.

The module ports these complete bodies from the supplied DLL:

| Entries | Work |
| --- | --- |
| `100012E0` | Validate/copy the session; allocate and clear player/address tables; start the host |
| `10004050`, `10004020` | Find a free address row; write its current fields |
| `10004A40` | Clear stop; initialize both queues; set up usage/drop state; launch the worker |
| `10002510`, `10002330` | Allocate usage records and read `NWUSAGE.CFG` |
| `10002240`, `10002290`, `10002310` | Read integer/token fields and apply the APPID mask |
| `100021D0` | Seed random from current time; read `LOSEPCKT.NFO` |
| `100028C0`, `100025A0`, `10002710`, `10002620` | Update, grow, build, and send timed usage records |

Use the raw frame layouts in the header. Preserve incoming frame bytes and partial stores. The usage formatter writes 14 bytes into a 16-byte key; keep its two remaining bytes current. Clear/copy each usage record with the source DWORD and WORD widths. Keep the full outgoing port argument: its high 16 bits come from the cached APPID word, and its low 16 bits come from the current endpoint port.

Bind current allocator, CRT/file, clock, resolver, and thread services. Use the real reached calls. The host launches `_beginthread(10004930, 0, 0)` before it sets the final host/session flags. The OS host owns launch, stop, join, thread-local CRT state, and frame lifetime. Preserve possible worker/main interleaving and the source plain flags.

The supplied optional files are absent. File-open failure still produces 128 zeroed usage records. Keep the current DLL endpoint, `208.231.90.230:2827`, and actual clock values. Bind periodic, ping, admission, and shutdown bodies when reached.

The original checker compares ordered reads, stores, calls, state, and meaningful returns. It saves the instruction-run inputs and outputs with DLL and checker hashes. Use `--replay-oracle` to check another build against that verified tape. Native tests join real session setup, both queues, a managed OS thread, the blocking worker, usage creation, and the real packet encoder. Their send provider captures bytes; OS packet output is zero. The worker test stops at the required periodic/ping body. Its declared prior timestamp also tests the timed usage branch with the current clock.

Source evidence: `.local/reports/mission-netsock-host-setup-scope.json`. DLL SHA-256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
