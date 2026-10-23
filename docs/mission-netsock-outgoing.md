# Netsock outgoing queue

Use `mission_netsock_outgoing` for `100048C0`, send select for `10003DD0`, direct port send for `10005240`, and compressed port send for `100058D0`. Share current packet memory, allocator, socket, and queues with the receiver and dispatch modules.

Map the drain's 12 current local bytes: DWORD port at `0`, IP at `4`, and length at `8`. The other entries use hex `14` bytes with data, length, IP, and DWORD port at `4/8/C/10`. Nested queue-pop entries use hex `18` bytes. Keep frames current and release them during success and failure.

Drain the actual queue at `10019B20` through `mission_netsock_queue_pop`. Read current `10019C5C` before each pop and each send. Current `10019C78` selects direct or compressed port send. Repeat while the source pop result is nonzero; the drain continues after a leaf returns an error code.

Each port wrapper reads its caller's WORD port, then IP and length. It captures WORD `10019868`, stores the caller port, reads the current data argument, and calls the required leaf. Restore the captured WORD after the leaf returns. A failed required call keeps the installed port and all completed queue changes. Preserve the upper WORD beside the port cell.

Bind actual leaf `10005020` for direct send and `10005860` for compressed send. Keep raw wire transforms, codec calls, buffer producers, socket imports, and OS error handling with their source-backed services. Native fixtures record required leaf calls and use explicit return codes; no OS packet is sent by these tests.

Run the original check:

```
python tools/check_mission_netsock_outgoing_original.py GAME PROBE --report REPORT
```

Evidence: `.local/reports/mission-netsock-outgoing-capstone-source.json`. Supported DLL SHA-256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
