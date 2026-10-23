# Netsock wire sends

Use direct encode `10005020`, extended encode `10005120`, and compression select `10005860` with the same current packet-memory owner as outgoing and receive. Keep work buffer `10019854`, capacity `1001985C`, WORD port `10019868`, socket `100096B4`, CRC table `10007234`, and codec cache `10009854` shared.

Map the working frames from the header. Sizes and offsets are hex. Direct uses `30` bytes with sockaddr at `10` and data/length/IP at `24/28/2C`. Extended uses `40` bytes with local header at `18/19`, sockaddr at `1C`, and data/length/IP/flag at `30/34/38/3C`. Compression select uses a `10`-byte entry with data/length/IP at `4/8/C`. Release nested frames on success and failure.

Preserve signed capacity checks after wrapping subtraction, the cached copy count, current source bytes and CRC rows, and 32 low-bit stores. Read the caller length again for the transform choice and send length. Keep direct and extended header stores, current IP reads, and current buffer/socket reads in source order. Backend bounds apply at the actual access and preserve completed stores.

Bind required `htons` at `1000592E`, `sendto` at `10005952`, and `WSAGetLastError` at `10005916`. Preserve the full DWORD passed to `htons`; Winsock consumes its low WORD. Pass the current sockaddr and buffer to `sendto`. A result of `FFFFFFFF` calls last-error; other send results return zero.

Compression select reads current gate `10019860` and codec target `100096A8`. Bind its four arguments: data, cached length, cache `10009854`, and capacity `10000`. The codec returns bits. Preserve signed shift and rounded byte count, the unsigned wrapping size comparison, cached data/length, and the late current IP read. Direct and extended child calls use these source values.

The native test joins real outgoing queues, Huffman code, raw wire capture, receive, dispatch, and `GetMessage` through one current backend. Required socket fixtures capture bytes and return explicit source statuses. OS socket calls, DLL startup, buffer producers, and thread lifetime remain host services.

Run the original check:

```
python tools/check_mission_netsock_send_leaves_original.py GAME PROBE --report REPORT
```

Evidence: `.local/reports/mission-netsock-send-leaves-capstone-source.json`. Supported DLL SHA-256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
