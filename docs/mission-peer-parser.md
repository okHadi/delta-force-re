# Peer packet parser

`mission_peer_parser_parse` implements `442AE0` from the supplied `Df.exe`.

Share the 17 client spans with `MissionClientOutbox`. Each span contains one complete `9B84` record. The parser uses the peer at `8E61BC`, packet address at `8E61B4`, and byte count at `8E61B8`. Keep source spans stable through synchronous callbacks. Resolve `8EC1CC` through `GameMessageStage::peer_frame_buffer()`.

Bind current memory reads and writes with widths 1, 2, and 4. Bind packet addresses to mutable bytes. Keep the same `GameMessageTable` as the outbox. Required handlers execute at their source call site before later reads. A missing reached binding raises an error and retains previous writes.

Reliable packets enqueue message `14` through the real outbox before checking the selected client's 64-word history at offset `9B04`. Duplicate messages retain the ACK and skip their body. The parser owns peer lookup, receive counters, inline state changes, movement publication, actor reply construction, item checks, and source cursor movement.

`MissionPeerParserCall` contains the source call address and arguments. `payload_argument` identifies a pointer argument with a mutable view. Source packet arguments retain their wire address. Stack-local pose, actor reply, and diagnostic views use zero in that argument. Consume these views during the callback. Bind the actual body for each reached address.

Required calls include `493A80` and `4564B0` diagnostics; `47DF40` terrain; `445B80` shot publication; `462F20` projectile creation; and the reached client, weapon, placement, restart, and equipment handlers. The parser requires `447150` for message `28`, `447760` for message `2C`, and `4473F0` for message `2A`. Message `2F` updates its target and timer directly. Message `0A` belongs to the network client update path.

Source memory spans must contain each reached read or write. Actor names fit their 16-byte field. Packet IDs fit the shared descriptor table. The parser raises an error for an unavailable span, an oversized actor name, a negative handler view length, or a source cursor that does not advance. Check these input limits before live use.

Run `tools/check_mission_peer_parser_original.py GAME_DIRECTORY PROBE --report REPORT` for the original instruction comparison. The checker executes the real `441750` ACK body. Foreign handler returns, shared-state mutations, and handler faults are explicit test fixtures.
