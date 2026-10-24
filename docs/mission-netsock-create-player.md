# Netsock CreatePlayer

Use `mission_netsock_create_player_call` with one current packet memory owner and required file, allocator, clock and thread providers.

| Entry | Body |
|---|---|
|100014F0|Find a free player slot and write its source ID|
|10001530|Clear and copy a player; copy current user data|
|10001630|Create the local host player|
|10001660|Read the optional `CUST_ID.DAT` token|
|10001700|Request a client player and wait for completion|
|10001780|Check the CreatePlayer export inputs|
|10004190|Send the 1031 reply|
|10004250|Queue the 1050 player-added event|
|10004280|Write the reached player-added log|
|10004340|Process the host1030 request|

Map the header's complete working frames. Keep 1660's incoming saved-prefix byte explicit: an empty returned line reads the byte before its line buffer. Supply the current file result and line bytes. The missing-file path needs no line or close provider.

Keep 1530's current 98-byte input and allocated user data in the canonical address space. Its flags bit 1 publishes the actual host record at 19B00. The export returns 0 after valid guards even if its nested create result is nonzero.

Keep 4340's port argument mapped as writable. The source reuses this cell for the new peer ID while retaining the original port for its reply. Its 1050 event uses the current caller cell after the reply. Keep the required usage body and reached log providers bound.

Client 1700 waits for 19B50. The published client worker sends 1030 and receives 1031 through 2E00 to produce this cell and the returned ID. Keep worker launch, source stop acknowledgement and the external thread join separate.

Source: supported `Netsocknl.dll` SHA256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`. The checker uses exact instruction bytes, including repeat-copy prefixes, and compares ordered reads, writes, calls and failure prefixes.

The native paired fixture has two isolated image/heap owners. Its captured datagrams connect the real encoder, receiver and worker paths. TCP readiness, buffers, endpoints and absent optional files are declared fixture inputs. The DF player-added callback is an explicit current provider; full 43F830 is a separate integration boundary. Run actual-DLL checks with the supplied local binary. All packets stay in the test process.
