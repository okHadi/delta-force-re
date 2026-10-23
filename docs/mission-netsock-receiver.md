# Netsock receiver

Use `mission_netsock_receiver` for original `100052E0`, `mission_netsock_ip_filter` for `10005280`, and `mission_netsock_receive_poll` for `10005830`.

Bind one current `MissionNetsockPacketMemory` and its shared foreign memory provider. Keep the DLL image at `10000000..1001AFFF`, the current socket at `100096B4`, work buffer at `10019854`, capacity at `1001985C`, and codec flag at `10019860`. Read the CRC table at `10007234` and filter rows at `100076A8` from that same owner.

Map the receiver's `48`-byte working frame. All sizes and offsets in this paragraph are hex. Caller destination, capacity, IP output, port output, and length output are at `34/38/3C/40/44`. Header and counts use `10/14/18`; `recvfrom` uses length at `1C` and sockaddr at `20`. Map each nested filter entry as `8` bytes with IP at `4`. Give the poll body one current four-byte local. Release nested frames during success and failure.

Bind required `recvfrom`, `WSAGetLastError`, `ntohl`, `ntohs`, and `ioctlsocket` calls at `1000595E/10005916/10005958/1000592E/10005934`. Bind the current codec target at `100096AC` when reached. Its arguments are the decoded input, output `10009854`, and capacity `10000`. Keep backend errors explicit.

Keep current bytes available through every read and store. Preserve signed capacity and count checks, 32-bit wrap, the retry transform, DWORD and byte copy order, and two-byte port output. The original CRC copy writes at least one byte even for a nonpositive count. Apply backend memory bounds at the actual access and keep completed writes when a later access fails.

The module performs one source receive or poll call. Supply DLL startup, socket and work-buffer producers, queue workers, thread launch, and OS error conversion through their actual host services.

Run the original check with the supplied game folder and native probe:

```
python tools/check_mission_netsock_receiver_original.py GAME PROBE --report REPORT
```

Source evidence: `.local/reports/netsocknl-udp-receive.json` and `.local/reports/mission-netsock-worker-capstone-source.json`. Supported `Netsocknl.dll` SHA-256: `fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2`.
