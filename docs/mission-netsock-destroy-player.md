# Netsock DestroyPlayer

Use `mission_netsock_destroy_player_call` with the current packet memory, source allocator, queues and required providers.

|Entry|Body|
|---|---|
|10001B10|Find and remove a host player|
|10001B40|Request a client removal and wait for completion|
|10001B80|Check export state and select the current role|
|10004510|Send the 1033 destroy reply|
|10004550|Check and process a host 1032 request|
|100045C0|Send the 1041 close reply|
|10004660|Process a host 1040 request|

Map each complete raw frame listed in the header. Keep cached endpoint arguments and current packet/player reads at their source positions. Response bodies write the current scratch and reread its pointer before the send.

The client parent caches Sleep, arms the source request cells and waits for `19BFC`. The published client worker sends 1032. Its 1033 handler sets the actual result and completion cells. The parent returns 0 after completion; read the current result cell when its status is needed.

A host 1032 request needs an admitted address and a current remote player with the requested ID. A matching request records usage, removes the player, frees its current user data, queues 1051 and sends status 0. A missing address returns directly. A missing or mismatched player sends status 1.

A host 1040 request removes the current address and its linked players through the existing removal body. Its 1041 reply reaches the real client close acknowledgement. Keep source stop and external OS thread joins separate.

The native paired fixture uses source-created IDs, real encoded datagrams, both worker bodies, actual remove/free/queue effects and current DF event callbacks. It tests an error reply, a successful remove, a new CreatePlayer request and a close with reply status 0. TCP state, work buffers, endpoints and absent optional files are declared inputs. Each peer owns one image and heap. Packets stay in the process. Full DF player-added logic remains a separate integration boundary.

Source: supported `Netsocknl.dll` SHA256 `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`. The checker compares ordered original reads, writes, calls, repeat-clear instructions and failure prefixes. Reached logging stays a required provider in the bounded oracle fixtures.
