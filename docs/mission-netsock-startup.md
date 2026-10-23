# Netsock startup

Use `mission_netsock_startup` with one current `MissionNetsockPacketMemory` and its canonical allocator. Keep that owner, all mapped frames, and all OS resources alive through each call. Release frames during error handling.

The module ports eleven complete bodies from the supplied `Netsocknl.dll`:

| Entry | Body |
| --- | --- |
| `10001000` | Clear DLL state; allocate receive, send, and enumeration buffers |
| `10001180` | Set the address; start TCP/IP and the codec |
| `10004D90` | Start Winsock; set current ports; create the socket and work buffer |
| `10004C80` | Create UDP socket; set nonblocking mode; bind with source retry rules |
| `10004C00` | Set socket options; allocate `signed_max(send, receive) + 10` bytes |
| `10004AF0` / `10004B90` | Read send / receive buffer size |
| `10004B30` / `10004BD0` | Set send and broadcast / receive options |
| `10004FA0` | Resolve numeric, broadcast, or host addresses |
| `10005660` | Find, load, and initialize the selected codec |

Use the raw frame layouts in the header. `10001180 → 10004D90` argument 3 and `10004C80 → htons(1000592E)` argument 1 consume a WORD. Providers and trace checks use those low 16 bits. Keep each real DWORD memory value, stride, and source read point intact. The first bind attempt reads the socket before its port store; retries store the port first.

Bind the current allocator, Winsock imports, resolver, file search, module loader, export lookup, codec entry, and reached enumeration stop. Keep caller-owned sockets and threads with the host. Preserve partial stores and allocation failures. Import and codec targets use the required foreign provider. Supply real operating-system error values or report an unsupported mapping.

Host-session player/address tables, queues, usage records, drop-file state, and worker launch require their own source producers. `NWUSAGE.CFG` open failure still reaches the source allocation of 128 usage records. Use the reached clock and service bodies when the worker runs.

Source evidence: `.local/reports/mission-netsock-startup-worker-inventory.json` and the retained startup/session dossiers named there. DLL SHA-256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.

The original checker compares ordered reads, stores, calls, returns, and state. Native tests use real loopback socket options and the resulting buffer capacity, then pass the produced socket/buffer through the real send encoder to a byte capture. The test bind provider substitutes loopback for source `INADDR_ANY`; the send provider captures bytes. Tests send zero OS packets. Supply the game folder to check the actual DLL and codec exports.
