# Netsock packet memory and receive

Construct one `MissionNetsockPacketMemory` with the supplied `Netsocknl.dll` and the canonical host memory provider. It checks DLL SHA-256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2` and maps its PE image at `10000000..1001B000`. Route that range through this owner. Route all dynamic storage, DF buffers, and caller frames through the host provider. `resolve` returns those same bytes. Use the host's one allocator and free service for every dynamic block.

The constructor copies source image bytes. Bind actual init, socket, queue, worker, and import services when reached. Use current raw IAT targets for Sleep and free. Keep the host memory provider and DLL owner alive through each call.

Ported bodies:

- `10001C80`: GetMessage and its init/socket/session gates.
- `10001BE0` / `10001C30`: host/client receive queues.
- `10003C00`: raw queue pop, lock, copy, free, and counter updates.
- `10001AA0`: GetPlayerName and its source gates.
- `100019E0` / `10001A20`: current host player lookup and `88`-byte record copy.
- `10001A50`: client name helper; returns zero and leaves output bytes current.
- DF `4B97A0`: copies the current returned name to `D0A500` and returns that source address.

Use the frame layouts in the header. Queue pop changes its own queue-argument cell to the next head. Host/client helpers use their local length word and overwrite their first argument with the sender. Keep these aliases mapped through nested calls and free callbacks.

Queue records contain a payload pointer, byte count, source address, and source port, each four bytes. The queue uses its current head/tail and a `10000`-byte ring. Pop captures head/tail before setting the lock. It copies the current full payload count with four-byte loads/stores and byte tail copies, then frees the current payload pointer. It reads the next head and queue count after free. Negative counts reach the required diagnostic. Missing calls preserve the completed write prefix.

The source pop does not test the incoming capacity. Keep destination bounds in the canonical memory provider. A failed access reports an error after the source's earlier stores. Release caller frames during errors; retain source lock and queue writes.

Connect the DF bridge's GetMessage import to `mission_netsock_get_message`. Keep `D0A540`, its destination bytes, and `501344/50135C/501360` in the same canonical memory chain. Connect GetPlayerName to its real DLL body and resolve `D0A500` through DF memory. Use source addresses in both paths.

Evidence: `.local/reports/mission-netsock-receive-capstone-source.json`, `mission-netsock-names-capstone-source.json`, retained `netsock-session-exports-root.json`, and `mission-transport-missing-producers-capstone-audit.json`. The original checker compares canonical byte hashes and ordered read/store/call hashes with the supplied DLL and DF instructions. Tests cover queue wrap, locks, alias copies, callback changes, failure prefixes, names, and client output retention. Native checks join the real DLL receive body to the DF bridge and verify shared destination bytes and name pointers. Tests send no network traffic.
