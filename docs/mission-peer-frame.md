# Peer frame receive

`mission_peer_frame_receive` implements `460E40` from the supplied `Df.exe`.

Use `GameMessageStage::peer_frame_buffer()` as its buffer. `4611C0` appends into this same fixed storage. Its bytes stay at a stable address when the count clears. Bind source wire address `8EC1CC` to those bytes. The producer accepts an append when the combined length is less than 8192.

Bind current shared cells and stores for `67F520`, `50133C`, `501344`, `50135C`, `501360`, and `8E61B4/B8/BC`. Pending local bytes take priority. The local path captures the peer, publishes count, peer, and pointer, then clears the producer count. Single-player receive returns zero when the producer count is zero.

Bind `receive_497790` for the reached network path. It returns the original EAX and updates the shared transport cells before return. A nonzero return publishes the current transport peer, pointer, and count in source order. The function returns the current published pointer after the last store.

Call the real `442AE0` parser when receive returns a nonzero pointer. Keep the producer buffer and published cells shared through parser execution. This module owns the receive operation; it requires the parser and network transport bodies from their live owners.

Run `tools/check_mission_peer_frame_original.py GAME_DIRECTORY PROBE --report REPORT` to compare the real instructions, ordered stores, return values, retained bytes, and mutable shared cells. The transport and post-store mutation providers are explicit test fixtures.
