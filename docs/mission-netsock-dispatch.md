# Netsock packet dispatch

Use `mission_netsock_dispatch` for `100046C0`, `mission_netsock_data_gateway` for `10004420`, address lookup for `10003FE0`, player lookup for `100041D0`, and receive enqueue for `10004220`. Share the current `MissionNetsockPacketMemory`, foreign allocator, and queue owner with the receiver.

Map raw entries with arguments at `+4`. Sizes are hex: dispatch and lookups `C`, gateway `10`, receive wrapper `14`, and nested queue enqueue `1C`. Keep frames current and release them on success and failure. Header comments give each argument offset.

Dispatch reads current `10019C5C`, the packet's WORD opcode, byte map `100047A0`, and jump table `1000477C`. It retains that packet pointer. Known branches read port before IP and call the selected body. Bind `10003F00/100040C0/10004340/10004550/10004660/100029D0/10002180` when reached. Unsupported jump targets cause an explicit error.

The `1080` gateway captures incoming IP and DWORD port. It finds an active address, clears its `+8` cell, finds an active player, and increments current player `+B0` with 32-bit wrap. Current `10019C80` gates delivery. The source then reads current player `+A0`, the current packet argument, and its WORD length. It queues bytes from `packet+8`, with length `WORD-0C` wrapping, sender `player+A0`, port `0`, and priority `1`.

Preserve each lookup's captured count/base and its per-field current reads. Player lookup captures its linked record once before IP and port checks. Keep the real queue allocator, lock/sleep, full-count checks, copy order, and failure prefixes. Apply allocator and memory limits when the reached operation uses them.

`GetMessage` consumes the same canonical queue bytes in FIFO order. Bind the other packet handlers for their own admission, sequence, and retry behavior. Supply DLL startup, buffer and table producers, OS socket services, and worker lifetime through their actual owners.

Run the original check:

```
python tools/check_mission_netsock_dispatch_original.py GAME PROBE --report REPORT
```

Source: `.local/reports/mission-netsock-dispatch-capstone-source.json`. Supported DLL SHA-256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
